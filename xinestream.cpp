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

#include "xinestream.h"
#include "xine_engine.h"
#include <QMutexLocker>
#include <QEvent>
#include <QCoreApplication>
#include <QTimer>
#include <kurl.h>

namespace Phonon
{
namespace Xine
{

enum {
    GetStreamInfo = 2001,
    UpdateVolume = 2002,
    RecreateStream = 2003,
    PlayCommand = 2004,
    PauseCommand = 2005,
    StopCommand = 2006,
    SeekCommand = 2007,
    MrlChanged = 2008,
    GaplessPlaybackChanged = 2009,
    GaplessSwitch = 2010,
    UpdateTime = 2011,
    SetTickInterval = 2012,
    SetAboutToFinishTime = 2013
};

class MrlChangedEvent : public QEvent
{
    public:
        MrlChangedEvent(const QByteArray &_mrl) : QEvent(static_cast<QEvent::Type>(MrlChanged)), mrl(_mrl) {}
        QByteArray mrl;
};

class SeekCommandEvent : public QEvent
{
    public:
        SeekCommandEvent(qint64 time) : QEvent(static_cast<QEvent::Type>(SeekCommand)), m_time(time) {}
        qint64 time() const { return m_time; }
    private:
        qint64 m_time;
};

class SetTickIntervalEvent : public QEvent
{
    public:
        SetTickIntervalEvent(qint32 interval) : QEvent(static_cast<QEvent::Type>(SetTickInterval)), m_interval(interval) {}
        qint32 interval() const { return m_interval; }
    private:
        qint32 m_interval;
};

class SetAboutToFinishTimeEvent : public QEvent
{
    public:
        SetAboutToFinishTimeEvent(qint32 interval) : QEvent(static_cast<QEvent::Type>(SetAboutToFinishTime)), m_interval(interval) {}
        qint32 time() const { return m_interval; }
    private:
        qint32 m_interval;
};

// called from main thread
XineStream::XineStream(QObject *parent)
    : QThread(parent),
    m_stream(0),
    m_event_queue(0),
    m_audioPort(0),
    m_videoPort(0),
    m_state(Phonon::LoadingState),
    m_tickTimer(0),
    m_aboutToFinishTimer(0),
    m_volume(100),
//    m_startTime(-1),
    m_totalTime(-1),
    m_currentTime(-1),
    m_streamInfoReady(false),
    m_hasVideo(false),
    m_isSeekable(false),
    m_recreateEventSent(false),
    m_useGaplessPlayback(false),
    m_aboutToFinishNotEmitted(true),
    m_ticking(false),
    m_closing(false),
    m_eventLoopReady(false)
{
}

// xine thread
void XineStream::xineOpen()
{
    Q_ASSERT(QThread::currentThread() == this);
    Q_ASSERT(m_stream);
    if (m_mrl.isEmpty() || m_closing) {
        return;
    }
    // only call xine_open if it's not already open
    Q_ASSERT(xine_get_status(m_stream) == XINE_STATUS_IDLE);

    // xine_open can call functions from ByteStream which will block waiting for data.
    //kDebug(610) << "xine_open(" << m_mrl.constData() << ")" << endl;
    const int ret = xine_open(m_stream, m_mrl);
    //m_waitingForXineOpen.wakeAll();
    if (ret == 0) {
        kDebug(610) << "xine_open failed for m_mrl = " << m_mrl.constData() << endl;
        changeState(Phonon::ErrorState);
    } else {
        m_lastTimeUpdate.tv_sec = 0;
        xine_get_pos_length(m_stream, 0, &m_currentTime, &m_totalTime);
        getStreamInfo();
        emit length(m_totalTime);
        updateMetaData();
        changeState(Phonon::StoppedState);
    }
}

// called from main thread
int XineStream::totalTime() const
{
    if (!m_stream || m_mrl.isEmpty()) {
        return -1;
    }
    return m_totalTime;
}

// called from main thread
int XineStream::remainingTime() const
{
    if (!m_stream || m_mrl.isEmpty()) {
        return 0;
    }
    QMutexLocker locker(&m_updateTimeMutex);
    if (m_state == Phonon::PlayingState && m_lastTimeUpdate.tv_sec > 0) {
        struct timeval now;
        gettimeofday(&now, 0);
        const int diff = (now.tv_sec - m_lastTimeUpdate.tv_sec) * 1000 + (now.tv_usec - m_lastTimeUpdate.tv_usec) / 1000;
        return m_totalTime - (m_currentTime + diff);
    }
    return m_totalTime - m_currentTime;
}

// called from main thread
int XineStream::currentTime() const
{
    if (!m_stream || m_mrl.isEmpty()) {
        return -1;
    }
    QMutexLocker locker(&m_updateTimeMutex);
    if (m_state == Phonon::PlayingState && m_lastTimeUpdate.tv_sec > 0) {
        struct timeval now;
        gettimeofday(&now, 0);
        const int diff = (now.tv_sec - m_lastTimeUpdate.tv_sec) * 1000 + (now.tv_usec - m_lastTimeUpdate.tv_usec) / 1000;
        return m_currentTime + diff;
    }
    return m_currentTime;
}

// called from main thread
bool XineStream::hasVideo() const
{
    if (!m_streamInfoReady) {
        QMutexLocker locker(&m_streamInfoMutex);
        QCoreApplication::postEvent(const_cast<XineStream*>(this), new QEvent(static_cast<QEvent::Type>(GetStreamInfo)));
        // wait a few ms, perhaps the other thread finishes the event in time and this method
        // can return a useful value
        // FIXME: this is non-deterministic: a program might fail sometimes and sometimes work
        // because of this
        if (!m_waitingForStreamInfo.wait(&m_streamInfoMutex, 80)) {
            kDebug(610) << k_funcinfo << "waitcondition timed out" << endl;
            return false;
        }
    }
    return m_hasVideo;
}

// called from main thread
bool XineStream::isSeekable() const
{
    if (!m_streamInfoReady) {
        QMutexLocker locker(&m_streamInfoMutex);
        QCoreApplication::postEvent(const_cast<XineStream*>(this), new QEvent(static_cast<QEvent::Type>(GetStreamInfo)));
        // wait a few ms, perhaps the other thread finishes the event in time and this method
        // can return a useful value
        // FIXME: this is non-deterministic: a program might fail sometimes and sometimes work
        // because of this
        if (!m_waitingForStreamInfo.wait(&m_streamInfoMutex, 80)) {
            kDebug(610) << k_funcinfo << "waitcondition timed out" << endl;
            return false;
        }
    }
    return m_isSeekable;
}

// xine thread
void XineStream::getStreamInfo()
{
    if (m_stream && !m_streamInfoReady && !m_mrl.isEmpty()) {
        if (xine_get_status(m_stream) == XINE_STATUS_IDLE) {
            kDebug(610) << "calling xineOpen from " << k_funcinfo << endl;
            xineOpen();
            if (xine_get_status(m_stream) == XINE_STATUS_IDLE) {
                return;
            }
        }
        QMutexLocker locker(&m_streamInfoMutex);
        m_hasVideo   = xine_get_stream_info(m_stream, XINE_STREAM_INFO_HAS_VIDEO);
        m_isSeekable = xine_get_stream_info(m_stream, XINE_STREAM_INFO_SEEKABLE);
        m_streamInfoReady = true;
    }
    m_waitingForStreamInfo.wakeAll();
}

// xine thread
bool XineStream::createStream()
{
    Q_ASSERT(QThread::currentThread() == this);

    if (m_stream || m_state == Phonon::ErrorState) {
        return false;
    }

    /*
	xine_audio_port_t *audioPort = NULL;
	xine_video_port_t *videoPort = NULL;
	if( m_audioPath )
		audioPort = m_audioPath->audioPort();
	if( m_videoPath )
		videoPort = m_videoPath->videoPort();
	if( m_stream && m_videoPort == videoPort && m_audioPort == audioPort )
		return;

	kDebug( 610 ) << "XXXXXXXXXXXXXX xine_stream_new( " << ( void* )XineEngine::xine() << ", " << ( void* )audioPort << ", " << ( void* )videoPort << " );" << kBacktrace() << endl;
    */

    m_stream = xine_stream_new(XineEngine::xine(), m_audioPort, m_videoPort);
    Q_ASSERT(!m_event_queue);
    m_event_queue = xine_event_new_queue(m_stream);
    xine_event_create_listener_thread(m_event_queue, &XineEngine::self()->xineEventListener, (void*)this);

    if (!m_audioPort) {
        xine_set_param(m_stream, XINE_PARAM_IGNORE_AUDIO, 1);
    } else if (m_volume != 100) {
        xine_set_param(m_stream, XINE_PARAM_AUDIO_AMP_LEVEL, m_volume);
    }
    if (!m_videoPort) {
        xine_set_param(m_stream, XINE_PARAM_IGNORE_VIDEO, 1);
    }

    if (m_useGaplessPlayback) {
        xine_set_param(m_stream, XINE_PARAM_EARLY_FINISHED_EVENT, 1);
    } else {
        xine_set_param(m_stream, XINE_PARAM_EARLY_FINISHED_EVENT, 0);
    }

    return true;
}

//called from main thread
void XineStream::setVolume(int vol)
{
    if (m_volume != vol) {
        m_volume = vol;
        QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(UpdateVolume)));
    }
}

