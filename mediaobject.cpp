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

#include "mediaobject.h"
#include <QTimer>
#include <kdebug.h>

#include "xine_engine.h"
#include <QEvent>

namespace Phonon
{
namespace Xine
{
MediaObject::MediaObject( QObject* parent, XineEngine* xe )
	: AbstractMediaProducer( parent, xe )
	, m_aboutToFinishNotEmitted( true )
	, m_aboutToFinishTimer( 0 )
{
	//kDebug( 610 ) << k_funcinfo << endl;

	m_xine_engine = xe;
}

MediaObject::~MediaObject()
{
	//kDebug( 610 ) << k_funcinfo << endl;
}

KUrl MediaObject::url() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_url;
}

qint64 MediaObject::totalTime() const
{
	if( xine_get_status( stream() ) == XINE_STATUS_IDLE && m_url.isValid() )
		xine_open( stream(), m_url.url().toUtf8() );

	int positionstream = 0;
	int positiontime = 0;
	int lengthtime = 0;

	if( xine_get_pos_length( stream(), &positionstream, &positiontime, &lengthtime ) == 1 )
		if( lengthtime >= 0 )
			return lengthtime;
	return -1;
}

/*
qint64 MediaObject::remainingTime() const
{
	int positionstream = 0;
	int positiontime = 0;
	int lengthtime = 0;

	if( xine_get_pos_length( stream(), &positionstream, &positiontime, &lengthtime ) == 1 )
		if( lengthtime - positiontime > 0 )
			return lengthtime - positiontime;
	return 0;
}
*/

qint32 MediaObject::aboutToFinishTime() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_aboutToFinishTime;
}

void MediaObject::setUrl( const KUrl& url )
{
	//kDebug( 610 ) << k_funcinfo << endl;
	stop();
	m_url = url;
	kDebug( 610 ) << "url = " << m_url.url() << endl;
	xine_open( stream(), m_url.url().toUtf8() );
	emit length( totalTime() );
	updateMetaData();
}

void MediaObject::setAboutToFinishTime( qint32 newAboutToFinishTime )
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

void MediaObject::play()
{
	//kDebug( 610 ) << k_funcinfo << endl;

	if( state() == PausedState )
		xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_NORMAL );
	else
	{
		if( xine_get_status( stream() ) == XINE_STATUS_IDLE )
			xine_open( stream(), m_url.url().toUtf8() );
		xine_play( stream(), 0, 0 );
	}
	AbstractMediaProducer::play();
}

void MediaObject::pause()
{
	//kDebug( 610 ) << k_funcinfo << endl;
	if( state() == PlayingState || state() == BufferingState )
	{
		xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
		AbstractMediaProducer::pause();
	}
}

void MediaObject::stop()
{
	//kDebug( 610 ) << k_funcinfo << endl;

	xine_stop( stream() );
	AbstractMediaProducer::stop();
	m_aboutToFinishNotEmitted = true;
	xine_close( stream() );
}

void MediaObject::seek( qint64 time )
{
	//kDebug( 610 ) << k_funcinfo << endl;
	if( !isSeekable() )
		return;

	AbstractMediaProducer::seek( time );

	int tmp;
	int timeAfter;
	int totalTime;
	xine_get_pos_length( stream(), &tmp, &timeAfter, &totalTime );
	//kDebug( 610 ) << k_funcinfo << "time after seek: " << timeAfter << endl;
	// xine_get_pos_length doesn't work immediately after seek :(
	timeAfter = time;

	if( m_aboutToFinishTime > 0 && timeAfter < totalTime - m_aboutToFinishTime ) // not about to finish
	{
		m_aboutToFinishNotEmitted = true;
		emitAboutToFinishIn( totalTime - m_aboutToFinishTime - timeAfter );
	}
}

void MediaObject::emitTick()
{
	AbstractMediaProducer::emitTick();
	if( m_aboutToFinishNotEmitted && m_aboutToFinishTime > 0 )
	{
		int tmp = 0;
		int currentTime = 0;
		int totalTime = 0;

		xine_get_pos_length( stream(), &tmp, &currentTime, &totalTime );
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

void MediaObject::recreateStream()
{
	kDebug( 610 ) << k_funcinfo << endl;

	// store state
	Phonon::State oldstate = state();
	int position;
	int tmp1, tmp2;
	xine_get_pos_length( stream(), &position, &tmp1, &tmp2 );

	AbstractMediaProducer::recreateStream();
	// restore state
	kDebug( 610 ) << "xine_open URL: " << m_url << endl;
	xine_open( stream(), m_url.url().toUtf8() );
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

void MediaObject::reachedPlayingState()
{
	kDebug( 610 ) << k_funcinfo << endl;
	if( m_aboutToFinishTime > 0 )
	{
		const qint64 time = currentTime();
		emitAboutToFinishIn( totalTime() - m_aboutToFinishTime - time );
	}
}

void MediaObject::leftPlayingState()
{
	kDebug( 610 ) << k_funcinfo << endl;
	if( m_aboutToFinishTimer )
		m_aboutToFinishTimer->stop();
}

void MediaObject::emitAboutToFinishIn( int timeToAboutToFinishSignal )
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

void MediaObject::emitAboutToFinish()
{
	kDebug( 610 ) << k_funcinfo << endl;
	if( m_aboutToFinishNotEmitted )
	{
		int tmp = 0;
		int currentTime = 0;
		int totalTime = 0;

		xine_get_pos_length( stream(), &tmp, &currentTime, &totalTime );
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

bool MediaObject::event( QEvent* ev )
{
	kDebug( 610 ) << k_funcinfo << endl;
	switch( ev->type() )
	{
		case Xine::MediaFinishedEvent:
			AbstractMediaProducer::stop();
			m_aboutToFinishNotEmitted = true;
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


}}

#include "mediaobject.moc"
// vim: sw=4 ts=4 noet
