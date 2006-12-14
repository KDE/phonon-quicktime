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
    XineOpen = 2008
};

class SeekCommandEvent : public QEvent
{
    public:
        SeekCommandEvent(qint64 time) : QEvent(static_cast<QEvent::Type>(SeekCommand)), m_time(time) {}
        qint64 time() const { return m_time; }

    private:
        qint64 m_time;
};

// called from main thread
XineStream::XineStream(QObject *parent)
    : QThread(parent),
    m_stream(0),
    m_event_queue(0),
    m_audioPort(0),
    m_videoPort(0),
    m_state(Phonon::LoadingState),
    m_volume(100),
//    m_startTime(-1),
    m_streamInfoReady(false),
    m_hasVideo(false),
    m_isSeekable(false),
    m_recreateEventSent(false)
{
    //moveToThread(this);
}

// xine thread
void XineStream::xineOpen()
{
    Q_ASSERT(QThread::currentThread() == this);
    m_mutex.lock();
    if (!m_stream) {
        createStream();
        if (!m_stream) {
            return;
        }
    }
    const int ret = xine_open(m_stream, m_mrl);
    m_mutex.unlock();
    m_waitingForXineOpen.wakeAll();
    if (ret == 0) {
        changeState(Phonon::ErrorState);
    } else {
        int total;
        xine_get_pos_length(m_stream, 0, 0, &total);
        emit length(total);
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

    int total;
    QMutexLocker locker(&m_mutex);

    if (xine_get_status(m_stream) == XINE_STATUS_IDLE) {
        QCoreApplication::postEvent(const_cast<XineStream*>(this), new QEvent(static_cast<QEvent::Type>(XineOpen)));
        // wait a few ms, perhaps the other thread finishes the event in time and this method
        // can return a useful value
        if (!m_waitingForXineOpen.wait(&m_mutex, 40)) {
            kDebug(610) << k_funcinfo << "waitcondition timed out" << endl;
            return -1;
        }
    }
    if (xine_get_pos_length(m_stream, 0, 0, &total) == 1) {
        if( total >= 0 ) {
            return total;
        }
    }
    return -1;
}

// called from main thread
int XineStream::remainingTime() const
{
    if (!m_stream || m_mrl.isEmpty()) {
        return 0;
    }

    int current, total;
    if (xine_get_pos_length(m_stream, 0, &current, &total) == 1) {
        if (total - current > 0) {
            return total - current;
        }
    }
    return 0;
}

// called from main thread
int XineStream::currentTime() const
{
    if (!m_stream || m_mrl.isEmpty()) {
        return -1;
    }

    int positiontime;
    QMutexLocker locker(&m_mutex);
    if (xine_get_pos_length(m_stream, 0, &positiontime, 0) != 1)
        return -1;

//X     if (m_startTime == -1) {
        return positiontime;
//X     } else {
//X         return positiontime - m_startTime;
//X     }
}

// called from main thread
bool XineStream::hasVideo() const
{
    if (!m_streamInfoReady) {
        QCoreApplication::postEvent(const_cast<XineStream*>(this), new QEvent(static_cast<QEvent::Type>(GetStreamInfo)));
        // wait a few ms, perhaps the other thread finishes the event in time and this method
        // can return a useful value
        QMutexLocker locker(&m_mutex);
        if (!m_waitingForStreamInfo.wait(&m_mutex, 40)) {
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
        QCoreApplication::postEvent(const_cast<XineStream*>(this), new QEvent(static_cast<QEvent::Type>(GetStreamInfo)));
        // wait a few ms, perhaps the other thread finishes the event in time and this method
        // can return a useful value
        QMutexLocker locker(&m_mutex);
        if (!m_waitingForStreamInfo.wait(&m_mutex, 40)) {
            kDebug(610) << k_funcinfo << "waitcondition timed out" << endl;
            return false;
        }
    }
    return m_isSeekable;
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
    emit stateChanged(newstate, oldstate);
    if (newstate == Phonon::StoppedState && oldstate != Phonon::LoadingState) {
        xine_close(m_stream); // TODO: is it necessary? should xine_close be called as late as possible?
    }
    if (newstate == Phonon::ErrorState) {
        if (m_event_queue) {
            xine_event_dispose_queue(m_event_queue);
            m_event_queue = 0;
        }
        if (m_stream) {
            xine_dispose(m_stream);
            m_stream = 0;
        }
    }
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
    kDebug(610) << "emitting metaDataChanged(" << m_metaDataMap << ")" << endl;
    emit metaDataChanged(m_metaDataMap);
}

// xine thread
bool XineStream::event(QEvent *ev)
{
    //Q_ASSERT(QThread::currentThread() == this);
    switch (ev->type()) {
        case Xine::MediaFinishedEvent:
            changeState(Phonon::StoppedState);
            emit finished();
            ev->accept();
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
            if (m_stream && !m_streamInfoReady) {
                QMutexLocker locker(&m_mutex);
                m_hasVideo   = xine_get_stream_info(m_stream, XINE_STREAM_INFO_HAS_VIDEO);
                m_isSeekable = xine_get_stream_info(m_stream, XINE_STREAM_INFO_SEEKABLE);
                m_streamInfoReady = true;
            }
            m_waitingForStreamInfo.wakeAll();
            ev->accept();
            return true;
        case UpdateVolume:
            if (m_stream) {
                xine_set_param(m_stream, XINE_PARAM_AUDIO_AMP_LEVEL, m_volume);
            }
            ev->accept();
            return true;
        case XineOpen:
            xineOpen();
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
                    if (!m_mrl.isEmpty()) {
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
                    xineOpen();
                }
                xine_play(m_stream, 0, 0);
                changeState(Phonon::PlayingState);
            }
            ev->accept();
            return true;
        case PauseCommand:
            if (m_state == Phonon::PlayingState || state() == Phonon::BufferingState) {
                xine_set_param(m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
                changeState(Phonon::PausedState);
            }
            ev->accept();
            return true;
        case StopCommand:
            xine_stop(m_stream);
            changeState(Phonon::StoppedState);
            ev->accept();
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
            }
            ev->accept();
            return true;
        default:
            return QThread::event(ev);
    }
}

void XineStream::setUrl(const KUrl &url)
{
    m_mrl = url.url().toUtf8();
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(XineOpen)));
}

void XineStream::setMrl(const QByteArray &mrl)
{
    m_mrl = mrl;
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(XineOpen)));
}

void XineStream::play()
{
    // FIXME: There should be an immediate state change
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(PlayCommand)));
}

void XineStream::pause()
{
    // FIXME: There should be an immediate state change
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(PauseCommand)));
}

void XineStream::stop()
{
    // FIXME: There should be an immediate state change
    QCoreApplication::postEvent(this, new QEvent(static_cast<QEvent::Type>(StopCommand)));
}

void XineStream::seek(qint64 time)
{
    QCoreApplication::postEvent(this, new SeekCommandEvent(time));
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
    exec();

    // clean ups
    if(m_event_queue) {
        xine_event_dispose_queue(m_event_queue);
        m_event_queue = 0;
    }
    if(m_stream) {
        xine_dispose(m_stream);
        m_stream = 0;
    }
}

} // namespace Xine
} // namespace Phonon

#include "xinestream.moc"

// vim: sw=4 ts=4