//called from main thread
void XineStream::setAudioPort(xine_audio_port_t *port)
{
    if (port == m_audioPort) {
        return;
    }
    m_mutex.lock();
    m_audioPort = port;
    if (!m_stream) {
        m_mutex.unlock();
        return;
    }
    m_mutex.unlock();
    // schedule m_stream recreation
    recreateStream();
}

//called from main thread
void XineStream::setVideoPort(xine_video_port_t *port)
{
    if (port == m_videoPort) {
        return;
    }
    m_mutex.lock();
    m_videoPort = port;
    if (!m_stream) {
        m_mutex.unlock();
        return;
    }
    m_mutex.unlock();
    // schedule m_stream recreation
    recreateStream();
}

// called from main thread
void XineStream::setTickInterval(qint32 interval)
{
    QCoreApplication::postEvent(this, new SetTickIntervalEvent(interval));
}

// called from main thread
void XineStream::setAboutToFinishTime(qint32 time)
{
    QCoreApplication::postEvent(this, new SetAboutToFinishTimeEvent(time));
}

// called from main thread
void XineStream::useGaplessPlayback(bool b)
{
    if (m_useGaplessPlayback == b) {
        return;
    }
    m_useGaplessPlayback = b;
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(GaplessPlaybackChanged)));
}

