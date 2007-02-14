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
#include "../xineengine.h"
#include "../abstractmediaproducer.h"
#include <QApplication>

#include <QDesktopWidget>
#include <QMouseEvent>

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
	, m_fullScreen( false )
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
    int preferredScreen = 0;
    m_xcbConnection = xcb_connect(NULL, &preferredScreen);//DisplayString(x11Info().display()), NULL);
    if (m_xcbConnection) {
        m_visual.connection = m_xcbConnection;
        xcb_screen_iterator_t screenIt = xcb_setup_roots_iterator(xcb_get_setup(m_xcbConnection));
        while ((screenIt.rem > 1) && (preferredScreen > 0)) {
            xcb_screen_next(&screenIt);
            --preferredScreen;
        }
        m_visual.screen = screenIt.data;
        m_visual.window = winId();
        m_visual.user_data = static_cast<void*>(this);
        m_visual.dest_size_cb = Phonon::Xine::dest_size_cb;
        m_visual.frame_output_cb = Phonon::Xine::frame_output_cb;

        // make sure all Qt<->X communication is done, else xine_open_video_driver will crash
        QApplication::syncX();

        Q_ASSERT(testAttribute(Qt::WA_WState_Created));
        m_videoPort = xine_open_video_driver(XineEngine::xine(), "auto", XINE_VISUAL_TYPE_XCB, static_cast<void*>(&m_visual));
        if (!m_videoPort) {
            kError(610) << "No xine video output plugin using libxcb for threadsafe access to the X server found. No video for you." << endl;
        }
    }
}

VideoWidget::~VideoWidget()
{
    xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_WILL_DESTROY_DRAWABLE, 0);

	xine_video_port_t* vp = m_videoPort;
	m_videoPort = 0;
	// tell the xine stream to stop using this videoPort
	//if( m_path && m_path->producer() )
		//emit videoPortChanged();
    if (m_path && m_path->producer()) {
        XineStream &xs = m_path->producer()->stream();
        xs.aboutToDeleteVideoWidget();
    }

	xine_close_video_driver( XineEngine::xine(), vp );

    xcb_disconnect(m_xcbConnection);
    m_xcbConnection = 0;
}

void VideoWidget::setPath( VideoPath* vp )
{
    Q_ASSERT(m_path == 0);
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
    if (m_path && m_path->producer()) {
        XineStream &xs = m_path->producer()->stream();
        switch (m_aspectRatio) {
            case Phonon::VideoWidget::AspectRatioWidget:
                xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_NUM_RATIOS);
                break;
            case Phonon::VideoWidget::AspectRatioAuto:
                xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_AUTO);
                break;
            case Phonon::VideoWidget::AspectRatioSquare:
                xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_SQUARE);
                break;
            case Phonon::VideoWidget::AspectRatio4_3:
                xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_4_3);
                break;
            case Phonon::VideoWidget::AspectRatioAnamorphic:
                xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_ANAMORPHIC);
                break;
            case Phonon::VideoWidget::AspectRatioDvb:
                xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_DVB);
                break;
        }
    }
}

void VideoWidget::setZoomX( int percent )
{
	if (m_path && m_path->producer()) {
        XineStream &xs = m_path->producer()->stream();
        xs.setParam(XINE_PARAM_VO_ZOOM_X, percent);
	}
}

void VideoWidget::setZoomY( int percent )
{
	if (m_path && m_path->producer()) {
        XineStream &xs = m_path->producer()->stream();
        xs.setParam(XINE_PARAM_VO_ZOOM_Y, percent);
	}
}

