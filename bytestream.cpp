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
#include <QTimer>
#include <kdebug.h>

#include "xine_engine.h"
#include <QEvent>
#include <cstring>
#include <cstdio>

extern "C" {
#include <xine/xine_plugin.h>
#define this this_xine
#include <xine/input_plugin.h>
#undef this
}

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
    : AbstractMediaProducer(parent),
    m_aboutToFinishNotEmitted(true),
    m_seekable(false),
    m_aboutToFinishTimer(0),
    m_streamSize(0),
    m_intstate(CreatedState),
    m_buffersize(0),
    m_offset(0),
    m_currentPosition(0),
    m_inReadFromBuffer(false)
{
    // created in the main thread
    m_mainThread = pthread_self();

    PXINE_DEBUG << k_funcinfo << endl;
	connect( this, SIGNAL( needDataQueued() ), this, SIGNAL( needData() ), Qt::QueuedConnection );
	connect( this, SIGNAL( seekStreamQueued( qint64 ) ), this, SLOT( slotSeekStream( qint64 ) ), Qt::QueuedConnection );
    connect(&stream(), SIGNAL(finished()), SLOT(handleFinished()));
    xine_register_plugins(XineEngine::xine(), kbytestream_xine_plugin_info);

    stream().setMrl(mrl());
}

