/*  This file is part of the KDE project
    Copyright (C) 2006-2007 Matthias Kretz <kretz@kde.org>

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
#include "events.h"
#include <QPalette>
#include <QImage>
#include <QPainter>
#include <kdebug.h>

#ifndef PHONON_XINE_NO_VIDEOWIDGET
#include <QX11Info>
#endif
#include "xineengine.h"
#include "mediaobject.h"
#include <QApplication>
#include <QVBoxLayout>

#include <QDesktopWidget>
#include <QMouseEvent>

namespace Phonon
{
namespace Xine
{

#ifndef PHONON_XINE_NO_VIDEOWIDGET
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
#endif // PHONON_XINE_NO_VIDEOWIDGET

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
    : QWidget(parent),
    m_videoPort(0),
    m_fullScreen(false),
    m_empty(true)
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

#ifndef PHONON_XINE_NO_VIDEOWIDGET
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
#endif // PHONON_XINE_NO_VIDEOWIDGET
            kError(610) << "No xine video output plugin using libxcb for threadsafe access to the X server found. No video for you." << endl;
#ifndef PHONON_XINE_NO_VIDEOWIDGET
        }
    }
#endif // PHONON_XINE_NO_VIDEOWIDGET
}

VideoWidget::~VideoWidget()
{
    if (m_videoPort) {
        xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_WILL_DESTROY_DRAWABLE, 0);

        xine_video_port_t *vp = m_videoPort;
        m_videoPort = 0;
        // tell the xine stream to stop using this videoPort
        //if( m_path && m_path->mediaObject() )
        //emit videoPortChanged();
//X         FIXME:
//X         if (m_path && m_path->mediaObject()) {
//X             XineStream &xs = m_path->mediaObject()->stream();
//X             xs.aboutToDeleteVideoWidget();
//X         }

        xine_close_video_driver(XineEngine::xine(), vp);
    }

#ifndef PHONON_XINE_NO_VIDEOWIDGET
    xcb_disconnect(m_xcbConnection);
    m_xcbConnection = 0;
#endif // PHONON_XINE_NO_VIDEOWIDGET
}

Phonon::VideoWidget::AspectRatio VideoWidget::aspectRatio() const
{
	return m_aspectRatio;
}

void VideoWidget::setAspectRatio( Phonon::VideoWidget::AspectRatio aspectRatio )
{
    m_aspectRatio = aspectRatio;
//X     FIXME
//X     if (m_path && m_path->mediaObject()) {
//X         XineStream &xs = m_path->mediaObject()->stream();
//X         switch (m_aspectRatio) {
//X             case Phonon::VideoWidget::AspectRatioWidget:
//X                 xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_SQUARE);
//X                 break;
//X             case Phonon::VideoWidget::AspectRatioAuto:
//X                 xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_AUTO);
//X                 break;
//X //X             case Phonon::VideoWidget::AspectRatioSquare:
//X //X                 xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_SQUARE);
//X //X                 break;
//X             case Phonon::VideoWidget::AspectRatio4_3:
//X                 xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_4_3);
//X                 break;
//X             case Phonon::VideoWidget::AspectRatio16_9:
//X                 xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_ANAMORPHIC);
//X                 break;
//X //X             case Phonon::VideoWidget::AspectRatioDvb:
//X //X                 xs.setParam(XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_DVB);
//X //X                 break;
//X         }
//X         updateZoom();
//X     }
}

Phonon::VideoWidget::ScaleMode VideoWidget::scaleMode() const
{
    return m_scaleMode;
}

void VideoWidget::setScaleMode(Phonon::VideoWidget::ScaleMode mode)
{
    m_scaleMode = mode;
    updateZoom();
}

/*
int VideoWidget::overlayCapabilities() const
{
	return Phonon::Experimental::OverlayApi::OverlayOpaque;
}

bool VideoWidget::createOverlay(QWidget *widget, int type)
{
	if ((overlay != 0) || (type != Phonon::Experimental::OverlayApi::OverlayOpaque))
		return false;

	if (layout() == 0) {
		QLayout *layout = new QVBoxLayout(this);
		layout->setMargin(0);
		setLayout(layout);
	}

	layout()->addWidget(widget);
	overlay = widget;

	return true;
}

void VideoWidget::childEvent(QChildEvent *event)
{
	if (event->removed() && (event->child() == overlay))
		overlay = 0;
	QWidget::childEvent(event);
}
*/