// called from main thread
void XineStream::gaplessSwitchTo(const KUrl &url)
{
    m_mrl = url.url().toUtf8();
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(GaplessSwitch)));
}

// called from main thread
void XineStream::gaplessSwitchTo(const QByteArray &mrl)
{
    m_mrl = mrl;
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(GaplessSwitch)));
}

// called from main thread
void XineStream::recreateStream()
{
    // make sure that multiple recreate events are compressed to one
    if (m_recreateEventSent) {
        return;
    }
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(RecreateStream)));
    m_recreateEventSent = true;
}

// xine thread
void XineStream::changeState(Phonon::State newstate)
{
    Q_ASSERT(QThread::currentThread() == this);
    if (m_state == newstate) {
        return;
    }
    Phonon::State oldstate = m_state;
    m_state = newstate;
    if (newstate == Phonon::PlayingState) {
        if (m_ticking) {
            m_tickTimer->start();
            //kDebug(610) << "tickTimer started." << endl;
        }
        if (m_aboutToFinishTime > 0) {
            emitAboutToFinish();
        }
    } else if (oldstate == Phonon::PlayingState) {
        m_tickTimer->stop();
        //kDebug(610) << "tickTimer stopped." << endl;
        m_aboutToFinishNotEmitted = true;
        if (m_aboutToFinishTimer) {
            m_aboutToFinishTimer->stop();
        }
    } else if (newstate == Phonon::ErrorState) {
        kDebug(610) << "reached error state from: " << kBacktrace() << endl;
        if (m_event_queue) {
            xine_event_dispose_queue(m_event_queue);
            m_event_queue = 0;
        }
        if (m_stream) {
            xine_dispose(m_stream);
            m_stream = 0;
        }
    }
    emit stateChanged(newstate, oldstate);
}

