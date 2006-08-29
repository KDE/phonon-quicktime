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
#include <X11/X.h>
#include <X11/Xlib.h>

extern Drawable qt_x11Handle( const QPaintDevice *pd );

namespace Phonon
{
namespace Xine
{

extern "C" {
	static void dest_size_cb( void* user_data, int /*video_width*/, int /*video_height*/, double /*video_pixel_aspect*/,
			int *dest_width, int *dest_height, double *dest_pixel_aspect )
	{
		Phonon::Xine::VideoWidget* vw = static_cast<VideoWidget*>( user_data );
		*dest_width = vw->width();
		*dest_height = vw->height();
		*dest_pixel_aspect = 1.0;
	}

	static void frame_output_cb( void* user_data, int /*video_width*/, int /*video_height*/,
			double /*video_pixel_aspect*/, int *dest_x, int *dest_y,
			int *dest_width, int *dest_height,
			double *dest_pixel_aspect, int *win_x, int *win_y )
	{
		Phonon::Xine::VideoWidget* vw = static_cast<VideoWidget*>( user_data );

		//if ( running && firstframe )
		//{
			//firstframe = 0;
			// ?
		//}

		*dest_x            = 0;
		*dest_y            = 0;
		*win_x             = vw->x();
		*win_y             = vw->y();
		*dest_width        = vw->width();
		*dest_height       = vw->height();
		*dest_pixel_aspect = 1.0;
	}
}

VideoWidget::VideoWidget( QWidget* parent )
	: QWidget( parent )
{
	Display* display = QX11Info::display();

	XLockDisplay( display );
	Window xwindow = XCreateSimpleWindow( display, QX11Info::appRootWindow( x11Info().screen() ),
			x(), y(), width(), height(), 1, 0, 0 );
	XSelectInput( display, xwindow, ExposureMask );
	XMapRaised( display, xwindow );
	XSync( display, xwindow );
	XUnlockDisplay( display );

	m_visual.display = display;
	m_visual.screen = x11Info().screen();
	m_visual.d = xwindow;
	m_visual.user_data = static_cast<void*>( this );
	m_visual.dest_size_cb = Phonon::Xine::dest_size_cb;
	m_visual.frame_output_cb = Phonon::Xine::frame_output_cb;
	m_videoPort = xine_open_video_driver( XineEngine::xine(), NULL, XINE_VISUAL_TYPE_X11, static_cast<void*>( &m_visual ) );

	QPalette p = palette();
	p.setColor( QPalette::Window, Qt::blue );
	setPalette( p );
	setBackgroundRole( QPalette::Window );
	setAutoFillBackground( true );
	setMinimumSize( 100, 100 );
}

Phonon::VideoWidget::AspectRatio VideoWidget::aspectRatio() const
{
	return m_aspectRatio;
}

void VideoWidget::setAspectRatio( Phonon::VideoWidget::AspectRatio aspectRatio )
{
	m_aspectRatio = aspectRatio;
}

}} //namespace Phonon::Xine

#include "videowidget.moc"
// vim: sw=4 ts=4 noet