void VideoWidget::updateZoom()
{
//X     FIXME
//X     if (m_path && m_path->mediaObject()) {
//X         XineStream &xs = m_path->mediaObject()->stream();
//X         if (m_aspectRatio == Phonon::VideoWidget::AspectRatioWidget) {
//X             const QSize s = size();
//X             QSize imageSize = m_sizeHint;
//X             imageSize.scale(s, Qt::KeepAspectRatio);
//X             if (imageSize.width() < s.width()) {
//X                 const int zoom = s.width() * 100 / imageSize.width();
//X                 xs.setParam(XINE_PARAM_VO_ZOOM_X, zoom);
//X                 xs.setParam(XINE_PARAM_VO_ZOOM_Y, 100);
//X             } else {
//X                 const int zoom = s.height() * 100 / imageSize.height();
//X                 xs.setParam(XINE_PARAM_VO_ZOOM_X, 100);
//X                 xs.setParam(XINE_PARAM_VO_ZOOM_Y, zoom);
//X             }
//X         } else if (m_scaleMode == Phonon::VideoWidget::ScaleAndCrop) {
//X             const QSize s = size();
//X             QSize imageSize = m_sizeHint;
//X             // the image size is in square pixels
//X             // first transform it to the current aspect ratio
//X             kDebug(610) << imageSize << endl;
//X             switch (m_aspectRatio) {
//X                 case Phonon::VideoWidget::AspectRatioAuto:
//X                     // FIXME: how can we find out the ratio xine decided on? the event?
//X                     break;
//X                 case Phonon::VideoWidget::AspectRatio4_3:
//X                     imageSize.setWidth(imageSize.height() * 4 / 3);
//X                     break;
//X                 case Phonon::VideoWidget::AspectRatio16_9:
//X                     imageSize.setWidth(imageSize.height() * 16 / 9);
//X                     break;
//X //X                 case Phonon::VideoWidget::AspectRatioDvb:
//X //X                     imageSize.setWidth(imageSize.height() * 2);
//X //X                     break;
//X                 default:
//X                     // correct ratio already
//X                     break;
//X             }
//X             kDebug(610) << imageSize << endl;
//X             imageSize.scale(s, Qt::KeepAspectRatioByExpanding);
//X             kDebug(610) << imageSize << s << endl;
//X             int zoom;
//X             if (imageSize.width() > s.width()) {
//X                 zoom = imageSize.width() * 100 / s.width();
//X             } else {
//X                 zoom = imageSize.height() * 100 / s.height();
//X             }
//X             xs.setParam(XINE_PARAM_VO_ZOOM_X, zoom);
//X             xs.setParam(XINE_PARAM_VO_ZOOM_Y, zoom);
//X         } else {
//X             xs.setParam(XINE_PARAM_VO_ZOOM_X, 100);
//X             xs.setParam(XINE_PARAM_VO_ZOOM_Y, 100);
//X         }
//X     }
}

void VideoWidget::resizeEvent(QResizeEvent *ev)
{
    updateZoom();
    QWidget::resizeEvent(ev);
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
                kDebug(610) << k_funcinfo << "XineFrameFormatChangeEvent " << e->size << endl;
                m_sizeHint = e->size;
                updateGeometry();
            }
            return true;
        default:
            return QWidget::event(ev);
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent *mev)
{
//X     FIXME
//X     if (m_path && m_path->mediaObject()) {
//X         XineStream &xs = m_path->mediaObject()->stream();
//X         if (cursor().shape() == Qt::BlankCursor) {
//X             setCursor(QCursor(Qt::ArrowCursor));
//X         }
//X 
//X         x11_rectangle_t   rect;
//X         xine_event_t      *event = new xine_event_t;
//X         xine_input_data_t *input = new xine_input_data_t;
//X 
//X         rect.x = mev->x();
//X         rect.y = mev->y();
//X         rect.w = 0;
//X         rect.h = 0;
//X 
//X         xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect);
//X 
//X         event->type        = XINE_EVENT_INPUT_MOUSE_MOVE;
//X         event->data        = input;
//X         event->data_length = sizeof(*input);
//X         input->button      = 0;
//X         input->x           = rect.x;
//X         input->y           = rect.y;
//X         xs.eventSend(event);
//X     }
    QWidget::mouseMoveEvent(mev);
}

void VideoWidget::mousePressEvent(QMouseEvent *mev)
{
//X     FIXME
//X     if (mev->button() == Qt::LeftButton && m_path && m_path->mediaObject()) {
//X         XineStream &xs = m_path->mediaObject()->stream();
//X         x11_rectangle_t   rect;
//X         xine_event_t      *event = new xine_event_t;
//X         xine_input_data_t *input = new xine_input_data_t;
//X 
//X         rect.x = mev->x();
//X         rect.y = mev->y();
//X         rect.w = 0;
//X         rect.h = 0;
//X 
//X         xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect);
//X 
//X         event->type        = XINE_EVENT_INPUT_MOUSE_BUTTON;
//X         event->data        = input;
//X         event->data_length = sizeof(*input);
//X         input->button      = 1;
//X         input->x           = rect.x;
//X         input->y           = rect.y;
//X         xs.eventSend(event);
//X     }
    QWidget::mousePressEvent(mev);
}

void VideoWidget::setVideoEmpty(bool b)
{
    m_empty = b;
    if (b) {
        update();
    }
}

void VideoWidget::paintEvent(QPaintEvent *event)
{
    //kDebug(610) << k_funcinfo << endl;
//X     FIXME
//X     if (m_empty || !m_path || !m_path->mediaObject() || m_path->mediaObject()->state() == Phonon::LoadingState) {
//X         QPainter p(this);
//X         p.fillRect(rect(), Qt::black);
//X #ifndef PHONON_XINE_NO_VIDEOWIDGET
//X     } else if (m_videoPort) {
//X         const QRect &rect = event->rect();
//X 
//X         xcb_expose_event_t xcb_event;
//X         memset(&xcb_event, 0, sizeof(xcb_event));
//X 
//X         xcb_event.window = winId();
//X         xcb_event.x = rect.x();
//X         xcb_event.y = rect.y();
//X         xcb_event.width = rect.width();
//X         xcb_event.height = rect.height();
//X         xcb_event.count = 0;
//X 
//X         xine_port_send_gui_data(m_videoPort, XINE_GUI_SEND_EXPOSE_EVENT, &xcb_event);
//X #endif // PHONON_XINE_NO_VIDEOWIDGET
//X     } else {
//X         QPainter p(this);
//X         p.fillRect(rect(), Qt::black);
//X     }
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
#ifndef PHONON_XINE_NO_VIDEOWIDGET
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
#endif // PHONON_XINE_NO_VIDEOWIDGET
    }
}

}} //namespace Phonon::Xine

#include "videowidget.moc"
// vim: sw=4 ts=4
