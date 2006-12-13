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
#ifndef Phonon_XINE_VIDEODATAOUTPUT_H
#define Phonon_XINE_VIDEODATAOUTPUT_H

#include "abstractvideooutput.h"
#include <phonon/videoframe.h>
#include <QVector>
#include <QByteArray>
#include <QObject>
#include <QSize>
#include "videopath.h"
#include <QList>

namespace Phonon
{
namespace Xine
{
	/**
	 * \author Matthias Kretz <kretz@kde.org>
	 */
	class VideoDataOutput : public QObject, public Phonon::Xine::AbstractVideoOutput
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::Xine::AbstractVideoOutput )
		public:
			VideoDataOutput( QObject* parent );
			~VideoDataOutput();

			void addPath( VideoPath* vp ) { m_paths << vp; }
			void removePath( VideoPath* vp ) { m_paths.removeAll( vp ); }

		public slots:
			int frameRate() const;
			void setFrameRate( int frameRate );

			QSize naturalFrameSize() const;
			QSize frameSize() const;
			void setFrameSize( const QSize& frameSize );

			quint32 format() const;
			void setFormat( quint32 fourcc );

		signals:
			void frameReady( const Phonon::VideoFrame& frame );
			void endOfMedia();

		private:
			quint32 m_fourcc;
			int m_frameRate;
			QSize m_frameSize;
			QList<VideoPath*> m_paths;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_VIDEODATAOUTPUT_H
