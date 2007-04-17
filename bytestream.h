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
#ifndef PHONON_XINE_BYTESTREAM_H
#define PHONON_XINE_BYTESTREAM_H

#include "mediaobject.h"

#include <xine.h>

#include "xineengine.h"
#include <phonon/bytestreaminterface.h>
#include <QByteArray>
#include <QQueue>
#include <kdebug.h>
#include <QCoreApplication>
#include <QMutex>
#include <QWaitCondition>
#include <pthread.h>
#include <cstdlib>

namespace Phonon
{
namespace Xine
{
    class ByteStream : public MediaObjectBase, public ByteStreamInterface
    {
        Q_OBJECT
        Q_INTERFACES( Phonon::ByteStreamInterface )
        public:
            ByteStream(QObject* parent);
            ~ByteStream();

        public slots:
            qint64 totalTime() const { return MediaProducer::totalTime(); }
            qint64 remainingTime() const { return MediaProducer::remainingTime(); }
            virtual bool isSeekable() const;

            void writeData(const QByteArray &data);

			void play();
			void stop();

			void endOfData();

			void setStreamSeekable( bool );
			bool streamSeekable() const;

			void setStreamSize( qint64 );
			qint64 streamSize() const;

            void setPauseForBuffering(bool);

            void setAboutToFinishTime(qint32 newAboutToFinishTime) { MediaObjectBase::setAboutToFinishTime(newAboutToFinishTime); }
            qint32 aboutToFinishTime() const { return MediaObjectBase::aboutToFinishTime(); }

            // for the xine input plugin:
            int peekBuffer(void *buf);
            qint64 readFromBuffer(void *buf, size_t count);
            off_t seekBuffer(qint64 offset);
            off_t currentPosition() const;

        signals:
            /* finished, aboutToFinish and length are emitted by MediaObjectBase already */
            void needData();
            void needDataQueued();
            void enoughData();
            void seekStream(qint64);
            void seekStreamQueued(qint64);

        protected:
            virtual void stateTransition(int newState);

            bool m_aboutToFinishNotEmitted;

        private slots:
            void syncSeekStream(qint64 offset);

        private:
            QByteArray mrl() const;
            void setMrl();
            void pullBuffer(char *buf, int len);
            enum State
            {
                PreviewReadyState = 1,
                StreamSizeSetState = 2,
                AboutToOpenState = PreviewReadyState | StreamSizeSetState
            };

            bool m_seekable;
            bool m_mrlSet;
            qint64 m_streamSize;

		QByteArray m_preview;
		QMutex m_mutex;
		QMutex m_seekMutex;
		QWaitCondition m_waitingForData;
		QWaitCondition m_seekWaitCondition;

		size_t m_buffersize;
		int m_offset;
		QQueue<QByteArray> m_buffers;
        //pthread_t m_mainThread;
		qint64 m_currentPosition;

        int m_intstate;
        bool m_stopped : 1;
        bool m_eod : 1;
        bool m_buffering : 1;
        bool m_playRequested : 1;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 sts=4 et tw=100
#endif // PHONON_XINE_BYTESTREAM_H
