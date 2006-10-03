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
}

extern plugin_info_t kbytestream_xine_plugin_info[];

namespace Phonon
{
namespace Xine
{
ByteStream::ByteStream( QObject* parent )
	: AbstractMediaProducer( parent )
	, m_aboutToFinishNotEmitted( true )
	, m_seekable( false )
	, m_aboutToFinishTimer( 0 )
	, m_streamSize( 0 )
{
	//kDebug( 610 ) << k_funcinfo << endl;
	connect( this, SIGNAL( needDataQueued() ), this, SIGNAL( needData() ), Qt::QueuedConnection );
	connect( this, SIGNAL( seekStreamQueued( qint64 ) ), this, SLOT( slotSeekStream( qint64 ) ), Qt::QueuedConnection );
	xine_register_plugins( XineEngine::xine(), kbytestream_xine_plugin_info );
}

void ByteStream::slotSeekStream( qint64 offset )
{
	syncSeekStream( offset );
}

ByteStream::~ByteStream()
{
	//kDebug( 610 ) << k_funcinfo << endl;
	stop();
}

void ByteStream::xineOpen()
{
	if( ( m_intstate != AboutToOpenState ) && ( m_intstate != ( AboutToOpenState | StreamAtEndState ) ) )
	{
		kDebug( 610 ) << k_funcinfo << "not ready yet!" << endl;
		return;
	}

	kDebug( 610 ) << k_funcinfo << m_intstate << endl;

	QByteArray mrl( "kbytestream:/" );
	// the address can contain 0s which will null-terminate the C-string
	// use a simple encoding: 0x00 -> 0x0101, 0x01 -> 0x0102
	const InternalByteStreamInterface* iface = this;
	const unsigned char *that = reinterpret_cast<const unsigned char*>( &iface );
	for( unsigned int i = 0; i < sizeof( void* ); ++i )
	{
		if( *that <= 1 )
		{
			mrl += 0x01;
			if( *that == 1 )
				mrl += 0x02;
			else
				mrl += 0x01;
		}
		else
			mrl += *that;

		++that;
	}
	xine_open( stream(), mrl.constData() );
	emit length( totalTime() );
	updateMetaData();
	stateTransition( InternalByteStreamInterface::OpenedState );
}

void ByteStream::setStreamSize( qint64 x )
{
	kDebug( 610 ) << k_funcinfo << x << endl;
	m_streamSize = x;
	stateTransition( m_intstate | InternalByteStreamInterface::StreamSizeSetState );
}

void ByteStream::endOfData()
{
	kDebug( 610 ) << k_funcinfo << endl;
	stateTransition( m_intstate | InternalByteStreamInterface::StreamAtEndState );
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
	return m_seekable;
}

void ByteStream::writeData( const QByteArray& data )
{
	pushBuffer( data );
}

qint64 ByteStream::totalTime() const
{
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
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_aboutToFinishTime;
}

void ByteStream::setAboutToFinishTime( qint32 newAboutToFinishTime )
{
	kDebug( 610 ) << k_funcinfo << newAboutToFinishTime << endl;
	m_aboutToFinishTime = newAboutToFinishTime;
	if( m_aboutToFinishTime > 0 )
	{
		const qint64 time = currentTime();
		if( time < totalTime() - m_aboutToFinishTime ) // not about to finish
		{
			m_aboutToFinishNotEmitted = true;
			if( state() == Phonon::PlayingState )
				emitAboutToFinishIn( totalTime() - m_aboutToFinishTime - time );
		}
	}
}

void ByteStream::play()
{
	//kDebug( 610 ) << k_funcinfo << endl;

	if( state() == PausedState )
		xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_NORMAL );
	else
	{
		if( xine_get_status( stream() ) == XINE_STATUS_IDLE )
			xineOpen();
		xine_play( stream(), 0, 0 );
	}
	AbstractMediaProducer::play();
}

void ByteStream::pause()
{
	//kDebug( 610 ) << k_funcinfo << endl;
	if( state() == Phonon::PlayingState || state() == Phonon::BufferingState )
	{
		xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
		AbstractMediaProducer::pause();
	}
}

void ByteStream::stop()
{
	//kDebug( 610 ) << k_funcinfo << endl;

	xine_stop( stream() );
	AbstractMediaProducer::stop();
	m_aboutToFinishNotEmitted = true;
	xine_close( stream() );

	// don't call stateTransition so that xineOpen isn't called automatically
	m_intstate = InternalByteStreamInterface::AboutToOpenState;
}

void ByteStream::seek( qint64 time )
{
	//kDebug( 610 ) << k_funcinfo << endl;
	if( !isSeekable() )
		return;

	AbstractMediaProducer::seek( time );

	int timeAfter;
	int totalTime;
	xine_get_pos_length( stream(), 0, &timeAfter, &totalTime );
	//kDebug( 610 ) << k_funcinfo << "time after seek: " << timeAfter << endl;
	// xine_get_pos_length doesn't work immediately after seek :(
	timeAfter = time;

	if( m_aboutToFinishTime > 0 && timeAfter < totalTime - m_aboutToFinishTime ) // not about to finish
	{
		m_aboutToFinishNotEmitted = true;
		emitAboutToFinishIn( totalTime - m_aboutToFinishTime - timeAfter );
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
				kDebug( 610 ) << "emitting aboutToFinish( " << remainingTime << " )" << endl;
				emit aboutToFinish( remainingTime );
			}
		}
	}
}

