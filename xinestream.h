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
#include "phononxineexport.h"

class QTimer;

namespace Phonon
{
namespace Xine
{
class VideoWidgetInterface;
class SeekCommandEvent;

/**
 * \brief xine_stream_t wrapper that runs in its own thread.
 *
 * The xine_stream_t object is created as late as possible so that it doesn't have to be recreated
 * when an audio- or video_port is added.
 *
 * \author Matthias Kretz <kretz@kde.org>
 */
class PHONON_XINE_ENGINE_EXPORT XineStream : public QThread
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
        void setVideoPort(VideoWidgetInterface *vwi);
        void setTickInterval(qint32 interval);
        void setAboutToFinishTime(qint32 time);

        void setParam(int param, int value);
        void eventSend(xine_event_t *);
        void useGaplessPlayback(bool);
        void gaplessSwitchTo(const KUrl &url);
        void gaplessSwitchTo(const QByteArray &mrl);
        void closeBlocking();
        void waitForEventLoop();
        void aboutToDeleteVideoWidget();
        VideoWidgetInterface *videoWidget() const
        {
            if (m_newVideoPort) {
                return m_newVideoPort;
            }
            return m_videoPort;
        }
        AudioPort audioPort() const { return m_audioPort; }

        QString errorString() const { return m_errorString; }
        Phonon::ErrorType errorType() const { return m_errorType; }

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
        void seekableChanged(bool);

    protected:
        bool event(QEvent *ev);
        void run();
        void timerEvent(QTimerEvent *event);

    private slots:
        void getStartTime();
        void emitAboutToFinish();
        void emitTick();
        void eventLoopReady();

    private:
        void getStreamInfo();
        bool xineOpen();
        void updateMetaData();
        void rewireOutputPorts();
        bool createStream();
        void changeState(Phonon::State newstate);
        void emitAboutToFinishIn(int timeToAboutToFinishSignal);
        bool updateTime();
        void playbackFinished();
        void error(Phonon::ErrorType, const QString &);

        xine_stream_t *m_stream;
        xine_event_queue_t *m_event_queue;

        AudioPort m_audioPort;
        AudioPort m_newAudioPort;
        VideoWidgetInterface *m_videoPort;
        VideoWidgetInterface *m_newVideoPort;

        Phonon::State m_state;

        QMutex m_portMutex;
        QMutex m_playMutex;
        mutable QMutex m_mutex;
        mutable QMutex m_streamInfoMutex;
        mutable QMutex m_updateTimeMutex;
        mutable QWaitCondition m_waitingForStreamInfo;
        QWaitCondition m_waitingForEventLoop;
        QWaitCondition m_waitingForClose;
        QWaitCondition m_waitingForRewire;
        QMultiMap<QString, QString> m_metaDataMap;
        QByteArray m_mrl;
        QTimer *m_tickTimer;
        QTimer *m_aboutToFinishTimer;
        struct timeval m_lastTimeUpdate;

        QString m_errorString;
        Phonon::ErrorType m_errorType;

        SeekCommandEvent *m_lastSeekCommand;
        qint32 m_aboutToFinishTime;
        int m_volume;
        int m_startTime;
        int m_totalTime;
        int m_currentTime;
        int m_waitForPlayingTimerId;
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
