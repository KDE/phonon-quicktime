/*  This file is part of the KDE project
    Copyright (C) 2005-2006 Matthias Kretz <kretz@kde.org>

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
#ifndef PHONON_XINE_VIDEOWIDGET_H
#define PHONON_XINE_VIDEOWIDGET_H

#include <QWidget>
#include <phonon/ui/videowidget.h>
#include "../abstractvideooutput.h"
#include "../videowidgetinterface.h"
#include <QPixmap>
#include <xine.h>

#include <X11/X.h>
#include <fixx11h.h>

class QString;

namespace Phonon
{
namespace Xine
{
	class VideoWidget : public QWidget, public Phonon::Xine::AbstractVideoOutput, public Phonon::Xine::VideoWidgetInterface
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::Xine::AbstractVideoOutput Phonon::Xine::VideoWidgetInterface )
		public:
			VideoWidget( QWidget* parent = 0 );
			~VideoWidget();

			Q_INVOKABLE Phonon::VideoWidget::AspectRatio aspectRatio() const;
			Q_INVOKABLE void setAspectRatio( Phonon::VideoWidget::AspectRatio aspectRatio );

			Q_INVOKABLE bool isVideoFullScreen() const;
			Q_INVOKABLE void setVideoFullScreen( bool );

			Q_INVOKABLE QWidget *widget() { return this; }

			xine_video_port_t* videoPort() const { return m_videoPort; }

			void setPath( VideoPath* vp );
			void unsetPath( VideoPath* vp );

			void xineCallback( int &x, int &y, int &width, int &height,
					double &ratio, int videoWidth, int videoHeight, double videoRatio, bool mayResize );
			void clearWindow();

		signals:
			void videoPortChanged();

		protected:
			virtual bool x11Event( XEvent* );
			virtual void showEvent( QShowEvent* );
			virtual void hideEvent( QHideEvent* );
			virtual void paintEvent( QPaintEvent* );
			virtual void changeEvent( QEvent* );

		private:
			xine_stream_t* stream() const;

			xine_video_port_t* m_videoPort;
			x11_visual_t m_visual;
			Phonon::VideoWidget::AspectRatio m_aspectRatio;
			VideoPath* m_path;

			Display* m_display;
			Window m_fullScreenWindow;
			int m_videoWidth;
			int m_videoHeight;
			bool m_clearWindow;
			bool m_fullScreen;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // PHONON_XINE_VIDEOWIDGET_H