void ByteStream::handleFinished()
{
    m_aboutToFinishNotEmitted = true;
    if (videoPath()) {
        videoPath()->streamFinished();
    }
    kDebug(610) << "emit finished()" << endl;
    emit finished();
}

		void pullBuffer( char *buf, int len ) {
			// called from either main or xine thread
			PXINE_VDEBUG << k_funcinfo << len << ", m_offset = " << m_offset << ", m_currentPosition = " << m_currentPosition << ", m_buffersize = " << m_buffersize << endl;
			// the preview doesn't change anymore when this method is called -> no mutex
			if( m_currentPosition < m_preview.size() )
			{
				int tocopy = qMin( len, static_cast<int>( m_preview.size() - m_currentPosition ) );
				PXINE_VDEBUG << k_funcinfo << "reading " << tocopy << " bytes from preview buffer" << endl;
				memcpy( buf, m_preview.constData() + m_currentPosition, tocopy );
				buf += tocopy;
				len -= tocopy;
				PXINE_VDEBUG << k_funcinfo << "reading " << len << " bytes from m_buffers" << endl;
			}
			while( len > 0 )
			{
				m_mutex.lock();
				if( m_buffers.isEmpty() )
				{
					kFatal() << k_funcinfo << "m_currentPosition = " << m_currentPosition << ", m_preview.size() = " << m_preview.size() << ", len = " << len << kBacktrace() << endl;
				}
				if( m_buffers.head().size() - m_offset <= len )
				{
					QByteArray buffer = m_buffers.dequeue();
					PXINE_VDEBUG << k_funcinfo << "dequeue one buffer of size " << buffer.size() << ", reading at offset = " << m_offset << ", resetting m_offset to 0" << endl;
					Q_ASSERT( buffer.size() > 0 );
					int tocopy = buffer.size() - m_offset;
					Q_ASSERT( tocopy > 0 );
					memcpy( buf, buffer.constData() + m_offset, tocopy );
					buf += tocopy;
					len -= tocopy;
					Q_ASSERT( len >= 0 );
					Q_ASSERT( m_buffersize >= static_cast<size_t>( tocopy ) );
					m_buffersize -= tocopy;
					m_offset = 0;
				}
				else
				{
					PXINE_VDEBUG << k_funcinfo << "read " << len << " bytes from the first buffer at offset = " << m_offset << endl;
					QByteArray &buffer = m_buffers.head();
					Q_ASSERT( buffer.size() > 0 );
					memcpy( buf, buffer.constData() + m_offset , len );
					m_offset += len;
					Q_ASSERT( m_buffersize >= static_cast<size_t>( len ) );
					m_buffersize -= len;
					len = 0;
				}
				m_mutex.unlock();
			}
			if( m_buffersize < MAXBUFFERSIZE / 2 && !( m_intstate & StreamAtEndState ) )
			{
				PXINE_VDEBUG << k_funcinfo << "emitting needData" << endl;
				if( m_mainThread == pthread_self() )
					emit needData();
				else
					emit needDataQueued();
			}
}

		int peekBuffer( void *buf ) {
			memcpy( buf, m_preview.constData(), m_preview.size() );
			return m_preview.size();
		}

		qint64 readFromBuffer( void *buf, size_t count ) {
			// called from either main or xine thread
			if( m_inReadFromBuffer )
				return 0;
			m_inReadFromBuffer = true;

			PXINE_VDEBUG << k_funcinfo << count << " called from " << ( pthread_self() == m_mainThread ? "main" : "xine" ) << " thread" << endl;

			/* get data while more is needed and while we're still receiving data */
			qint64 previewsize = qMax( static_cast<qint64>( 0 ), static_cast<qint64>( m_preview.size() ) - m_currentPosition );
			if( previewsize + m_buffersize < count && !( m_intstate & StreamAtEndState ) )
			{
				if( m_mainThread == pthread_self() )
				{
					// if it calls from the main thread processEvents has to be called to block until
					// the data has arrived
					PXINE_VDEBUG << k_funcinfo << "wait in main thread" << endl;
					while( previewsize + m_buffersize < count && !( m_intstate & StreamAtEndState ) )
					{
						emit needData();
						// XXX: this processEvents is pure evil but is needed for the following
						// situation:
						// - Xine::ByteStream::xineOpen calls xine_open
						// - xine_open goes through all the demuxers to find the right one for the
						// data
						// - each demuxer reads data from the input plugin (in the main thread)
						// - this method is called but there's not enough data available (if you
						// think the data could be buffered before xine_open is called you forgot
						// that a demuxer might seek and thereby invalidate the buffers)
						// - emitting needData does not ensure that writeData is called until some
						// time later in the event-loop
						QCoreApplication::processEvents( QEventLoop::ExcludeUserInputEvents );
					}
				}
				else
				{
					// if it calls from a different thread, the thread needs to sleep until a wait
					// condition is signalled from writeData
					PXINE_VDEBUG << k_funcinfo << "wait in xine thread" << endl;
					m_mutex.lock();
					while( previewsize + m_buffersize < count && !( m_intstate & StreamAtEndState ) && ( m_intstate & OpenedState ) )
					{
						emit needDataQueued();
						m_waitForDataCondition.wait( &m_mutex );
					}
					m_mutex.unlock();
				}
				// better be safe than sorry, m_currentPosition might have changed while waiting:
				previewsize = qMax( static_cast<qint64>( 0 ), static_cast<qint64>( m_preview.size() ) - m_currentPosition );
				//PXINE_VDEBUG << "m_buffersize = " << m_buffersize << endl;
			}
			if( previewsize + m_buffersize >= count )
			{
				PXINE_VDEBUG << k_funcinfo << "calling pullBuffer with previewsize = " << previewsize << ", m_buffersize = " << m_buffersize << endl;
				pullBuffer( static_cast<char*>( buf ), count );
				m_currentPosition += count;
				m_inReadFromBuffer = false;
				return count;
			}
			else if( previewsize + m_buffersize > 0 )
			{
				PXINE_VDEBUG << k_funcinfo << "calling pullBuffer with previewsize = " << previewsize << ", m_buffersize = " << m_buffersize << endl;
				size_t tmp = m_buffersize + previewsize;
				pullBuffer( static_cast<char*>( buf ), tmp );
				m_currentPosition += tmp;
				m_inReadFromBuffer = false;
				return tmp;
			}
			PXINE_VDEBUG << k_funcinfo << "return 0" << endl;
			m_inReadFromBuffer = false;
			return 0;
		}

		off_t seekBuffer( qint64 offset )
		{
			// no seek
			if( offset == m_currentPosition )
				return m_currentPosition;

			// first try to seek in the data we have buffered
			m_mutex.lock();
			// seek in the preview data
			if( offset <= m_preview.size() && m_currentPosition <= m_preview.size() )
			{
				m_offset = 0;
				m_currentPosition = offset;
				m_mutex.unlock();
				return m_currentPosition;
			}
			// seek behind the current position in the buffer
			else if( offset > m_currentPosition && offset < m_currentPosition + m_buffersize )
			{
				while( offset > m_currentPosition )
				{
					const int gap = offset - m_currentPosition;
					const int buffersize = m_buffers.head().size() - m_offset;
					if( buffersize <= gap )
					{
						QByteArray buffer = m_buffers.dequeue();
						m_buffersize -= buffersize;
						m_currentPosition += buffersize;
						m_offset = 0;
					}
					else
					{
						m_buffersize -= gap;
						m_currentPosition += gap;
						m_offset += gap;
					}
				}
				Q_ASSERT( offset == m_currentPosition );
				m_mutex.unlock();
				if( m_buffersize < MAXBUFFERSIZE / 2 && !( m_intstate & StreamAtEndState ) )
				{
					PXINE_VDEBUG << k_funcinfo << "emitting needData" << endl;
					if( m_mainThread == pthread_self() )
						emit needData();
					else
						emit needDataQueued();
				}
				return m_currentPosition;
			}
			// seek before the current position in the buffer
			else if( offset < m_currentPosition && m_currentPosition - offset <= m_offset )
			{
				m_offset -= m_currentPosition - offset;
				m_buffersize += m_currentPosition - offset;
				Q_ASSERT( m_offset >= 0 );
				m_currentPosition = offset;
				m_mutex.unlock();
				return m_currentPosition;
			}
			m_mutex.unlock();

			// throw away the buffers and ask for new data

			if( !isSeekable() )
				return m_currentPosition;

			if( offset > streamSize() )
			{
				kWarning( 610 ) << "stupid xine is asking to seek behind the end of the data stream" << endl;
				return m_currentPosition;
			}

			// called from either main or xine thread
			m_mutex.lock();
			m_buffers.clear();
			m_buffersize = 0;
			m_offset = 0;
			stateTransition( m_intstate - ( m_intstate & StreamAtEndState ) );
			if( m_mainThread == pthread_self() )
			{
				PXINE_VDEBUG << k_funcinfo << offset << endl;
				emit seekStream( qMax( static_cast<qint64>( m_preview.size() ), offset ) );
			}
			else
			{
				PXINE_VDEBUG << k_funcinfo << "from xine thread " << offset << " = " << qulonglong( offset ) << endl;
				::exit( 1 ); // XXX
				m_seekMutex.lock();
				emit seekStreamQueued( qMax( static_cast<qint64>( m_preview.size() ), offset ) ); //calls syncSeekStream from the main thread
				m_seekWaitCondition.wait( &m_seekMutex );
				m_seekMutex.unlock();
			}
			m_currentPosition = offset;
			m_mutex.unlock();
			return m_currentPosition;
		}

		off_t currentPosition() const {
			return m_currentPosition;
		}

