/*  This file is part of the KDE project
    Copyright (C) 2006 Matthias Kretz <kretz@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.

*/

#include "videowidget.h"
#include <QPalette>
#include <QImage>
#include <QPainter>
#include <kdebug.h>

#include <QX11Info>
#include "../xine_engine.h"
#include "../abstractmediaproducer.h"
#include <QApplication>
#include <QMouseEvent>

#include <X11/Xlib.h>

namespace Phonon
{
namespace Xine
{

static void dest_size_cb( void* user_data, int video_width, int video_height, double video_pixel_aspect,
		int *dest_width, int *dest_height, double *dest_pixel_aspect )
{
	Phonon::Xine::VideoWidget* vw = static_cast<VideoWidget*>( user_data );
	int win_x, win_y;

	vw->xineCallback( win_x, win_y, *dest_width, *dest_height, *dest_pixel_aspect, video_width,
			video_height, video_pixel_aspect, false );
}

static void frame_output_cb( void* user_data, int video_width, int video_height,
		double video_pixel_aspect, int *dest_x, int *dest_y,
		int *dest_width, int *dest_height,
		double *dest_pixel_aspect, int *win_x, int *win_y )
{
	Phonon::Xine::VideoWidget* vw = static_cast<VideoWidget*>( user_data );

	vw->xineCallback( *win_x, *win_y, *dest_width, *dest_height, *dest_pixel_aspect, video_width,
			video_height, video_pixel_aspect, true );

	*dest_x            = 0;
	*dest_y            = 0;
}


void VideoWidget::xineCallback( int &x, int &y, int &width, int &height, double &ratio,
		int videoWidth, int videoHeight, double videoRatio, bool mayResize )
{
	Q_UNUSED( videoRatio );
	Q_UNUSED( videoWidth );
	Q_UNUSED( videoHeight );
	Q_UNUSED( mayResize );

	x = this->x();
	y = this->y();
	width = this->width();
	height = this->height();

	// square pixels
	ratio = 1.0;
}

VideoWidget::VideoWidget( QWidget* parent )
	: QWidget( parent )
	, m_videoPort( 0 )
	, m_path( 0 )
	, m_clearWindow( false )
{
	// for some reason it can hang if the widget is 0x0
	setMinimumSize( 1, 1 );

	QPalette palette = this->palette();
	palette.setColor( backgroundRole(), Qt::black );
	setPalette( palette );

	// when resizing fill with black (backgroundRole color) the rest is done by paintEvent
	setAttribute( Qt::WA_OpaquePaintEvent, true );

	// disable Qt composition management as Xine draws onto the widget directly using X calls
	setAttribute( Qt::WA_PaintOnScreen, true );

	// required for dvdnav
	setMouseTracking(true);

	// make a new X connection for xine
	// ~QApplication hangs (or crashes) in XCloseDisplay called from Qt when XInitThreads is
	// called. Without it everything is fine, except of course xine rendering onto the window from
	// multiple threads. So Phonon-Xine will use its own xine vo plugins not using X(Un)LockDisplay.
	m_display = XOpenDisplay( DisplayString( x11Info().display() ) );
	if( m_display )
	{
		m_visual.display = m_display;
		m_visual.screen = DefaultScreen( m_display );
		m_visual.user_data = static_cast<void*>( this );
		m_visual.dest_size_cb = Phonon::Xine::dest_size_cb;
		m_visual.frame_output_cb = Phonon::Xine::frame_output_cb;
		m_visual.d = winId();

		// make sure all Qt<->X communication is done, else xine_open_video_driver will hang
		QApplication::syncX();

		Q_ASSERT( testAttribute( Qt::WA_WState_Created ) );
		m_videoPort = xine_open_video_driver( XineEngine::xine(), "xvnt", XINE_VISUAL_TYPE_X11, static_cast<void*>( &m_visual ) );
		if( !m_videoPort )
		{
			kWarning( 610 ) << "No xine video output plugin without XThreads found. Expect to see X errors." << endl;
			m_videoPort = xine_open_video_driver( XineEngine::xine(), 0, XINE_VISUAL_TYPE_X11, static_cast<void*>( &m_visual ) );
		}
	}
}

VideoWidget::~VideoWidget()
{
	//xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_WILL_DESTROY_DRAWABLE, 0 );

	xine_video_port_t* vp = m_videoPort;
	m_videoPort = 0;
	// tell the xine stream to stop using this videoPort
	if( m_path && m_path->producer() )
		emit videoPortChanged();

	xine_close_video_driver( XineEngine::xine(), vp );

	XCloseDisplay( m_display );
	m_display = 0;
}

void VideoWidget::setPath( VideoPath* vp )
{
	Q_ASSERT( m_path == 0 );
	m_path = vp;
}

void VideoWidget::unsetPath( VideoPath* vp )
{
	Q_ASSERT( m_path == vp );
	m_path = 0;
}

void VideoWidget::setNavCursor( bool b )
{
	if ( b )
		setCursor( QCursor(Qt::PointingHandCursor) );
	else
		setCursor( QCursor(Qt::ArrowCursor) );
}

void VideoWidget::mouseMoveEvent( QMouseEvent* mev )
{
	if( m_path && m_path->producer() && m_path->producer()->stream() ) {
		xine_stream_t* stream = m_path->producer()->stream();
		if ( cursor().shape() == Qt::BlankCursor )
			setCursor( QCursor(Qt::ArrowCursor) );

		x11_rectangle_t   rect;
		xine_event_t      event;
		xine_input_data_t input;

		rect.x = mev->x();
		rect.y = mev->y();
		rect.w = 0;
		rect.h = 0;

		xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect );

		event.type        = XINE_EVENT_INPUT_MOUSE_MOVE;
		event.data        = &input;
		event.data_length = sizeof(input);
		input.button      = 0;
		input.x           = rect.x;
		input.y           = rect.y;
		xine_event_send( stream, &event );
		mev->ignore(); // forward to parent
	}
}


void VideoWidget::mousePressEvent( QMouseEvent* mev )
{
	if ( mev->button() == Qt::LeftButton ) {
		if( m_path && m_path->producer() && m_path->producer()->stream() ) {
			xine_stream_t* stream = m_path->producer()->stream();
			x11_rectangle_t   rect;
			xine_event_t      event;
			xine_input_data_t input;

			rect.x = mev->x();
			rect.y = mev->y();
			rect.w = 0;
			rect.h = 0;

			xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect );

			event.type        = XINE_EVENT_INPUT_MOUSE_BUTTON;
			event.data        = &input;
			event.data_length = sizeof(input);
			input.button      = 1;
			input.x           = rect.x;
			input.y           = rect.y;
			xine_event_send( stream, &event );
			mev->accept(); /* don't send event to parent */
		}
	}
}