// xine thread
void XineStream::updateMetaData()
{
    Q_ASSERT(QThread::currentThread() == this);
    QMultiMap<QString, QString> metaDataMap;
    metaDataMap.insert(QLatin1String("TITLE"),
            QString::fromUtf8(xine_get_meta_info(m_stream, XINE_META_INFO_TITLE)));
    metaDataMap.insert(QLatin1String("ARTIST"),
            QString::fromUtf8(xine_get_meta_info(m_stream, XINE_META_INFO_ARTIST)));
    metaDataMap.insert(QLatin1String("GENRE"),
            QString::fromUtf8(xine_get_meta_info(m_stream, XINE_META_INFO_GENRE)));
    metaDataMap.insert(QLatin1String("ALBUM"),
            QString::fromUtf8(xine_get_meta_info(m_stream, XINE_META_INFO_ALBUM)));
    metaDataMap.insert(QLatin1String("DATE"),
            QString::fromUtf8(xine_get_meta_info(m_stream, XINE_META_INFO_YEAR)));
    metaDataMap.insert(QLatin1String("TRACKNUMBER"),
            QString::fromUtf8(xine_get_meta_info(m_stream, XINE_META_INFO_TRACK_NUMBER)));
    metaDataMap.insert(QLatin1String("DESCRIPTION"),
            QString::fromUtf8(xine_get_meta_info(m_stream, XINE_META_INFO_COMMENT)));
    if(metaDataMap == m_metaDataMap)
        return;
    m_metaDataMap = metaDataMap;
    //kDebug(610) << "emitting metaDataChanged(" << m_metaDataMap << ")" << endl;
    emit metaDataChanged(m_metaDataMap);
}

// xine thread
void XineStream::playbackFinished()
{
    {
        QMutexLocker locker(&m_mutex);
        changeState(Phonon::StoppedState);
        emit finished();
        xine_close(m_stream); // TODO: is it necessary? should xine_close be called as late as possible?
        m_streamInfoReady = false;
        m_aboutToFinishNotEmitted = true;
    }
    m_waitingForClose.wakeAll();
}