void ByteStream::slotSeekStream( qint64 offset )
{
	syncSeekStream( offset );
}

ByteStream::~ByteStream()
{
	PXINE_DEBUG << k_funcinfo << endl;
	stop();
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

void ByteStream::setStreamSize( qint64 x )
{
	PXINE_VDEBUG << k_funcinfo << x << endl;
	m_streamSize = x;
	stateTransition( m_intstate | ByteStream::StreamSizeSetState );
}

void ByteStream::endOfData()
{
	PXINE_VDEBUG << k_funcinfo << endl;
	stateTransition( m_intstate | ByteStream::StreamAtEndState );
}

qint64 ByteStream::streamSize() const
{
	return m_streamSize;
}

void ByteStream::setStreamSeekable( bool seekable )
{
	m_seekable = seekable;
}

bool ByteStream::streamSeekable() const
{
	return m_seekable;
}

bool ByteStream::isSeekable() const
{
	if( m_seekable )
		return AbstractMediaProducer::isSeekable();
	return false;
}

void ByteStream::writeData(const QByteArray &data)
{
    if (data.size() <= 0) {
        return;
    }

			if( !( m_intstate & PreviewReadyState ) )
			{
				PXINE_DEBUG << k_funcinfo << "fill preview" << endl;
				if( m_preview.size() + data.size() > MAX_PREVIEW_SIZE )
				{
					int tocopy = MAX_PREVIEW_SIZE - m_preview.size();
					m_preview += data.left( tocopy );
					QByteArray leftover( data.right( data.size() - tocopy ) );
					m_buffers.enqueue( leftover );
					m_buffersize += leftover.size();
					PXINE_VDEBUG << k_funcinfo << "enqueued " << leftover.size() << " bytes to m_buffers, m_buffersize = " << m_buffersize << endl;
				}
				else
					m_preview += data;

				PXINE_VDEBUG << k_funcinfo << "filled preview buffer to " << m_preview.size() << endl;
				if( m_preview.size() == MAX_PREVIEW_SIZE )
					stateTransition( m_intstate | PreviewReadyState );
				return;
			}
			// always called in the main thread
			Q_ASSERT( m_mainThread == pthread_self() );

			PXINE_VDEBUG << k_funcinfo << data.size() << " m_intstate = " << m_intstate << endl;

			m_mutex.lock();
			m_buffers.enqueue( data );
			m_buffersize += data.size();
			m_mutex.unlock();
			PXINE_VDEBUG << k_funcinfo << "m_buffersize = " << m_buffersize << endl;
			if( m_buffersize > MAXBUFFERSIZE )
			{
				PXINE_VDEBUG << k_funcinfo << "emitting enoughData" << endl;
				emit enoughData();
			}
			m_waitForDataCondition.wakeOne();
		}
}


		void syncSeekStream( qint64 offset ) {
			PXINE_VDEBUG << k_funcinfo << endl;
			m_seekMutex.lock();
			emit seekStream( offset );
			m_seekMutex.unlock();
			m_seekWaitCondition.wakeOne();
		}

		bool canRecreateStream() const {
			// if we still have all the data from the beginning
			return ( ( m_currentPosition <= m_preview.size() || m_currentPosition - m_preview.size() == m_offset )
					// and if that won't change anytime soon
					&& !( m_intstate & PlayingState ) );
			// return true
			// else false
		}

