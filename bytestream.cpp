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
	PXINE_DEBUG << k_funcinfo << endl;
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
	PXINE_DEBUG << k_funcinfo << endl;
	stop();
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

	if( !isSeekable() )
	{
		const off_t oldPos = currentPosition();
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
	stateTransition( m_intstate | InternalByteStreamInterface::StreamSizeSetState );
}

void ByteStream::endOfData()
{
	PXINE_VDEBUG << k_funcinfo << endl;
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
	if( m_seekable )
		return AbstractMediaProducer::isSeekable();
	return false;
}

void ByteStream::writeData( const QByteArray& data )
{
	pushBuffer( data );
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
		stateTransition( m_intstate | InternalByteStreamInterface::PlayingState );
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

void ByteStream::pause()
{
	PXINE_VDEBUG << k_funcinfo << endl;
	if( state() == Phonon::PlayingState || state() == Phonon::BufferingState )
	{
		xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
		AbstractMediaProducer::pause();
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
	PXINE_VDEBUG << k_funcinfo << endl;
	if( m_aboutToFinishTimer )
		m_aboutToFinishTimer->stop();
}

void ByteStream::stateTransition( int newState )
{
	if( m_intstate == newState )
		return;

	PXINE_VDEBUG << k_funcinfo << newState << endl;
	InternalByteStreamInterface::stateTransition( newState );
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

bool ByteStream::event( QEvent* ev )
{
	PXINE_VDEBUG << k_funcinfo << endl;
	switch( ev->type() )
	{
		case Xine::MediaFinishedEvent:
			AbstractMediaProducer::stop();
			m_aboutToFinishNotEmitted = true;
			if( videoPath() )
				videoPath()->streamFinished();
			PXINE_VDEBUG << "emit finished()" << endl;
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
// vim: sw=4 ts=4
