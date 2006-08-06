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
{
	//kDebug() << k_funcinfo << endl;

	m_xine_engine = xe;

}

MediaObject::~MediaObject()
{
	//kDebug() << k_funcinfo << endl;
}

KUrl MediaObject::url() const
{
	//kDebug() << k_funcinfo << endl;
	return m_url;
}

qint64 MediaObject::totalTime() const
{
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
	//kDebug() << k_funcinfo << endl;
	return m_aboutToFinishTime;
}

void MediaObject::setUrl( const KUrl& url )
{
	//kDebug() << k_funcinfo << endl;
	stop();
	m_url = url;
	kDebug() << "url = " << m_url.url() << endl;
	xine_open( stream(), m_url.url().toUtf8() );
	emit length( totalTime() );
	updateMetaData();
}

void MediaObject::setAboutToFinishTime( qint32 newAboutToFinishTime )
{
	//kDebug() << k_funcinfo << endl;
	m_aboutToFinishTime = newAboutToFinishTime;
	if( currentTime() < totalTime() - m_aboutToFinishTime ) // not about to finish
		m_aboutToFinishNotEmitted = true;
}

void MediaObject::play()
{
	//kDebug() << k_funcinfo << endl;

	if( state() == PausedState )
		xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_NORMAL );
	else
		xine_play( stream(), 0, 0 );
	AbstractMediaProducer::play();
}

void MediaObject::pause()
{
	//kDebug() << k_funcinfo << endl;
	if( state() == PlayingState || state() == BufferingState )
	{
		xine_set_param( stream(), XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
		AbstractMediaProducer::pause();
	}
}

void MediaObject::stop()
{
	//kDebug() << k_funcinfo << endl;

	xine_stop( stream() );
	AbstractMediaProducer::stop();
	m_aboutToFinishNotEmitted = true;
}

void MediaObject::seek( qint64 time )
{
	//kDebug() << k_funcinfo << endl;
	if( !isSeekable() )
		return;

	//xine_trick_mode( m_xine_engine->m_stream, XINE_TRICK_MODE_SEEK_TO_TIME, time );

	AbstractMediaProducer::seek( time );

	int tmp;
	int timeAfter;
	int totalTime;
	xine_get_pos_length( stream(), &tmp, &timeAfter, &totalTime );

	if( timeAfter < totalTime - m_aboutToFinishTime ) // not about to finish
		m_aboutToFinishNotEmitted = true;
}

void MediaObject::emitTick()
{
	AbstractMediaProducer::emitTick();
	int tmp = 0;
	int currentTime = 0;
	int totalTime = 0;

	xine_get_pos_length( stream(), &tmp, &currentTime, &totalTime );
	const int remainingTime = totalTime - currentTime;
	const int timeToAboutToFinishSignal = remainingTime - m_aboutToFinishTime;
	if( timeToAboutToFinishSignal <= tickInterval() ) // about to finish
	{
		if( timeToAboutToFinishSignal > 40 )
			QTimer::singleShot( timeToAboutToFinishSignal, this, SLOT( emitAboutToFinish() ) );
		else if( m_aboutToFinishNotEmitted )
		{
			m_aboutToFinishNotEmitted = false;
			emit aboutToFinish( remainingTime );
		}
	}
}

void MediaObject::emitAboutToFinish()
{
	if( m_aboutToFinishNotEmitted )
	{
		m_aboutToFinishNotEmitted = false;
		emit aboutToFinish( totalTime() - currentTime() );
	}
}

bool MediaObject::event( QEvent* ev )
{
	switch( ev->type() )
	{
		case Xine::MediaFinishedEvent:
			AbstractMediaProducer::stop();
			m_aboutToFinishNotEmitted = true;
			emit finished();
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
