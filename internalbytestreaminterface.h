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

#ifndef INTERNALBYTESTREAMDATA_H
#define INTERNALBYTESTREAMDATA_H

#include <QByteArray>
#include <QQueue>
#include <kdebug.h>
#include <QCoreApplication>
#include <QMutex>
#include <QWaitCondition>
#include <pthread.h>
#include <cstdlib>

static const size_t MAXBUFFERSIZE = 1024 * 128; // 128kB

class InternalByteStreamInterface
{
	public:
		InternalByteStreamInterface() : m_atEnd( false ) {
			// created in the main thread
			m_mainThread = pthread_self();
		}

		virtual ~InternalByteStreamInterface() {
		}

		void pushBuffer( const QByteArray& data ) {
			// always called in the main thread
			kDebug() << k_funcinfo << data.size() << ' ' << pthread_self() << endl;// << data.left( 4 ).constData() << endl;

			m_mutex.lock();
			m_buffers.enqueue( data );
			m_buffersize += data.size();
			m_mutex.unlock();
			kDebug() << k_funcinfo << "m_buffersize = " << m_buffersize << endl;
			if( m_buffersize > MAXBUFFERSIZE )
			{
				kDebug() << k_funcinfo << "emitting enoughData" << endl;
				emit enoughData();
			}
			m_waitForDataCondition.wakeOne();
		}

		void pullBuffer( char *buf, int len ) {
			// called from either main or xine thread
			kDebug() << k_funcinfo << endl;
			while( len > 0 )
			{
				m_mutex.lock();
				if( m_buffers.head().size() - m_offset < len )
				{
					QByteArray buffer = m_buffers.dequeue();
					int tocopy = buffer.size() - m_offset;
					memcpy( buf, buffer.constData() + m_offset, tocopy );
					buf += tocopy;
					len -= tocopy;
					m_buffersize -= tocopy;
					m_offset = 0;
				}
				else
				{
					QByteArray &buffer = m_buffers.head();
					memcpy( buf, buffer.constData() + m_offset , len );
					m_offset += len;
					m_buffersize -= len;
					len = 0;
				}
				m_mutex.unlock();
			}
			if( m_buffersize < MAXBUFFERSIZE / 2 )
			{
				kDebug() << k_funcinfo << "emitting needData" << endl;
				if( m_mainThread == pthread_self() )
					emit needData();
				else
					emit needDataQueued();
			}
		}

		qint64 readFromBuffer( void *buf, size_t count ) {
			// called from either main or xine thread
			if( m_atEnd )
				return 0;

			kDebug() << k_funcinfo << count << " called from thread " << pthread_self() << endl;
			size_t oldbuffersize = 0;

			/* get data while more is needed and while we're still receiving data */
			while( m_buffersize < count && !m_atEnd ) // && m_buffersize > oldbuffersize )
			{
				oldbuffersize = m_buffersize;

				// if it calls from the main thread processEvents has to be called to block until
				// the data has arrived
				// if it calls from a different thread, the thread needs to sleep until a wait
				// condition is signalled from pushBuffer

				if( m_mainThread == pthread_self() )
				{
					kDebug() << k_funcinfo << "wait in main thread" << endl;
					while( m_buffersize == oldbuffersize && !m_atEnd )
					{
						//kDebug() << k_funcinfo << "waiting: " << m_buffersize << ", " << oldbuffersize << ", " << m_atEnd << endl;
						emit needData();
						QCoreApplication::processEvents( QEventLoop::ExcludeUserInputEvents );
					}
				}
				else
				{
					kDebug() << k_funcinfo << "wait in xine thread" << endl;
					m_mutex.lock();
					while( m_buffersize == oldbuffersize && !m_atEnd )
					{
						emit needDataQueued();
						m_waitForDataCondition.wait( &m_mutex, 50 );
					}
					m_mutex.unlock();
				}
				//kDebug() << "m_buffersize = " << m_buffersize << endl;
			}
			if( m_buffersize >= count )
			{
				pullBuffer( static_cast<char*>( buf ), count );
				return count;
			}
			else if( m_buffersize > 0 )
			{
				size_t tmp = m_buffersize;
				pullBuffer( static_cast<char*>( buf ), m_buffersize );
				return tmp;
			}
			return 0;
		}

		void seek( qint64 offset )
		{
			// called from either main or xine thread
			m_mutex.lock();
			m_buffers.clear();
			m_buffersize = 0;
			m_offset = 0;
			m_atEnd = false;
			if( m_mainThread == pthread_self() )
			{
				kDebug() << k_funcinfo << "from main thread " << offset << " = " << qulonglong( offset ) << endl;
				emit seekStream( offset );
			}
			else
			{
				kDebug() << k_funcinfo << "from xine thread " << offset << " = " << qulonglong( offset ) << endl;
				::exit( 1 ); // XXX
				m_seekMutex.lock();
				emit seekStreamQueued( offset ); //calls syncSeekStream from the main thread
				m_seekWaitCondition.wait( &m_seekMutex );
				m_seekMutex.unlock();
			}
			m_mutex.unlock();
		}

		virtual qint64 streamSize() const = 0;
		virtual bool isSeekable() const = 0;

	protected:
		virtual void needData() = 0;
		virtual void needDataQueued() = 0;
		virtual void enoughData() = 0;
		virtual void seekStream( qint64 ) = 0;
		virtual void seekStreamQueued( qint64 ) = 0;
		void syncSeekStream( qint64 offset ) {
			kDebug() << k_funcinfo << endl;
			m_seekMutex.lock();
			emit seekStream( offset );
			m_seekMutex.unlock();
			m_seekWaitCondition.wakeOne();
		}

		bool m_atEnd;

	private:
		QMutex m_mutex;
		QMutex m_seekMutex;
		QWaitCondition m_waitForDataCondition;
		QWaitCondition m_seekWaitCondition;

		size_t m_buffersize;
		int m_offset;
		QQueue<QByteArray> m_buffers;
		pthread_t m_mainThread;
};

#endif // INTERNALBYTESTREAMDATA_H