// xine thread
bool XineStream::event(QEvent *ev)
{
    //Q_ASSERT(QThread::currentThread() == this);
    switch (ev->type()) {
        case Xine::MediaFinishedEvent:
            if (m_useGaplessPlayback) {
                emit needNextUrl();
            } else {
                playbackFinished();
            }
            ev->accept();
            return true;
        case UpdateTime:
            updateTime();
            ev->accept();
            return true;
        case GaplessSwitch:
            ev->accept();
            {
                m_mutex.lock();
                if (m_mrl.isEmpty() || m_closing) {
                    m_mutex.unlock();
                    playbackFinished();
                    return true;
                }
                xine_set_param(m_stream, XINE_PARAM_GAPLESS_SWITCH, 1);
                if (!xine_open(m_stream, m_mrl)) {
                    kWarning(610) << "xine_open for gapless playback failed!" << endl;
                    xine_set_param(m_stream, XINE_PARAM_GAPLESS_SWITCH, 0);
                    m_mutex.unlock();
                    playbackFinished();
                    return true; // FIXME: correct?
                }
                m_mutex.unlock();
                xine_play(m_stream, 0, 0);

                m_aboutToFinishNotEmitted = true;
                getStreamInfo();
                emit finished();
                xine_get_pos_length(m_stream, 0, &m_currentTime, &m_totalTime);
                emit length(m_totalTime);
                updateMetaData();
            }
            return true;
        case Xine::NewMetaDataEvent:
            updateMetaData();
            ev->accept();
            return true;
        case Xine::ProgressEvent:
            {
                XineProgressEvent* e = static_cast<XineProgressEvent*>(ev);
                if (e->percent() < 100) {
                    if (m_state == Phonon::PlayingState) {
                        changeState(Phonon::BufferingState);
                    }
                } else {
                    if (m_state == Phonon::BufferingState) {
                        changeState(Phonon::PlayingState);
                    }
                    QTimer::singleShot(20, this, SLOT(getStartTime()));
                }
            }
            ev->accept();
            return true;
        case GetStreamInfo:
            getStreamInfo();
            ev->accept();
            return true;
        case UpdateVolume:
            if (m_stream) {
                xine_set_param(m_stream, XINE_PARAM_AUDIO_AMP_LEVEL, m_volume);
            }
            ev->accept();
            return true;
        case MrlChanged:
            ev->accept();
            {
                MrlChangedEvent *e = static_cast<MrlChangedEvent*>(ev);
                if (m_mrl == e->mrl) {
                    return true;
                }
                m_mrl = e->mrl;
                if (!m_stream) {
                    changeState(Phonon::LoadingState);
                    m_mutex.lock();
                    createStream();
                    m_mutex.unlock();
                    if (!m_stream) {
                        kError(610) << "MrlChangedEvent: createStream didn't create a stream. This should not happen." << endl;
                        changeState(Phonon::ErrorState);
                        return true;
                    }
                } else if (xine_get_status(m_stream) != XINE_STATUS_IDLE) {
                    m_mutex.lock();
                    xine_close(m_stream);
                    m_streamInfoReady = false;
                    m_aboutToFinishNotEmitted = true;
                    changeState(Phonon::LoadingState);
                    m_mutex.unlock();
                }
                if (m_closing || m_mrl.isEmpty()) {
                    kDebug(610) << "MrlChanged: don't call xineOpen. m_closing = " << m_closing << ", m_mrl = " << m_mrl.constData() << endl;
                    m_waitingForClose.wakeAll();
                } else {
                    kDebug(610) << "calling xineOpen from MrlChanged" << endl;
                    xineOpen();
                }
            }
            return true;
        case GaplessPlaybackChanged:
            if (m_stream) {
                if (m_useGaplessPlayback) {
                    xine_set_param(m_stream, XINE_PARAM_EARLY_FINISHED_EVENT, 1);
                } else {
                    xine_set_param(m_stream, XINE_PARAM_EARLY_FINISHED_EVENT, 0);
                }
            }
            ev->accept();
            return true;
        case RecreateStream:
            {
                QMutexLocker locker(&m_mutex);
                m_recreateEventSent = false;

                // TODO: Q_ASSERT that there were output port changes
                if (!m_stream) {
                    createStream();
                } else {
                    // save state; TODO: not all params are needed
                    Phonon::State oldstate = m_state;
                    int position;
                    xine_get_pos_length(m_stream, &position, 0, 0);
                    /*int params[ 33 ];
                    for (int i = 1; i < 33; ++i) {
                        params[ i ] = xine_get_param( m_stream, i );
                    }*/

                    switch (m_state) {
                        case Phonon::PlayingState:
                            changeState(Phonon::BufferingState);
                            // fall through
                        case Phonon::BufferingState:
                        case Phonon::PausedState:
                            xine_stop(m_stream);
                            break;
                        default:
                            break;
                    }

                    xine_close(m_stream);

                    // dispose of old xine objects
                    Q_ASSERT(m_event_queue);
                    xine_event_dispose_queue(m_event_queue);
                    m_event_queue = 0;
                    xine_dispose(m_stream);
                    m_stream = 0;

                    // create new xine objects
                    createStream();

                    // restore state
                    /*for (int i = 1; i < 33; ++i) {
                        xine_set_param(m_stream, i, params[i]);
                    }*/
                    if (!m_mrl.isEmpty() && !m_closing) {
                        xine_open(m_stream, m_mrl);
                        switch (oldstate) {
                            case Phonon::PausedState:
                                xine_play(m_stream, position, 0);
                                xine_set_param(m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
                                break;
                            case Phonon::PlayingState:
                            case Phonon::BufferingState:
                                xine_play(m_stream, position, 0);
                                changeState(Phonon::PlayingState);
                                break;
                            case Phonon::StoppedState:
                            case Phonon::LoadingState:
                            case Phonon::ErrorState:
                                break;
                        }
                    }
                }
            }
            ev->accept();
            return true;
        case PlayCommand:
            ev->accept();
            if (m_mrl.isEmpty()) {
                kError(610) << "PlayCommand: m_mrl is empty. This should not happen." << endl;
                changeState(Phonon::ErrorState);
                return true;
            }
            if (!m_stream) {
                QMutexLocker locker(&m_mutex);
                createStream();
                if (!m_stream) {
                    kError(610) << "PlayCommand: createStream didn't create a stream. This should not happen." << endl;
                    changeState(Phonon::ErrorState);
                    return true;
                }
            }
            if (m_state == Phonon::PausedState) {
                xine_set_param(m_stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
                changeState(Phonon::PlayingState);
            } else {
                //X                 int total;
                //X                 if (xine_get_pos_length(stream(), 0, &m_startTime, &total) == 1) {
                //X                     if (total > 0 && m_startTime < total && m_startTime >= 0)
                //X                         m_startTime = -1;
                //X                 } else {
                //X                     m_startTime = -1;
                //X                 }
                if (xine_get_status(m_stream) == XINE_STATUS_IDLE) {
                    kDebug(610) << "calling xineOpen from PlayCommand" << endl;
                    xineOpen();
                }
                xine_play(m_stream, 0, 0);
                changeState(Phonon::PlayingState);
            }
            return true;
        case PauseCommand:
            ev->accept();
            if (m_mrl.isEmpty()) {
                kError(610) << "PauseCommand: m_mrl is empty. This should not happen." << endl;
                changeState(Phonon::ErrorState);
                return true;
            }
            if (!m_stream) {
                QMutexLocker locker(&m_mutex);
                createStream();
                if (!m_stream) {
                    kError(610) << "PauseCommand: createStream didn't create a stream. This should not happen." << endl;
                    changeState(Phonon::ErrorState);
                    return true;
                }
            }
            if (m_state == Phonon::PlayingState || state() == Phonon::BufferingState) {
                xine_set_param(m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
                changeState(Phonon::PausedState);
            }
            return true;
        case StopCommand:
            ev->accept();
            if (m_mrl.isEmpty()) {
                kError(610) << "StopCommand: m_mrl is empty. This should not happen." << endl;
                changeState(Phonon::ErrorState);
                return true;
            }
            if (!m_stream) {
                QMutexLocker locker(&m_mutex);
                createStream();
                if (!m_stream) {
                    kError(610) << "StopCommand: createStream didn't create a stream. This should not happen." << endl;
                    changeState(Phonon::ErrorState);
                    return true;
                }
            }
            xine_stop(m_stream);
            changeState(Phonon::StoppedState);
            return true;
        case SetTickInterval:
            ev->accept();
            {
                SetTickIntervalEvent *e = static_cast<SetTickIntervalEvent*>(ev);
                if (e->interval() <= 0) {
                    // disable ticks
                    m_ticking = false;
                    m_tickTimer->stop();
                    //kDebug(610) << "tickTimer stopped." << endl;
                } else {
                    m_tickTimer->setInterval(e->interval());
                    if (m_ticking == false && m_state == Phonon::PlayingState) {
                        m_tickTimer->start();
                        //kDebug(610) << "tickTimer started." << endl;
                    }
                    m_ticking = true;
                }
            }
            return true;
        case SetAboutToFinishTime:
            ev->accept();
            {
                SetAboutToFinishTimeEvent *e = static_cast<SetAboutToFinishTimeEvent*>(ev);
                m_aboutToFinishTime = e->time();
                if (m_aboutToFinishTime > 0) {
                    updateTime();
                    if (m_currentTime < m_totalTime - m_aboutToFinishTime) { // not about to finish
                        m_aboutToFinishNotEmitted = true;
                        if (m_state == Phonon::PlayingState) {
                            emitAboutToFinishIn(m_totalTime - m_aboutToFinishTime - m_currentTime);
                        }
                    }
                }
            }
            return true;
        case SeekCommand:
            {
                SeekCommandEvent *e = static_cast<SeekCommandEvent*>(ev);
                switch(m_state) {
                    case Phonon::PausedState:
                    case Phonon::BufferingState:
                    case Phonon::PlayingState:
                        kDebug(610) << k_funcinfo << "seeking xine stream to " << e->time() << "ms" << endl;
                        // xine_trick_mode aborts :(
                        //if (0 == xine_trick_mode(m_stream, XINE_TRICK_MODE_SEEK_TO_TIME, time)) {
                        xine_play(m_stream, 0, e->time());
                        if (Phonon::PausedState == state()) {
                            // go back to paused speed after seek
                            xine_set_param(m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
                        }
                        //}
                        emit seekDone();
                        break;
                    case Phonon::StoppedState:
                    case Phonon::ErrorState:
                    case Phonon::LoadingState:
                        break; // cannot seek
                }
                const int timeToSignal = m_totalTime - m_aboutToFinishTime - e->time();
                if (m_aboutToFinishTime > 0) {
                    if (timeToSignal > 0 ) { // not about to finish
                        m_aboutToFinishNotEmitted = true;
                        emitAboutToFinishIn(timeToSignal);
                    } else if (m_aboutToFinishNotEmitted) {
                        m_aboutToFinishNotEmitted = false;
                        kDebug(610) << "emitting aboutToFinish(" << timeToSignal + m_aboutToFinishTime << ")" << endl;
                        emit aboutToFinish(timeToSignal + m_aboutToFinishTime);
                    }
                }
            }
            ev->accept();
            return true;
        default:
            return QThread::event(ev);
    }
}

// xine thread
void XineStream::eventLoopReady()
{
    m_mutex.lock();
    m_eventLoopReady = true;
    m_mutex.unlock();
    m_waitingForEventLoop.wakeAll();
}

// called from main thread
void XineStream::waitForEventLoop()
{
    m_mutex.lock();
    if (!m_eventLoopReady) {
        m_waitingForEventLoop.wait(&m_mutex);
    }
    m_mutex.unlock();
}

// called from main thread
void XineStream::closeBlocking()
{
    m_mutex.lock();
    if (m_stream && xine_get_status(m_stream) != XINE_STATUS_IDLE) {
        //m_mrl.clear();
        //Q_ASSERT(m_mrl.isEmpty());
        m_closing = true;

        // this event will call xine_close
        QCoreApplication::postEvent(this, new MrlChangedEvent(QByteArray()));

        // wait until the xine_close is done
        m_waitingForClose.wait(&m_mutex);
        m_closing = false;
    }
    m_mutex.unlock();
}

// called from main thread
void XineStream::setUrl(const KUrl &url)
{
    setMrl(url.url().toUtf8());
}

// called from main thread
void XineStream::setMrl(const QByteArray &mrl)
{
    /*
    if (m_mrl == mrl) {
        return;
    }
    m_mrl = mrl;
    */
    QCoreApplication::postEvent(this, new MrlChangedEvent(mrl));
}

// called from main thread
void XineStream::play()
{
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(PlayCommand)));
}

// called from main thread
void XineStream::pause()
{
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(PauseCommand)));
}

