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

	xine_event_create_listener_thread( m_xine_engine->m_eventQueue = xine_event_new_queue( m_xine_engine->m_stream ), &m_xine_engine->xineEventListener, (void*)this );

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
	//kDebug() << k_funcinfo << endl;

	int positionstream = 0;
	int positiontime = 0;
	int lengthtime = 0;

	if( xine_get_pos_length( m_xine_engine->m_stream, &positionstream, &positiontime, &lengthtime ) == 1 )
	{
		return lengthtime;
	}
	else
		return 0;
}

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
	xine_open( m_xine_engine->m_stream, m_url.url().toUtf8() );
	emit length( totalTime() );
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

	xine_play( m_xine_engine->m_stream, 0, 0 );
	AbstractMediaProducer::play();
}

void MediaObject::pause()
{
	//kDebug() << k_funcinfo << endl;
	if( state() == PlayingState || state() == BufferingState )
	{
		AbstractMediaProducer::pause();
	}
}

void MediaObject::stop()
{
	//kDebug() << k_funcinfo << endl;

	xine_stop( m_xine_engine->m_stream );
	AbstractMediaProducer::stop();
	m_aboutToFinishNotEmitted = true;
}

void MediaObject::seek( qint64 time )
{
	//kDebug() << k_funcinfo << endl;

	//xine_trick_mode( m_xine_engine->m_stream, XINE_TRICK_MODE_SEEK_TO_TIME, time );

	AbstractMediaProducer::seek( time );

	if( currentTime() < totalTime() - m_aboutToFinishTime ) // not about to finish
		m_aboutToFinishNotEmitted = true;
}

void MediaObject::emitTick()
{
	AbstractMediaProducer::emitTick();
	if( currentTime() >= totalTime() - m_aboutToFinishTime ) // about to finish
	{
		if( m_aboutToFinishNotEmitted )
		{
			m_aboutToFinishNotEmitted = false;
			emit aboutToFinish( totalTime() - currentTime() );
		}
	}
	if( currentTime() >= totalTime() ) // finished
	{
		stop();
		emit finished();
	}
}

}}

#include "mediaobject.moc"
// vim: sw=4 ts=4 noet
