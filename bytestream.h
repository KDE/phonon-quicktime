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
#ifndef Phonon_XINE_BYTESTREAM_H
#define Phonon_XINE_BYTESTREAM_H

#include "abstractmediaproducer.h"

#include <xine.h>

#include "xine_engine.h"
#include <phonon/bytestreaminterface.h>
#include <QByteArray>
#include <QQueue>
#include <kdebug.h>
#include <QCoreApplication>
#include <QMutex>
#include <QWaitCondition>
#include <pthread.h>
#include <cstdlib>

class QTimer;

namespace Phonon
{
namespace Xine
{
	class ByteStream : public AbstractMediaProducer, public ByteStreamInterface
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::ByteStreamInterface )
		public:
			ByteStream( QObject* parent );
			~ByteStream();

		public slots:
			qint64 totalTime() const;
			bool isSeekable() const;

			void writeData( const QByteArray& data );

			void play();
			void pause();
			void stop();
			void seek( qint64 time );

			void endOfData();

			void setStreamSeekable( bool );
			bool streamSeekable() const;

			void setStreamSize( qint64 );
			qint64 streamSize() const;

			void setAboutToFinishTime( qint32 newAboutToFinishTime );
			qint32 aboutToFinishTime() const;

		Q_SIGNALS:
			void finished();
			void aboutToFinish( qint32 msec );
			void length( qint64 length );
			void needData();
			void needDataQueued();
			void enoughData();
			void seekStream( qint64 );
			void seekStreamQueued( qint64 );

		protected:
			virtual void emitTick();
			virtual bool recreateStream();
			virtual void reachedPlayingState();
			virtual void leftPlayingState();
			virtual void stateTransition( int newState );
            void syncSeekStream(qint64 offset);
            bool canRecreateStream() const;

			bool m_aboutToFinishNotEmitted;

		private slots:
			void emitAboutToFinish();
			void slotSeekStream( qint64 );
			bool xineOpen();

		private:
            enum State
            {
                CreatedState = 1,
                PreviewReadyState = 2,
                StreamSizeSetState = 4,
                AboutToOpenState = CreatedState | PreviewReadyState | StreamSizeSetState,
                OpenedState = 8,
                PlayingState = 16,
                StreamAtEndState = 32
            };

			void emitAboutToFinishIn( int timeToAboutToFinishSignal );

			bool m_seekable;
			qint32 m_aboutToFinishTime;
			QTimer *m_aboutToFinishTimer;
			qint64 m_streamSize;

		QByteArray m_preview;
		int m_intstate;
		QMutex m_mutex;
		QMutex m_seekMutex;
		QWaitCondition m_waitForDataCondition;
		QWaitCondition m_seekWaitCondition;

		size_t m_buffersize;
		int m_offset;
		QQueue<QByteArray> m_buffers;
		pthread_t m_mainThread;
		qint64 m_currentPosition;
		bool m_inReadFromBuffer;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_MEDIAOBJECT_H