// called from main thread
void XineStream::stop()
{
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(StopCommand)));
}

// called from main thread
void XineStream::seek(qint64 time)
{
    QCoreApplication::postEvent(this, new SeekCommandEvent(time));
}

// xine thread
bool XineStream::updateTime()
{
    Q_ASSERT(QThread::currentThread() == this);
    if (m_stream) {
        if (xine_get_status(m_stream) == XINE_STATUS_IDLE) {
            kDebug(610) << "calling xineOpen from " << k_funcinfo << endl;
            xineOpen();
        }
        QMutexLocker locker(&m_updateTimeMutex);
        if (xine_get_pos_length(m_stream, 0, &m_currentTime, &m_totalTime) != 1) {
            m_currentTime = -1;
            m_totalTime = -1;
            m_lastTimeUpdate.tv_sec = 0;
            return false;
        }
        if (m_currentTime == 0) {
            // are we seeking? when xine seeks xine_get_pos_length returns 0 for m_currentTime
            m_lastTimeUpdate.tv_sec = 0;
            // XineStream::currentTime will return 0 now
            return false;
        }
        if (m_state == Phonon::PlayingState) {
            gettimeofday(&m_lastTimeUpdate, 0);
        } else {
            m_lastTimeUpdate.tv_sec = 0;
        }
    }
    return true;
}

