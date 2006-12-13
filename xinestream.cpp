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
#include <QMutexLocker>
#include <QEvent>

namespace Phonon
{
namespace Xine
{

enum {
    GetStreamInfo = 2001,
    UpdateVolume = 2002,
    RecreateStream = 2003
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
    m_streamInfoReady(false),
    m_hasVideo(false),
    m_isSeekable(false),
    m_recreateEventSent(false)
{
}

// called from main thread
bool XineStream::hasVideo() const
{
    if (m_streamInfoReady) {
        return m_hasVideo;
    } else {
        QCoreApplication::postEvent(this, new QEvent(GetStreamInfo));
        return false;
    }
}

// called from main thread
bool XineStream::isSeekable() const
{
    if (m_streamInfoReady) {
        return m_isSeekable;
    } else {
        QCoreApplication::postEvent(this, new QEvent(GetStreamInfo));
        return false;
    }
}

// xine thread
bool XineStream::createStream()
{
    //QMutexLocker locker(&m_mutex);

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

    m_mutex.lock();
    m_stream = xine_stream_new(XineEngine::xine(), m_audioPort, m_videoPort);
    m_mutex.unlock();
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
}

//called from main thread
void XineStream::setVolume(int vol)
{
    if (m_volume != vol) {
        m_volume = vol;
        QCoreApplication::postEvent(this, QEvent(UpdateVolume));
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
    QCoreApplication::postEvent(this, QEvent(RecreateStream));
    m_recreateEventSent = true;
}

// xine thread
void XineStream::changeState(Phonon::State newstate)
{
    if (m_state == newstate) {
        return;
    }
    Phonon::State oldstate = m_state;
    m_state = newstate;
    emit stateChanged(newstate, oldstate);
}

// xine thread
void AbstractMediaProducer::updateMetaData()
{
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
    emit metaDataChanged(m_metaDataMap);
}

// xine thread
bool XineStream::event(QEvent *ev)
{
    switch (ev->type()) {
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
                m_hasVideo   = xine_get_stream_info(m_stream, XINE_STREAM_INFO_HAS_VIDEO);
                m_isSeekable = xine_get_stream_info(m_stream, XINE_STREAM_INFO_SEEKABLE);
                m_streamInfoReady = true;
            }
            ev->accept();
            return true;
        case UpdateVolume:
            if (m_stream) {
                xine_set_param(m_stream, XINE_PARAM_AUDIO_AMP_LEVEL, m_volume);
            }
            ev->accept();
            return true;
        case RecreateStream:
            m_recreateEventSent = false;
            {
                // save state; TODO: not all params are needed
                int params[ 33 ];
                for( int i = 1; i < 33; ++i )
                    params[ i ] = xine_get_param( m_stream, i );

                switch (m_state) {
                    case Phonon::PlayingState:
                        changeState(Phonon::BufferingState);
                        // fall through
                    case Phonon::BufferingState:
                    case Phonon::PausedState:
                        xine_stop();
                        break;
                }

                Q_ASSERT(m_event_queue);
                Q_ASSERT(m_stream);
                // dispose of old xine objects
                xine_event_dispose_queue(m_event_queue);
                xine_dispose(m_stream);

                // create new xine objects
                createStream();

                // restore state
                for (int i = 1; i < 33; ++i) {
                    xine_set_param(m_stream, i, params[i]);
                }
            }
            ev->accept();
            return true;
    }
    return QThread::event(ev);
}

// xine thread
void XineStream::run()
{
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