qint64 ByteStream::totalTime() const
{
	delayedInit();

	if( xine_get_status( stream() ) == XINE_STATUS_IDLE )
		const_cast<ByteStream*>( this )->xineOpen();

	int lengthtime = 0;
	if( xine_get_pos_length( stream(), 0, 0, &lengthtime ) == 1 )
		if( lengthtime >= 0 )
			return lengthtime;
	return -1;
}


qint32 ByteStream::aboutToFinishTime() const
{
	PXINE_VDEBUG << k_funcinfo << endl;
	return m_aboutToFinishTime;
}

void ByteStream::setAboutToFinishTime( qint32 newAboutToFinishTime )
{
	PXINE_VDEBUG << k_funcinfo << newAboutToFinishTime << endl;
	m_aboutToFinishTime = newAboutToFinishTime;
	if( m_aboutToFinishTime > 0 )
	{
		const qint64 time = currentTime();
		if( !stream() )
			m_aboutToFinishNotEmitted = true;
		else if( time < totalTime() - m_aboutToFinishTime ) // not about to finish
		{
			m_aboutToFinishNotEmitted = true;
			if( state() == Phonon::PlayingState )
				emitAboutToFinishIn( totalTime() - m_aboutToFinishTime - time );
		}
	}
}

void ByteStream::play()
{
	PXINE_VDEBUG << k_funcinfo << endl;
	if( m_intstate & OpenedState )
	{
		if( state() == PausedState )
			xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_NORMAL );
		else
		{
			if( xine_get_status( stream() ) == XINE_STATUS_IDLE )
				xineOpen();
			xine_play( stream(), 0, 0 );
		}
		AbstractMediaProducer::play(); // goes into PlayingState
		stateTransition( m_intstate | ByteStream::PlayingState );
	}
	else
	{
		if( ( m_intstate & AboutToOpenState ) == AboutToOpenState )
		{
			// the stream was playing before and has been stopped, so try to open it again, if it
			// fails (e.g. because the data stream cannot be seeked back) go into ErrorState
			if( xineOpen() )
				play();
			else
				setState( Phonon::ErrorState );
		}
		else
			setState( Phonon::BufferingState );
	}
}

void ByteStream::stop()
{
	PXINE_VDEBUG << k_funcinfo << endl;

	if( stream() )
	{
		// don't call stateTransition so that xineOpen isn't called automatically
		m_intstate = ( m_intstate & StreamAtEndState ) | AboutToOpenState;

		xine_stop( stream() );
		AbstractMediaProducer::stop();
		m_aboutToFinishNotEmitted = true;
		xine_close( stream() );
	}
}

void ByteStream::seek( qint64 time )
{
	PXINE_VDEBUG << k_funcinfo << endl;
	if( !isSeekable() || !stream() )
		return;

	AbstractMediaProducer::seek( time );

	int totalTime;
	xine_get_pos_length( stream(), 0, 0, &totalTime );

	if( m_aboutToFinishTime > 0 && time < totalTime - m_aboutToFinishTime ) // not about to finish
	{
		m_aboutToFinishNotEmitted = true;
		emitAboutToFinishIn( totalTime - m_aboutToFinishTime - time );
	}
}

void ByteStream::emitTick()
{
	AbstractMediaProducer::emitTick();
	if( m_aboutToFinishNotEmitted && m_aboutToFinishTime > 0 )
	{
		int currentTime = 0;
		int totalTime = 0;

		xine_get_pos_length( stream(), 0, &currentTime, &totalTime );
		const int remainingTime = totalTime - currentTime;
		const int timeToAboutToFinishSignal = remainingTime - m_aboutToFinishTime;
		if( timeToAboutToFinishSignal <= tickInterval() ) // about to finish
		{
			if( timeToAboutToFinishSignal > 100 )
				emitAboutToFinishIn( timeToAboutToFinishSignal );
			else
			{
				m_aboutToFinishNotEmitted = false;
				PXINE_VDEBUG << "emitting aboutToFinish( " << remainingTime << " )" << endl;
				emit aboutToFinish( remainingTime );
			}
		}
	}
}