// xine thread
void XineStream::emitAboutToFinishIn(int timeToAboutToFinishSignal)
{
    Q_ASSERT(QThread::currentThread() == this);
    //kDebug(610) << k_funcinfo << timeToAboutToFinishSignal << kBacktrace() << endl;
    Q_ASSERT(m_aboutToFinishTime > 0);
    if (!m_aboutToFinishTimer) {
        m_aboutToFinishTimer = new QTimer(this);
        //m_aboutToFinishTimer->setObjectName("aboutToFinish timer");
        Q_ASSERT(m_aboutToFinishTimer->thread() == this);
        m_aboutToFinishTimer->setSingleShot(true);
        connect(m_aboutToFinishTimer, SIGNAL(timeout()), SLOT(emitAboutToFinish()), Qt::DirectConnection);
    }
    m_aboutToFinishTimer->start(timeToAboutToFinishSignal);
}

// xine thread
void XineStream::emitAboutToFinish()
{
    Q_ASSERT(QThread::currentThread() == this);
    kDebug(610) << k_funcinfo << endl;
    if (m_aboutToFinishNotEmitted && m_aboutToFinishTime > 0) {
        updateTime();
        const int remainingTime = m_totalTime - m_currentTime;

        if (remainingTime <= m_aboutToFinishTime + 150) {
            m_aboutToFinishNotEmitted = false;
            kDebug(610) << "emitting aboutToFinish(" << remainingTime << ")" << endl;
            emit aboutToFinish(remainingTime);
        } else {
            emitAboutToFinishIn(remainingTime - m_aboutToFinishTime);
        }
    }
}

