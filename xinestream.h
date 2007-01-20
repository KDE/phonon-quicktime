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

#ifndef PHONON_XINE_XINESTREAM_H
#define PHONON_XINE_XINESTREAM_H

#include <QThread>
#include <QMutex>
#include <QMultiMap>
#include <phonon/phononnamespace.h>
#include <xine.h>
#include <QWaitCondition>

#include <sys/time.h>
#include <time.h>
#include "audioport.h"

class QTimer;

namespace Phonon
{
namespace Xine
{

/**
 * \brief xine_stream_t wrapper that runs in its own thread.
 *
 * The xine_stream_t object is created as late as possible so that it doesn't have to be recreated
 * when an audio- or video_port is added.
 *
 * \author Matthias Kretz <kretz@kde.org>
 */
class XineStream : public QThread
{
    Q_OBJECT
    public:
        XineStream(QObject *parent = 0);

        Phonon::State state() const { return m_state; }

        int totalTime() const;
        int remainingTime() const;
        int currentTime() const;
        bool hasVideo() const;
        bool isSeekable() const;
        void setVolume(int vol);
        void setAudioPort(AudioPort port);
        void setVideoPort(xine_video_port_t *port);
        void setTickInterval(qint32 interval);
        void setAboutToFinishTime(qint32 time);

        void setParam(int param, int value) { xine_set_param(m_stream, param, value); }
        void useGaplessPlayback(bool);
        void gaplessSwitchTo(const KUrl &url);
        void gaplessSwitchTo(const QByteArray &mrl);
        void closeBlocking();
        void waitForEventLoop();

    public slots:
        void setUrl(const KUrl &url);
        void setMrl(const QByteArray &mrl);
        void play();
        void pause();
        void stop();
        void seek(qint64 time);

        /**
         * all signals emitted from the xine thread
         */
    Q_SIGNALS:
        void stateChanged(Phonon::State newstate, Phonon::State oldstate);
        void metaDataChanged(const QMultiMap<QString, QString>&);
        void length(qint64);
        void seekDone();
        void needNextUrl();
        void tick(qint64);
        void aboutToFinish(qint32);

    protected:
        bool event(QEvent *ev);
        void run();

    private slots:
        void getStartTime();
        void emitAboutToFinish();
        void emitTick();
        void eventLoopReady();

    private:
        void getStreamInfo();
        void xineOpen();
        void updateMetaData();
        void rewireOutputPorts();
        bool createStream();
        void changeState(Phonon::State newstate);
        void emitAboutToFinishIn(int timeToAboutToFinishSignal);
        bool updateTime();
        void playbackFinished();

        xine_stream_t *m_stream;
        xine_event_queue_t *m_event_queue;

        AudioPort m_audioPort;
        AudioPort m_newAudioPort;
        xine_video_port_t *m_videoPort;
        xine_video_port_t *m_newVideoPort;

        Phonon::State m_state;

        QMutex m_portMutex;
        QMutex m_playMutex;
        mutable QMutex m_mutex;
        mutable QMutex m_streamInfoMutex;
        mutable QMutex m_updateTimeMutex;
        mutable QWaitCondition m_waitingForStreamInfo;
        QWaitCondition m_waitingForEventLoop;
        QWaitCondition m_waitingForClose;
        QMultiMap<QString, QString> m_metaDataMap;
        QByteArray m_mrl;
        QTimer *m_tickTimer;
        QTimer *m_aboutToFinishTimer;
        struct timeval m_lastTimeUpdate;

        qint32 m_aboutToFinishTime;
        int m_volume;
        int m_startTime;
        int m_totalTime;
        int m_currentTime;
        bool m_streamInfoReady : 1;
        bool m_hasVideo : 1;
        bool m_isSeekable : 1;
        bool m_rewireEventSent : 1;
        bool m_useGaplessPlayback : 1;
        bool m_aboutToFinishNotEmitted : 1;
        bool m_ticking : 1;
        bool m_closing : 1;
        bool m_eventLoopReady : 1;
        bool m_playCalled : 1;
};

} // namespace Xine
} // namespace Phonon

#endif // PHONON_XINE_XINESTREAM_H