Phonon::VideoWidget::AspectRatio VideoWidget::aspectRatio() const
{
	return m_aspectRatio;
}

void VideoWidget::setAspectRatio( Phonon::VideoWidget::AspectRatio aspectRatio )
{
	m_aspectRatio = aspectRatio;
	if( m_path && m_path->producer() && m_path->producer()->stream() )
	{
		xine_stream_t* stream = m_path->producer()->stream();
		switch( m_aspectRatio )
		{
			case Phonon::VideoWidget::AspectRatioWidget:
				xine_set_param( stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_NUM_RATIOS );
				break;
			case Phonon::VideoWidget::AspectRatioAuto:
				xine_set_param( stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_AUTO );
				break;
			case Phonon::VideoWidget::AspectRatioSquare:
				xine_set_param( stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_SQUARE );
				break;
			case Phonon::VideoWidget::AspectRatio4_3:
				xine_set_param( stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_4_3 );
				break;
			case Phonon::VideoWidget::AspectRatioAnamorphic:
				xine_set_param( stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_ANAMORPHIC );
				break;
			case Phonon::VideoWidget::AspectRatioDvb:
				xine_set_param( stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_DVB );
				break;
		}
	}
}

bool VideoWidget::x11Event( XEvent* event )
{
	switch( event->type )
	{
		case Expose:
			if( event->xexpose.count == 0 && event->xexpose.window == winId() )
			{
				//kDebug( 610 ) << "XINE_GUI_SEND_EXPOSE_EVENT" << endl;
				xine_port_send_gui_data( m_videoPort,
						XINE_GUI_SEND_EXPOSE_EVENT, event );
			}
			return true;
	}
	return false;
}

void VideoWidget::clearWindow()
{
	m_clearWindow = true;
	repaint();
}

void VideoWidget::showEvent( QShowEvent* )
{
	//xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_VIDEOWIN_VISIBLE, static_cast<void*>( 1 ) );
}

void VideoWidget::hideEvent( QHideEvent* )
{
	//xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_VIDEOWIN_VISIBLE, static_cast<void*>( 0 ) );
}

void VideoWidget::paintEvent( QPaintEvent* )
{
	//kDebug( 610 ) << k_funcinfo << endl;
	if( m_clearWindow )
	{
		m_clearWindow = false;
		QPainter p( this );
		p.fillRect( rect(), Qt::black );
	}
}

void VideoWidget::changeEvent( QEvent* event )
{
	if( event->type() == QEvent::ParentAboutToChange )
	{
	}
	else if( event->type() == QEvent::ParentChange )
	{
		m_visual.d = winId();
		if( m_videoPort )
			xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_DRAWABLE_CHANGED, reinterpret_cast<void*>( m_visual.d ) );
	}
}

}} //namespace Phonon::Xine

#include "videowidget.moc"
// vim: sw=4 ts=4 noet