// xine thread
void XineStream::emitTick()
{
    Q_ASSERT(QThread::currentThread() == this);
    if (!updateTime()) {
        kDebug(610) << k_funcinfo << "no useful time information available. skipped." << endl;
        return;
    }
    //kDebug(610) << k_funcinfo << m_currentTime << endl;
    if (m_ticking) {
        emit tick(m_currentTime);
    }
    if (m_aboutToFinishNotEmitted && m_aboutToFinishTime > 0) {
        const int remainingTime = m_totalTime - m_currentTime;
        const int timeToAboutToFinishSignal = remainingTime - m_aboutToFinishTime;
        if (timeToAboutToFinishSignal <= m_tickTimer->interval()) { // about to finish
            if (timeToAboutToFinishSignal > 100) {
                emitAboutToFinishIn(timeToAboutToFinishSignal);
            } else {
                m_aboutToFinishNotEmitted = false;
                kDebug(610) << "emitting aboutToFinish(" << remainingTime << ")" << endl;
                emit aboutToFinish(remainingTime);
            }
        }
    }
}

// xine thread
void XineStream::getStartTime()
{
    Q_ASSERT(QThread::currentThread() == this);
//X     if (m_startTime == -1 || m_startTime == 0) {
//X         int total;
//X         if (xine_get_pos_length(m_stream, 0, &m_startTime, &total) == 1) {
//X             if(total > 0 && m_startTime < total && m_startTime >= 0)
//X                 m_startTime = -1;
//X         } else {
//X             m_startTime = -1;
//X         }
//X     }
//X     if (m_startTime == -1 || m_startTime == 0) {
//X         QTimer::singleShot(30, this, SLOT(getStartTime()));
//X     }
}

// xine thread
void XineStream::run()
{
    Q_ASSERT(QThread::currentThread() == this);
    m_tickTimer = new QTimer(this);
    //m_tickTimer->setObjectName("tick timer");
    connect(m_tickTimer, SIGNAL(timeout()), SLOT(emitTick()), Qt::DirectConnection);
    QTimer::singleShot(0, this, SLOT(eventLoopReady()));
    exec();
    m_eventLoopReady = false;

    // clean ups
    if(m_event_queue) {
        xine_event_dispose_queue(m_event_queue);
        m_event_queue = 0;
    }
    if(m_stream) {
        xine_dispose(m_stream);
        m_stream = 0;
    }
    delete m_aboutToFinishTimer;
    m_aboutToFinishTimer = 0;
    delete m_tickTimer;
    m_tickTimer = 0;
}

} // namespace Xine
} // namespace Phonon

#include "xinestream.moc"

// vim: sw=4 ts=4
