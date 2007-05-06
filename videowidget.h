/*  This file is part of the KDE project
    Copyright (C) 2005-2007 Matthias Kretz <kretz@kde.org>

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

#ifndef PHONON_XINE_NO_VIDEOWIDGET

#include <QWidget>
#include <phonon/videowidget.h>
#include "abstractvideooutput.h"
#include "videowidget.h"
#include <QPixmap>
#include <xine.h>

#include <xcb/xcb.h>

class QString;
class QMouseEvent;

namespace Phonon
{
namespace Xine
{
    class VideoPath;
	class VideoWidget : public QWidget, public Phonon::Xine::AbstractVideoOutput
	{
		Q_OBJECT
        Q_INTERFACES(Phonon::Xine::AbstractVideoOutput)
		public:
			VideoWidget( QWidget* parent = 0 );
			~VideoWidget();

			Q_INVOKABLE Phonon::VideoWidget::AspectRatio aspectRatio() const;
			Q_INVOKABLE void setAspectRatio( Phonon::VideoWidget::AspectRatio aspectRatio );
            Q_INVOKABLE Phonon::VideoWidget::ScaleMode scaleMode() const;
            Q_INVOKABLE void setScaleMode(Phonon::VideoWidget::ScaleMode mode);

			Q_INVOKABLE QWidget *widget() { return this; }

			Q_INVOKABLE int overlayCapabilities() const;
			Q_INVOKABLE bool createOverlay(QWidget *widget, int type);

			xine_video_port_t* videoPort() const { return m_videoPort; }

			void setPath( VideoPath* vp );
			void unsetPath( VideoPath* vp );

			void xineCallback( int &x, int &y, int &width, int &height,
					double &ratio, int videoWidth, int videoHeight, double videoRatio, bool mayResize );

            bool isValid() const { return videoPort() != 0; }
            void setVideoEmpty(bool);

		signals:
			void videoPortChanged();

		protected:
			virtual void childEvent(QChildEvent *);
            virtual void resizeEvent(QResizeEvent *);
            virtual bool event(QEvent *);
            virtual void mouseMoveEvent(QMouseEvent *);
            virtual void mousePressEvent(QMouseEvent *);
			virtual void showEvent( QShowEvent* );
			virtual void hideEvent( QHideEvent* );
			virtual void paintEvent( QPaintEvent* );
			virtual void changeEvent( QEvent* );
            virtual QSize sizeHint() const { return m_sizeHint; }

		private:
			QWidget *overlay;
            void updateZoom();
			xine_video_port_t* m_videoPort;
            xcb_visual_t m_visual;
			Phonon::VideoWidget::AspectRatio m_aspectRatio;
            Phonon::VideoWidget::ScaleMode m_scaleMode;
			VideoPath* m_path;

            xcb_connection_t *m_xcbConnection;
            QSize m_sizeHint;
			int m_videoWidth;
			int m_videoHeight;
			bool m_fullScreen;
            /**
             * No video should be shown, all paint events should draw black
             */
            bool m_empty;
	};
}} //namespace Phonon::Xine

#endif // PHONON_XINE_NO_VIDEOWIDGET

// vim: sw=4 ts=4 tw=80
#endif // PHONON_XINE_VIDEOWIDGET_H
