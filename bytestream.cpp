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

#include "bytestream.h"
#include <kdebug.h>

#include "xine_engine.h"
#include <QEvent>
#include <cstring>
#include <cstdio>
#include <unistd.h>

extern "C" {
#include <xine/xine_plugin.h>
#define this this_xine
#include <xine/input_plugin.h>
#undef this
}

//#define VERBOSE_DEBUG
#ifdef VERBOSE_DEBUG
#  define PXINE_VDEBUG kDebug( 610 )
#else
#  define PXINE_VDEBUG kndDebug()
#endif
#define PXINE_DEBUG kDebug( 610 )

static const size_t MAXBUFFERSIZE = 1024 * 128; // 128kB

extern plugin_info_t kbytestream_xine_plugin_info[];

namespace Phonon
{
namespace Xine
{
ByteStream::ByteStream(QObject* parent)
    : MediaObject(parent),
    m_seekable(false),
    m_streamSize(0),
    m_intstate(CreatedState),
    m_buffersize(0),
    m_offset(0),
    m_currentPosition(0),
    m_inReadFromBuffer(false),
    m_inDtor(false),
    m_eod(false)
{
    // created in the main thread
    m_mainThread = pthread_self();

    xine_register_plugins(XineEngine::xine(), kbytestream_xine_plugin_info);

    connect(this, SIGNAL(needDataQueued()), this, SIGNAL(needData()), Qt::QueuedConnection);
    connect(this, SIGNAL(seekStreamQueued(qint64)), this, SLOT(syncSeekStream(qint64)), Qt::QueuedConnection);
}

void ByteStream::pullBuffer(char *buf, int len)
{
    // never called from main thread
    Q_ASSERT(m_mainThread != pthread_self());

    PXINE_VDEBUG << k_funcinfo << len << ", m_offset = " << m_offset << ", m_currentPosition = "
        << m_currentPosition << ", m_buffersize = " << m_buffersize << endl;
    // the preview doesn't change anymore when this method is called -> no mutex
    if (m_currentPosition < m_preview.size()) {
        int tocopy = qMin(len, static_cast<int>(m_preview.size() - m_currentPosition));
        PXINE_VDEBUG << k_funcinfo << "reading " << tocopy << " bytes from preview buffer" << endl;
        memcpy(buf, m_preview.constData() + m_currentPosition, tocopy);
        buf += tocopy;
        len -= tocopy;
        PXINE_VDEBUG << k_funcinfo << "reading " << len << " bytes from m_buffers" << endl;
    }
    while (len > 0) {
        m_mutex.lock();
        //kDebug(610) << "LOCKED m_mutex: " << k_funcinfo << endl;
        if (m_buffers.isEmpty()) {
            // pullBuffer is only called when there's => len data available
            kFatal(610) << k_funcinfo << "m_currentPosition = " << m_currentPosition
                << ", m_preview.size() = " << m_preview.size() << ", len = " << len << kBacktrace()
                << endl;
        }
        if (m_buffers.head().size() - m_offset <= len) {
            // The whole data of the next buffer is needed
            QByteArray buffer = m_buffers.dequeue();
            PXINE_VDEBUG << k_funcinfo << "dequeue one buffer of size " << buffer.size()
                << ", reading at offset = " << m_offset << ", resetting m_offset to 0" << endl;
            Q_ASSERT(buffer.size() > 0);
            int tocopy = buffer.size() - m_offset;
            Q_ASSERT(tocopy > 0);
            memcpy(buf, buffer.constData() + m_offset, tocopy);
            buf += tocopy;
            len -= tocopy;
            Q_ASSERT(len >= 0);
            Q_ASSERT(m_buffersize >= static_cast<size_t>(tocopy));
            m_buffersize -= tocopy;
            m_offset = 0;
        } else {
            // only a part of the next buffer is needed
            PXINE_VDEBUG << k_funcinfo << "read " << len
                << " bytes from the first buffer at offset = " << m_offset << endl;
            QByteArray &buffer = m_buffers.head();
            Q_ASSERT(buffer.size() > 0);
            memcpy(buf, buffer.constData() + m_offset , len);
            m_offset += len;
            Q_ASSERT(m_buffersize >= static_cast<size_t>(len));
            m_buffersize -= len;
            len = 0;
        }
        //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
        m_mutex.unlock();
    }
    // The buffer is half empty and there's more data to come: request more data
    if (m_buffersize < MAXBUFFERSIZE / 2 && !m_eod) {
        PXINE_VDEBUG << k_funcinfo << "emitting needData" << endl;
        emit needDataQueued();
    }
}

int ByteStream::peekBuffer(void *buf)
{
    // never called from main thread
    Q_ASSERT(m_mainThread != pthread_self());

    memcpy(buf, m_preview.constData(), m_preview.size());
    return m_preview.size();
}

qint64 ByteStream::readFromBuffer(void *buf, size_t count)
{
    // never called from main thread
    Q_ASSERT(m_mainThread != pthread_self());

    Q_ASSERT(!m_inReadFromBuffer);
    m_inReadFromBuffer = true;

    qint64 currentPosition = m_currentPosition;

    PXINE_VDEBUG << k_funcinfo << count << endl;

    // get data while more is needed and while we're still receiving data
    qint64 previewsize = qMax(static_cast<qint64>(0), static_cast<qint64>(m_preview.size()) - m_currentPosition);
    if (previewsize + m_buffersize < count && !m_eod) {
        // the thread needs to sleep until a wait condition is signalled from writeData
        PXINE_VDEBUG << k_funcinfo << "xine waits for data: " << previewsize + m_buffersize << ", " << m_eod << ", " << (m_intstate & OpenedState) << endl;
        m_mutex.lock();
        //kDebug(610) << "LOCKED m_mutex: " << k_funcinfo << endl;
        while (previewsize + m_buffersize < count && !m_eod) {// && (m_intstate & OpenedState)) {
            emit needDataQueued();
            if (!m_inDtor) {
                m_waitingForData.wait(&m_mutex);
            }
            if (m_inDtor) {
                //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
                m_mutex.unlock();
                return 0;
            }
        }
        //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
        m_mutex.unlock();

        // better be safe than sorry, m_currentPosition might have changed while waiting:
        Q_ASSERT(currentPosition == m_currentPosition);
        //previewsize = qMax(static_cast<qint64>(0), static_cast<qint64>(m_preview.size()) - m_currentPosition);
        //PXINE_VDEBUG << "m_buffersize = " << m_buffersize << endl;
    }
    if (previewsize + m_buffersize >= count) {
        PXINE_VDEBUG << k_funcinfo << "calling pullBuffer with previewsize = " << previewsize
            << ", m_buffersize = " << m_buffersize << endl;
        pullBuffer(static_cast<char*>(buf), count);
        m_currentPosition += count;
        m_inReadFromBuffer = false;
        return count;
    } else if (previewsize + m_buffersize > 0) {
        PXINE_VDEBUG << k_funcinfo << "calling pullBuffer with previewsize = " << previewsize
            << ", m_buffersize = " << m_buffersize << endl;
        size_t tmp = m_buffersize + previewsize;
        pullBuffer(static_cast<char*>(buf), tmp);
        m_currentPosition += tmp;
        m_inReadFromBuffer = false;
        return tmp;
    }
    PXINE_VDEBUG << k_funcinfo << "return 0" << endl;
    m_inReadFromBuffer = false;
    return 0;
}

off_t ByteStream::seekBuffer(qint64 offset)
{
    // never called from main thread
    Q_ASSERT(m_mainThread != pthread_self());

    // no seek
    if (offset == m_currentPosition) {
        return m_currentPosition;
    }

    // first try to seek in the data we have buffered
    m_mutex.lock();
    //kDebug(610) << "LOCKED m_mutex: " << k_funcinfo << endl;
    if (offset <= m_preview.size() && m_currentPosition <= m_preview.size()) {
        // seek in the preview data
        m_offset = 0;
        m_currentPosition = offset;
        //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
        m_mutex.unlock();
        return m_currentPosition;
    } else if (offset > m_currentPosition && offset < m_currentPosition + m_buffersize) {
        // seek behind the current position in the buffer
        while( offset > m_currentPosition ) {
            const int gap = offset - m_currentPosition;
            const int buffersize = m_buffers.head().size() - m_offset;
            if (buffersize <= gap) {
                // discard buffers if they hold data before offset
                QByteArray buffer = m_buffers.dequeue();
                m_buffersize -= buffersize;
                m_currentPosition += buffersize;
                m_offset = 0;
            } else {
                // offset points to data in the next buffer
                m_buffersize -= gap;
                m_currentPosition += gap;
                m_offset += gap;
            }
        }
        Q_ASSERT( offset == m_currentPosition );
        //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
        m_mutex.unlock();
        // The buffer is half empty and there's more data to come: request more data
        if (m_buffersize < MAXBUFFERSIZE / 2 && !m_eod) {
            PXINE_VDEBUG << k_funcinfo << "emitting needData" << endl;
            emit needDataQueued();
        }
        return m_currentPosition;
    } else if (offset < m_currentPosition && m_currentPosition - offset <= m_offset) {
        // seek before the current position in the buffer
        m_offset -= m_currentPosition - offset;
        m_buffersize += m_currentPosition - offset;
        Q_ASSERT(m_offset >= 0);
        m_currentPosition = offset;
        //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
        m_mutex.unlock();
        return m_currentPosition;
    }
    //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
    m_mutex.unlock();

    // the ByteStream is not seekable: no chance to seek to the requested offset
    if (!streamSeekable()) {
        return m_currentPosition;
    }

    // impossible seek
    if (offset > streamSize()) {
        kWarning(610) << "xine is asking to seek behind the end of the data stream" << endl;
        return m_currentPosition;
    }

    // throw away the buffers and ask for new data

    m_mutex.lock();
    //kDebug(610) << "LOCKED m_mutex: " << k_funcinfo << endl;
    m_buffers.clear();
    m_buffersize = 0;
    m_offset = 0;
    m_eod = false;

    m_currentPosition = offset;
    //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
    m_mutex.unlock();

    m_seekMutex.lock();
    emit seekStreamQueued(qMax(static_cast<qint64>(m_preview.size()), offset)); //calls syncSeekStream from the main thread
    m_seekWaitCondition.wait(&m_seekMutex); // waits until the seekStream signal returns
    m_seekMutex.unlock();
    return offset;
}

off_t ByteStream::currentPosition() const
{
    return m_currentPosition;
}

ByteStream::~ByteStream()
{
    PXINE_DEBUG << k_funcinfo << endl;
    m_mutex.lock();
    m_inDtor = true;
    // the other thread is now not between m_mutex.lock() and m_waitingForData.wait(&m_mutex), so it
    // won't get stuck in m_waitingForData.wait if it's not there right now
    m_mutex.unlock();
    stream().setMrl(QByteArray());
    m_seekWaitCondition.wakeAll();
    m_waitingForData.wakeAll();
    stream().closeBlocking();
}

QByteArray ByteStream::mrl() const
{
    QByteArray mrl("kbytestream:/");
    // the address can contain 0s which will null-terminate the C-string
    // use a simple encoding: 0x00 -> 0x0101, 0x01 -> 0x0102
    const ByteStream *iface = this;
    const unsigned char *that = reinterpret_cast<const unsigned char *>(&iface);
    for(unsigned int i = 0; i < sizeof( void* ); ++i, ++that) {
        if (*that <= 1) {
            mrl += 0x01;
            if (*that == 1) {
                mrl += 0x02;
            } else {
                mrl += 0x01;
            }
        } else {
            mrl += *that;
        }
    }
    return mrl;
}

/*
bool ByteStream::xineOpen()
{
	if( ( m_intstate != AboutToOpenState ) && ( m_intstate != ( AboutToOpenState | StreamAtEndState ) ) )
	{
		PXINE_VDEBUG << k_funcinfo << "not ready yet! state = " << m_intstate << endl;
		return true;
	}
	if( !isSeekable() && !canRecreateStream() )
	{
		kWarning( 610 ) << k_funcinfo << "cannot reopen a non-seekable stream, the ByteStream has to be recreated." << endl;
		return false;
	}

	PXINE_VDEBUG << k_funcinfo << m_intstate << endl;

	delayedInit();

    QByteArray mrl = mrl();

	if( !isSeekable() )
	{
		const off_t oldPos = m_currentPosition;
		if( 0 != seekBuffer( 0 ) )
		{
			seekBuffer( oldPos );
			return false;
		}
	}
	if( 0 == xine_open( stream(), mrl.constData() ) )
	{
		setState( Phonon::ErrorState );
		return false;
	}
	emit length( totalTime() );
	updateMetaData();
	stateTransition( m_intstate | OpenedState );
	if( state() == Phonon::BufferingState )
		play();
	else
		setState( Phonon::StoppedState );
	return true;
}
*/

void ByteStream::setStreamSize(qint64 x)
{
    PXINE_VDEBUG << k_funcinfo << x << endl;
    m_streamSize = x;
    stateTransition(m_intstate | ByteStream::StreamSizeSetState);
}

void ByteStream::endOfData()
{
    PXINE_VDEBUG << k_funcinfo << endl;
    m_eod = true;
    stream().setMrl(mrl());
    m_waitingForData.wakeAll();
}

qint64 ByteStream::streamSize() const
{
    return m_streamSize;
}

void ByteStream::setStreamSeekable(bool seekable)
{
    m_seekable = seekable;
}

bool ByteStream::streamSeekable() const
{
    return m_seekable;
}

bool ByteStream::isSeekable() const
{
    if (m_seekable) {
        return MediaObject::isSeekable();
    }
    return false;
}

void ByteStream::writeData(const QByteArray &data)
{
    if (data.size() <= 0) {
        return;
    }

    // first fill the preview buffer
    if (!(m_intstate & PreviewReadyState)) {
        PXINE_DEBUG << k_funcinfo << "fill preview" << endl;
        // more data than the preview buffer needs
        if (m_preview.size() + data.size() > MAX_PREVIEW_SIZE) {
            int tocopy = MAX_PREVIEW_SIZE - m_preview.size();
            m_preview += data.left(tocopy);
            QByteArray leftover(data.right(data.size() - tocopy));
            m_mutex.lock();
            //kDebug(610) << "LOCKED m_mutex: " << k_funcinfo << endl;
            m_buffers.enqueue(leftover);
            m_buffersize += leftover.size();
            //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
            m_mutex.unlock();
            PXINE_VDEBUG << k_funcinfo << "enqueued " << leftover.size() << " bytes to m_buffers, m_buffersize = " << m_buffersize << endl;
        } else { // all data fits into the preview buffer
            m_preview += data;
        }

        PXINE_VDEBUG << k_funcinfo << "filled preview buffer to " << m_preview.size() << endl;
        if (m_preview.size() == MAX_PREVIEW_SIZE) { // preview buffer is full
            stateTransition(m_intstate | PreviewReadyState);
        }
        return; // all data has been written to buffers. done.
    }

    PXINE_VDEBUG << k_funcinfo << data.size() << " m_intstate = " << m_intstate << endl;

    m_mutex.lock();
    //kDebug(610) << "LOCKED m_mutex: " << k_funcinfo << endl;
    m_buffers.enqueue(data);
    m_buffersize += data.size();
    //kDebug(610) << "UNLOCKING m_mutex: " << k_funcinfo << endl;
    m_mutex.unlock();
    PXINE_VDEBUG << k_funcinfo << "m_buffersize = " << m_buffersize << endl;
    if (m_buffersize > MAXBUFFERSIZE) {
        PXINE_VDEBUG << k_funcinfo << "emitting enoughData" << endl;
        emit enoughData();
    }
    m_waitingForData.wakeOne();
}

void ByteStream::syncSeekStream(qint64 offset)
{
    PXINE_VDEBUG << k_funcinfo << endl;
    m_seekMutex.lock();
    emit seekStream( offset );
    m_seekMutex.unlock();
    m_seekWaitCondition.wakeOne();
}

bool ByteStream::canRecreateStream() const
{
    // if we still have all the data from the beginning
    return ((m_currentPosition <= m_preview.size() || m_currentPosition - m_preview.size() == m_offset)
            // and if that won't change anytime soon
            && !(m_intstate & PlaybackState));
    // return true else false
}

void ByteStream::play()
{
    PXINE_VDEBUG << k_funcinfo << endl;
    /*if ((m_intstate & AboutToOpenState) == AboutToOpenState) {
        // the stream was playing before and has been stopped, so try to open it again, if it
        // fails (e.g. because the data stream cannot be seeked back) go into ErrorState
			if( xineOpen() )
				play();
			else
				setState( Phonon::ErrorState );
    } else {*/
        AbstractMediaProducer::play(); // goes into Phonon::BufferingState/PlayingState
        stateTransition(m_intstate | PlaybackState);
    //}
}

void ByteStream::stop()
{
	PXINE_VDEBUG << k_funcinfo << endl;
	//if( stream() )
	//{
		// don't call stateTransition so that xineOpen isn't called automatically
		m_intstate &= AboutToOpenState;

		AbstractMediaProducer::stop();
	//}
}

void ByteStream::stateTransition( int newState )
{
	if( m_intstate == newState )
		return;

	PXINE_VDEBUG << k_funcinfo << newState << endl;
    // if the OpenedState was unset
    if (!(newState & OpenedState) && (m_intstate & OpenedState)) {
        m_waitingForData.wakeAll();
    }
    m_intstate = newState;
	switch( newState )
	{
		case AboutToOpenState:
            stream().setMrl(mrl());
			break;
		default:
			break;
	}
}

}} //namespace Phonon::Xine

#include "bytestream.moc"
// vim: sw=4 ts=4
