/*  This file is part of the KDE project
    Copyright (C) 2006 Tim Beaulen <tbscope@gmail.com>
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
#ifndef Phonon_XINE_ABSTRACTMEDIAPRODUCER_H
#define Phonon_XINE_ABSTRACTMEDIAPRODUCER_H

#include <QObject>
#include <QTime>
#include <QList>
#include "audiopath.h"
#include "videopath.h"
#include <QHash>

#include <xine.h>
#include "xineengine.h"
#include <phonon/mediaproducerinterface.h>
#include <QMultiMap>
#include "xinestream.h"

namespace Phonon
{
namespace Xine
{
	class SeekThread;

	class AbstractMediaProducer : public QObject, public MediaProducerInterface
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::MediaProducerInterface )
		public:
			AbstractMediaProducer( QObject* parent );
			virtual ~AbstractMediaProducer();

			virtual bool addVideoPath( QObject* videoPath );
			virtual bool addAudioPath( QObject* audioPath );
			virtual void removeVideoPath( QObject* videoPath );
			virtual void removeAudioPath( QObject* audioPath );
			virtual State state() const;
			virtual bool hasVideo() const;
			virtual bool isSeekable() const;
			virtual qint64 currentTime() const;
            qint64 totalTime() const;
            qint64 remainingTime() const;
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

			XineStream& stream() { return m_stream; }
			const XineStream& stream() const { return m_stream; }
            void setAudioPort(AudioPort port) { m_stream.setAudioPort(port); }
            void setVideoPort(VideoWidgetInterface *port) { m_stream.setVideoPort(port); }

		public slots:

		Q_SIGNALS:
			void stateChanged( Phonon::State newstate, Phonon::State oldstate );
			void tick( qint64 time );
			void metaDataChanged( const QMultiMap<QString, QString>& );
			void asyncSeek( xine_stream_t*, qint64, bool );

		protected:
			void setState( State );
			void updateMetaData();
            virtual void reachedPlayingState() {}
            virtual void leftPlayingState() {}
			VideoPath* videoPath() const { return m_videoPath; }
			bool outputPortsNotChanged() const;

		protected slots:
            void changeState(Phonon::State);

		private slots:
            void handleStateChange(Phonon::State newstate, Phonon::State oldstate);
            void seekDone();

		private:
			void createStream();

            Phonon::State m_state;
            XineStream m_stream;
            qint32 m_tickInterval;
            AudioPath *m_audioPath;
            VideoPath *m_videoPath;

			QHash<const QObject*, QString> m_selectedAudioStream;
			QHash<const QObject*, QString> m_selectedVideoStream;
			QHash<const QObject*, QString> m_selectedSubtitleStream;

            int m_seeking;
			mutable int m_currentTimeOverride;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_ABSTRACTMEDIAPRODUCER_H