bool ByteStream::recreateStream()
{
	PXINE_VDEBUG << k_funcinfo << endl;
	if( !stream() )
		return true;
	if( outputPortsNotChanged() )
		return true;

	// store state
	Phonon::State oldstate = state();
	int position;
	xine_get_pos_length( stream(), &position, 0, 0 );

	AbstractMediaProducer::recreateStream();
	// restore state
	m_intstate &= AboutToOpenState | StreamAtEndState;
	if( !xineOpen() )
		return false;
	switch( oldstate )
	{
		case Phonon::PausedState:
			PXINE_VDEBUG << "xine_play" << endl;
			xine_play( stream(), position, 0 );
			PXINE_VDEBUG << "pause" << endl;
			xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
			break;
		case Phonon::PlayingState:
		case Phonon::BufferingState:
			PXINE_VDEBUG << "xine_play" << endl;
			xine_play( stream(), position, 0 );
			break;
		case Phonon::StoppedState:
		case Phonon::LoadingState:
		case Phonon::ErrorState:
			break;
	}
	return true;
}

void ByteStream::reachedPlayingState()
{
	PXINE_VDEBUG << k_funcinfo << endl;
	if( m_aboutToFinishTime > 0 )
	{
		const qint64 time = currentTime();
		emitAboutToFinishIn( totalTime() - m_aboutToFinishTime - time );
	}
}

void ByteStream::leftPlayingState()
{
    m_aboutToFinishNotEmitted = true;
    if (m_aboutToFinishTimer) {
        m_aboutToFinishTimer->stop();
    }
    AbstractMediaProducer::leftPlayingState();
}

void ByteStream::stateTransition( int newState )
{
	if( m_intstate == newState )
		return;

	PXINE_VDEBUG << k_funcinfo << newState << endl;
		{
			// if the StreamAtEndState was set
			if( ( newState & StreamAtEndState ) && !( m_intstate & StreamAtEndState ) )
			{
				m_waitForDataCondition.wakeOne();
//X 				m_mutex.lock();
//X 				setStreamSize( m_currentPosition + m_buffersize );
//X 				m_mutex.unlock();
			}
			// if the OpenedState was unset
			else if( !( newState & OpenedState ) && ( m_intstate & OpenedState ) )
			{
				m_waitForDataCondition.wakeOne();
			}
			m_intstate = newState;
		}
	switch( newState )
	{
		case AboutToOpenState:
		case AboutToOpenState | StreamAtEndState:
			QTimer::singleShot( 0, this, SLOT( xineOpen() ) );
			break;
		default:
			break;
	}
}

void ByteStream::emitAboutToFinishIn( int timeToAboutToFinishSignal )
{
	PXINE_VDEBUG << k_funcinfo << timeToAboutToFinishSignal << endl;
	Q_ASSERT( m_aboutToFinishTime > 0 );
	if( !m_aboutToFinishTimer )
	{
		m_aboutToFinishTimer = new QTimer( this );
		m_aboutToFinishTimer->setSingleShot( true );
		connect( m_aboutToFinishTimer, SIGNAL( timeout() ), SLOT( emitAboutToFinish() ) );
	}
	m_aboutToFinishTimer->start( timeToAboutToFinishSignal );
}

void ByteStream::emitAboutToFinish()
{
	PXINE_VDEBUG << k_funcinfo << endl;
	if( m_aboutToFinishNotEmitted )
	{
		int currentTime = 0;
		int totalTime = 0;

		xine_get_pos_length( stream(), 0, &currentTime, &totalTime );
		const int remainingTime = totalTime - currentTime;

		if( remainingTime <= m_aboutToFinishTime + 150 )
		{
			m_aboutToFinishNotEmitted = false;
			PXINE_VDEBUG << "emitting aboutToFinish( " << remainingTime << " )" << endl;
			emit aboutToFinish( remainingTime );
		}
		else
		{
			PXINE_VDEBUG << "not yet" << endl;
			emitAboutToFinishIn( totalTime - m_aboutToFinishTime - remainingTime );
		}
	}
}

}} //namespace Phonon::Xine

#include "bytestream.moc"
// vim: sw=4 ts=4