void ByteStream::recreateStream()
{
	kDebug( 610 ) << k_funcinfo << endl;

	// store state
	Phonon::State oldstate = state();
	int position;
	xine_get_pos_length( stream(), &position, 0, 0 );

	AbstractMediaProducer::recreateStream();
	// restore state
	xineOpen();
	switch( oldstate )
	{
		case Phonon::PausedState:
			kDebug( 610 ) << "xine_play" << endl;
			xine_play( stream(), position, 0 );
			kDebug( 610 ) << "pause" << endl;
			xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
			break;
		case Phonon::PlayingState:
		case Phonon::BufferingState:
			kDebug( 610 ) << "xine_play" << endl;
			xine_play( stream(), position, 0 );
			break;
		case Phonon::StoppedState:
		case Phonon::LoadingState:
		case Phonon::ErrorState:
			break;
	}
}

void ByteStream::reachedPlayingState()
{
	kDebug( 610 ) << k_funcinfo << endl;
	if( m_aboutToFinishTime > 0 )
	{
		const qint64 time = currentTime();
		emitAboutToFinishIn( totalTime() - m_aboutToFinishTime - time );
	}
}

void ByteStream::leftPlayingState()
{
	kDebug( 610 ) << k_funcinfo << endl;
	if( m_aboutToFinishTimer )
		m_aboutToFinishTimer->stop();
}

void ByteStream::stateTransition( int newState )
{
	if( m_intstate == newState )
		return;

	kDebug() << k_funcinfo << newState << endl;
	InternalByteStreamInterface::stateTransition( newState );
	switch( newState )
	{
		case InternalByteStreamInterface::AboutToOpenState:
			QTimer::singleShot( 0, this, SLOT( xineOpen() ) );
			break;
		default:
			break;
	}
}

void ByteStream::emitAboutToFinishIn( int timeToAboutToFinishSignal )
{
	kDebug( 610 ) << k_funcinfo << timeToAboutToFinishSignal << endl;
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
	kDebug( 610 ) << k_funcinfo << endl;
	if( m_aboutToFinishNotEmitted )
	{
		int currentTime = 0;
		int totalTime = 0;

		xine_get_pos_length( stream(), 0, &currentTime, &totalTime );
		const int remainingTime = totalTime - currentTime;

		if( remainingTime <= m_aboutToFinishTime + 150 )
		{
			m_aboutToFinishNotEmitted = false;
			kDebug( 610 ) << "emitting aboutToFinish( " << remainingTime << " )" << endl;
			emit aboutToFinish( remainingTime );
		}
		else
		{
			kDebug( 610 ) << "not yet" << endl;
			emitAboutToFinishIn( totalTime - m_aboutToFinishTime - remainingTime );
		}
	}
}

bool ByteStream::event( QEvent* ev )
{
	kDebug( 610 ) << k_funcinfo << endl;
	switch( ev->type() )
	{
		case Xine::MediaFinishedEvent:
			AbstractMediaProducer::stop();
			m_aboutToFinishNotEmitted = true;
			if( videoPath() )
				videoPath()->streamFinished();
			kDebug( 610 ) << "emit finished()" << endl;
			emit finished();
			xine_close( stream() );
			ev->accept();
			return true;
		default:
			break;
	}
	return AbstractMediaProducer::event( ev );
}

}} //namespace Phonon::Xine

#include "bytestream.moc"
// vim: sw=4 ts=4 noet
