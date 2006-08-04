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
#include <QTime>
#include <QList>
#include "audiopath.h"
#include "videopath.h"
#include <QHash>

#include <xine.h>
#include "xine_engine.h"
#include <phonon/mediaproducerinterface.h>

class QTimer;

namespace Phonon
{

namespace Xine
{
	class AbstractMediaProducer : public QObject, public MediaProducerInterface
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::MediaProducerInterface )
		public:
			AbstractMediaProducer( QObject* parent, XineEngine* xe );
			virtual ~AbstractMediaProducer();

			virtual bool addVideoPath( QObject* videoPath );
			virtual bool addAudioPath( QObject* audioPath );
			virtual void removeVideoPath( QObject* videoPath );
			virtual void removeAudioPath( QObject* audioPath );
			virtual State state() const;
			virtual bool hasVideo() const;
			virtual bool isSeekable() const;
			virtual qint64 currentTime() const;
			virtual qint32 tickInterval() const;

			virtual QStringList availableAudioStreams() const;
			virtual QStringList availableVideoStreams() const;
			virtual QStringList availableSubtitleStreams() const;

			virtual QString selectedAudioStream( const QObject* audioPath ) const;
			virtual QString selectedVideoStream( const QObject* videoPath ) const;
			virtual QString selectedSubtitleStream( const QObject* videoPath ) const;

			virtual void selectAudioStream( const QString& streamName, const QObject* audioPath );
			virtual void selectVideoStream( const QString& streamName, const QObject* videoPath );
			virtual void selectSubtitleStream( const QString& streamName, const QObject* videoPath );

			virtual void setTickInterval( qint32 newTickInterval );
			virtual void play();
			virtual void pause();
			virtual void stop();
			virtual void seek( qint64 time );

		Q_SIGNALS:
			void stateChanged( Phonon::State newstate, Phonon::State oldstate );
			void tick( qint64 time );

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

			QHash<const QObject*, QString> m_selectedAudioStream;
			QHash<const QObject*, QString> m_selectedVideoStream;
			QHash<const QObject*, QString> m_selectedSubtitleStream;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_ABSTRACTMEDIAPRODUCER_H
