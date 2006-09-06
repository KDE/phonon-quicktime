/*  This file is part of the KDE project
    Copyright (C) 2006 Matthias Kretz <kretz@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

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

#include <X11/Xlib.h>

extern Drawable qt_x11Handle( const QPaintDevice *pd );

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
	Q_UNUSED( mayResize );
	/*
	XLockDisplay( m_display );
	Window root;
	int n;
	unsigned int u;

	XGetGeometry( m_display, m_visual.d, &root, &n, &n, reinterpret_cast<unsigned int*>( &width ),
			reinterpret_cast<unsigned int*>( &height ), &u, &u );
	if( !mayResize )
	{
		Window child;
		XTranslateCoordinates( m_display, m_visual.d, root, 0, 0, &x, &y, &child );
	}

	XUnlockDisplay( m_display );
	*/
	x = this->x();
	y = this->y();
	width = this->width();
	height = this->height();

	// square pixels
	ratio = 1.0;

	m_videoWidth = videoWidth;
	m_videoHeight = videoHeight;
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

	// make sure all Qt<->X communication is done
	QApplication::syncX();

	// make a new X connection for xine with X threading enabled
	XInitThreads();
	m_display = XOpenDisplay( NULL );
	if( m_display )
	{
		//XFlush( m_display );

		m_visual.display = m_display;
		m_visual.screen = DefaultScreen( m_display );
		m_visual.d = winId();
		m_visual.user_data = static_cast<void*>( this );
		m_visual.dest_size_cb = Phonon::Xine::dest_size_cb;
		m_visual.frame_output_cb = Phonon::Xine::frame_output_cb;
	}
}

VideoWidget::~VideoWidget()
{
	//xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_WILL_DESTROY_DRAWABLE, 0 );
	// tell all streams to stop using this videoPort
	xine_video_port_t* vp = m_videoPort;
	m_videoPort = 0;
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

Phonon::VideoWidget::AspectRatio VideoWidget::aspectRatio() const
{
	return m_aspectRatio;
}

void VideoWidget::setAspectRatio( Phonon::VideoWidget::AspectRatio aspectRatio )
{
	m_aspectRatio = aspectRatio;
	switch( m_aspectRatio )
	{
		case Phonon::VideoWidget::AspectRatioWidget:
			sizePolicy().setHeightForWidth( false );
			break;
		case Phonon::VideoWidget::AspectRatioAuto:
			sizePolicy().setHeightForWidth( true );
			break;
		case Phonon::VideoWidget::AspectRatioSquare:
			sizePolicy().setHeightForWidth( true );
			break;
		case Phonon::VideoWidget::AspectRatio4_3:
			sizePolicy().setHeightForWidth( true );
			break;
		case Phonon::VideoWidget::AspectRatioAnamorphic:
			sizePolicy().setHeightForWidth( true );
			break;
		case Phonon::VideoWidget::AspectRatioDvb:
			sizePolicy().setHeightForWidth( true );
			break;
	}
}

bool VideoWidget::x11Event( XEvent* event )
{
	switch( event->type )
	{
		case Expose:
			if( event->xexpose.count == 0 && event->xexpose.window == winId() )
			{
				xine_port_send_gui_data( m_videoPort,
						XINE_GUI_SEND_EXPOSE_EVENT, event );
			}
			return true;
			/*
		case MotionNotify:
			{
				XMotionEvent* mev = reinterpret_cast<XMotionEvent*>( event );
				x11_rectangle_t rect = { mev->x, mev->y, 0, 0 };
				if( xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO,
							static_cast<void*>( &rect ) ) == -1 )
				{
					return true;
				}
				if( m_path && m_path->producer() && m_path->producer()->stream() )
				{
					xine_stream_t* stream = m_path->producer()->stream();
					xine_input_data_t data;
					data.x = rect.x;
					data.y = rect.y;
					data.button = 0;
					xine_event_t xine_event =  {
						XINE_EVENT_INPUT_MOUSE_MOVE,
						stream, &data, sizeof( xine_input_data_t ),
						{ 0 , 0 }
					};
					xine_event_send( stream, &xine_event );
					return true;
				}
				break;
			}
			*/
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
	if( !m_videoPort )
	{
		m_videoPort = xine_open_video_driver( XineEngine::xine(), NULL, XINE_VISUAL_TYPE_X11, static_cast<void*>( &m_visual ) );
		emit videoPortChanged();
	}
	//xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_VIDEOWIN_VISIBLE, static_cast<void*>( 1 ) );
}

void VideoWidget::hideEvent( QHideEvent* )
{
	//xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_VIDEOWIN_VISIBLE, static_cast<void*>( 0 ) );
}

void VideoWidget::paintEvent( QPaintEvent* )
{
	kDebug( 610 ) << k_funcinfo << endl;
	if( m_clearWindow )
	{
		m_clearWindow = false;
		QPainter p( this );
		p.fillRect( rect(), Qt::black );
	}
	// paint the area that is not covered by the video
	//QPainter p( this );
	//p.fillRect( rect(), Qt::black );
	/*
	if( m_videoWidth < width() - 1 )
	{
		// fill left and right
		int borderWidth = ( width() - m_videoWidth ) / 2;
		p.fillRect( 0, 0, borderWidth, height(), Qt::black );
		p.fillRect( width() - borderWidth, 0, borderWidth, height(), Qt::black );
	}
	if( m_videoHeight < height() - 1 )
	{
		// fill top and bottom
		int borderHeight = ( height() - m_videoHeight ) / 2;
		p.fillRect( 0, 0, width(), borderHeight, Qt::black );
		p.fillRect( 0, height() - borderHeight, height(), borderHeight, Qt::black );
	}
	*/
}

}} //namespace Phonon::Xine

#include "videowidget.moc"
// vim: sw=4 ts=4 noet