bool VideoWidget::event(QEvent *ev)
{
    switch (ev->type()) {
        case Xine::NavButtonInEvent:
            setCursor(QCursor(Qt::PointingHandCursor));
            ev->accept();
            return true;
        case Xine::NavButtonOutEvent:
            unsetCursor();
            ev->accept();
            return true;
        case Xine::FrameFormatChangeEvent:
            ev->accept();
            {
                XineFrameFormatChangeEvent *e = static_cast<XineFrameFormatChangeEvent *>(ev);
                kDebug(610) << k_funcinfo << "XineFrameFormatChangeEvent " << e->size() << endl;
                m_sizeHint = e->size();
                //updateGeometry();
            }
            return true;
        default:
            return QWidget::event(ev);
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent *mev)
{
    if (m_path && m_path->producer()) {
        XineStream &xs = m_path->producer()->stream();
        if (cursor().shape() == Qt::BlankCursor) {
            setCursor(QCursor(Qt::ArrowCursor));
        }

        x11_rectangle_t   rect;
        xine_event_t      *event = new xine_event_t;
        xine_input_data_t *input = new xine_input_data_t;

        rect.x = mev->x();
        rect.y = mev->y();
        rect.w = 0;
        rect.h = 0;

        xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect);

        event->type        = XINE_EVENT_INPUT_MOUSE_MOVE;
        event->data        = input;
        event->data_length = sizeof(*input);
        input->button      = 0;
        input->x           = rect.x;
        input->y           = rect.y;
        xs.eventSend(event);
    }
    QWidget::mouseMoveEvent(mev);
}

void VideoWidget::mousePressEvent(QMouseEvent *mev)
{
    if (mev->button() == Qt::LeftButton && m_path && m_path->producer()) {
        XineStream &xs = m_path->producer()->stream();
        x11_rectangle_t   rect;
        xine_event_t      *event = new xine_event_t;
        xine_input_data_t *input = new xine_input_data_t;

        rect.x = mev->x();
        rect.y = mev->y();
        rect.w = 0;
        rect.h = 0;

        xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect);

        event->type        = XINE_EVENT_INPUT_MOUSE_BUTTON;
        event->data        = input;
        event->data_length = sizeof(*input);
        input->button      = 1;
        input->x           = rect.x;
        input->y           = rect.y;
        xs.eventSend(event);
    }
    QWidget::mousePressEvent(mev);
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    //kDebug(610) << k_funcinfo << endl;
    if (!m_path || !m_path->producer() || m_path->producer()->state() == Phonon::LoadingState) {
        QPainter p(this);
        p.fillRect(rect(), Qt::black);
    } else if (m_videoPort) {
        const QRect &rect = event->rect();

        xcb_expose_event_t xcb_event;
        memset(&xcb_event, 0, sizeof(xcb_event));

        xcb_event.window = winId();
        xcb_event.x = rect.x();
        xcb_event.y = rect.y();
        xcb_event.width = rect.width();
        xcb_event.height = rect.height();
        xcb_event.count = 0;

        xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_EXPOSE_EVENT, &xcb_event);
    } else {
        QPainter p(this);
        p.fillRect(rect(), Qt::black);
    }
    QWidget::paintEvent(event);
}

void VideoWidget::showEvent( QShowEvent* )
{
	//xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_VIDEOWIN_VISIBLE, static_cast<void*>( 1 ) );
}

void VideoWidget::hideEvent( QHideEvent* )
{
	//xine_port_send_gui_data( m_videoPort, XINE_GUI_SEND_VIDEOWIN_VISIBLE, static_cast<void*>( 0 ) );
}

void VideoWidget::changeEvent( QEvent* event )
{
	if( event->type() == QEvent::ParentAboutToChange )
	{
		kDebug( 610 ) << k_funcinfo << "ParentAboutToChange" << endl;
	}
	else if( event->type() == QEvent::ParentChange )
	{
        kDebug(610) << k_funcinfo << "ParentChange" << winId() << endl;
        if (m_visual.window != winId()) {
            m_visual.window = winId();
            if (m_videoPort) {
                // make sure all Qt<->X communication is done, else winId() might not be known at the
                // X-server yet
                QApplication::syncX();
                xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_DRAWABLE_CHANGED, reinterpret_cast<void*>(m_visual.window));
                kDebug(610) << "XINE_GUI_SEND_DRAWABLE_CHANGED done." << endl;
            }
        }
    }
}

}} //namespace Phonon::Xine

#include "videowidget.moc"
// vim: sw=4 ts=4
