/*  This file is part of the KDE project
    Copyright (C) 2006 Tim Beaulen <tbscope@gmail.com>

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
#ifndef Phonon_XINE_ABSTRACTMEDIAPRODUCER_H
#define Phonon_XINE_ABSTRACTMEDIAPRODUCER_H

#include <QObject>
#include <phonon/ifaces/abstractmediaproducer.h>
#include <QTime>
#include <QList>
#include "audiopath.h"
#include "videopath.h"
#include <QHash>

#include <xine.h>
#include "xine_engine.h"

class QTimer;

namespace Phonon
{

namespace Ifaces
{
	class VideoPath;
	class AudioPath;
}
namespace Xine
{
	class AbstractMediaProducer : public QObject, virtual public Ifaces::AbstractMediaProducer
	{
		Q_OBJECT
		public:
			AbstractMediaProducer( QObject* parent, XineEngine* xe );
			virtual ~AbstractMediaProducer();
			virtual bool addVideoPath( Ifaces::VideoPath* videoPath );
			virtual bool addAudioPath( Ifaces::AudioPath* audioPath );
			virtual void removeVideoPath( Ifaces::VideoPath* videoPath );
			virtual void removeAudioPath( Ifaces::AudioPath* audioPath );
			virtual State state() const;
			virtual bool hasVideo() const;
			virtual bool seekable() const;
			virtual qint64 currentTime() const;
			virtual qint32 tickInterval() const;

			virtual QStringList availableAudioStreams() const;
			virtual QStringList availableVideoStreams() const;
			virtual QStringList availableSubtitleStreams() const;

			virtual QString selectedAudioStream( const Ifaces::AudioPath* audioPath ) const;
			virtual QString selectedVideoStream( const Ifaces::VideoPath* videoPath ) const;
			virtual QString selectedSubtitleStream( const Ifaces::VideoPath* videoPath ) const;

			virtual void selectAudioStream( const QString& streamName, const Ifaces::AudioPath* audioPath );
			virtual void selectVideoStream( const QString& streamName, const Ifaces::VideoPath* videoPath );
			virtual void selectSubtitleStream( const QString& streamName, const Ifaces::VideoPath* videoPath );

			virtual void setTickInterval( qint32 newTickInterval );
			virtual void play();
			virtual void pause();
			virtual void stop();
			virtual void seek( qint64 time );

		Q_SIGNALS:
			void stateChanged( Phonon::State newstate, Phonon::State oldstate );
			void tick( qint64 time );

		public:
			virtual QObject* qobject() { return this; }
			virtual const QObject* qobject() const { return this; }

		protected:
			void setState( State );

		protected Q_SLOTS:
			virtual void emitTick();

		private:
			XineEngine* m_xine_engine;
			State m_state;
			QTimer* m_tickTimer;
			qint32 m_tickInterval;
			QTime m_startTime, m_pauseTime;
			int m_bufferSize;
			QList<AudioPath*> m_audioPathList;
			QList<VideoPath*> m_videoPathList;

			QHash<const Ifaces::AudioPath*, QString> m_selectedAudioStream;
			QHash<const Ifaces::VideoPath*, QString> m_selectedVideoStream;
			QHash<const Ifaces::VideoPath*, QString> m_selectedSubtitleStream;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_ABSTRACTMEDIAPRODUCER_H
