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

#include "abstractmediaproducer.h"
#include <QTimer>
#include "videopath.h"
#include "audiopath.h"
#include <kdebug.h>
#include <QVector>
#include <cmath>
#include <QFile>
#include <QByteArray>
#include <QStringList>
#include <QMultiMap>
#include <QEvent>
#include <QtDebug>

namespace Phonon
{
namespace Xine
{
AbstractMediaProducer::AbstractMediaProducer( QObject* parent )
	: QObject( parent )
	, m_stream( 0 )
	, m_event_queue( 0 )
	, m_state( Phonon::LoadingState )
	, m_tickTimer( new QTimer( this ) )
	, m_startTime( -1 )
	, m_audioPath( 0 )
	, m_videoPath( 0 )
	, m_currentTimeOverride( -1 )
{
	connect( m_tickTimer, SIGNAL( timeout() ), SLOT( emitTick() ) );
	createStream();
}

AbstractMediaProducer::~AbstractMediaProducer()
{
	xine_event_dispose_queue( m_event_queue );
	xine_dispose( m_stream );
}

void AbstractMediaProducer::checkAudioOutput()
{
	recreateStream();
	if( m_audioPath->hasOutput() )
		xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 0 );
	else
		xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 1 );
}

void AbstractMediaProducer::checkVideoOutput()
{
	recreateStream();
	if( m_videoPath->hasOutput() )
		xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 0 );
	else
		xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
}

void AbstractMediaProducer::createStream()
{
	xine_audio_port_t *audioPort = NULL;
	xine_video_port_t *videoPort = NULL;
	if( m_audioPath )
	{
		kDebug( 610 ) << "getting audioPort from m_audioPath" << endl;
		audioPort = m_audioPath->audioPort();
	}
	if( m_videoPath )
	{
		kDebug( 610 ) << "getting videoPort from m_videoPath" << endl;
		videoPort = m_videoPath->videoPort();
	}
	kDebug( 610 ) << "XXXXXXXXXXXXXX xine_stream_new( " << ( void* )XineEngine::xine() << ", " << ( void* )audioPort << ", " << ( void* )videoPort << " );" << endl;
	m_stream = xine_stream_new( XineEngine::xine(), audioPort, videoPort );
	m_event_queue = xine_event_new_queue( m_stream );
	xine_event_create_listener_thread( m_event_queue, &XineEngine::self()->xineEventListener, (void*)this );
	xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 1 );
	xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
}

void AbstractMediaProducer::recreateStream()
{
	// save state
	int params[ 33 ];
	for( int i = 1; i < 33; ++i )
		params[ i ] = xine_get_param( m_stream, i );

	// dispose of old xine objects
	xine_event_dispose_queue( m_event_queue );
	xine_dispose( m_stream );
	
	// create new xine objects
	createStream();

	// restore state
	//for( int i = 1; i < 33; ++i )
		//xine_set_param( m_stream, i, params[ i ] );
}

bool AbstractMediaProducer::event( QEvent* ev )
{
	switch( ev->type() )
	{
		case Xine::NewMetaDataEvent:
			updateMetaData();
			ev->accept();
			return true;
		case Xine::ProgressEvent:
			{
				XineProgressEvent* e = static_cast<XineProgressEvent*>( ev );
				if( e->percent() < 100 )
				{
					m_tickTimer->stop();
					setState( Phonon::BufferingState );
				}
				else
				{
					m_tickTimer->start();
					setState( Phonon::PlayingState );
					QTimer::singleShot( 20, this, SLOT( getStartTime() ) );
				}
			}
			return true;
		default:
			break;
	}
	return QObject::event( ev );
}

void AbstractMediaProducer::getStartTime()
{
	if( m_startTime == -1 || m_startTime == 0 )
	{
		int total;
		if( xine_get_pos_length( stream(), 0, &m_startTime, &total ) == 1 )
		{
			if( total > 0 && m_startTime < total && m_startTime >= 0 )
				m_startTime = -1;
		}
		else
			m_startTime = -1;
	}
	if( m_startTime == -1 || m_startTime == 0 )
		QTimer::singleShot( 30, this, SLOT( getStartTime() ) );
}

void AbstractMediaProducer::updateMetaData()
{
	QMultiMap<QString, QString> metaDataMap;
	metaDataMap.insert( QLatin1String( "TITLE"  ),
			QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_TITLE  ) ) );
	metaDataMap.insert( QLatin1String( "ARTIST" ),
			QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_ARTIST ) ) );
	metaDataMap.insert( QLatin1String( "GENRE" ),
			QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_GENRE ) ) );
	metaDataMap.insert( QLatin1String( "ALBUM" ),
			QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_ALBUM ) ) );
	metaDataMap.insert( QLatin1String( "DATE" ),
			QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_YEAR ) ) );
	metaDataMap.insert( QLatin1String( "TRACKNUMBER" ),
			QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_TRACK_NUMBER ) ) );
	metaDataMap.insert( QLatin1String( "DESCRIPTION" ),
			QString::fromUtf8( xine_get_meta_info( m_stream, XINE_META_INFO_COMMENT ) ) );
	if( metaDataMap == m_metaDataMap )
		return;
	qDebug() << metaDataMap;
	m_metaDataMap = metaDataMap;
	emit metaDataChanged( m_metaDataMap );
}

bool AbstractMediaProducer::addVideoPath( QObject* videoPath )
{
	if( m_videoPath )
		return false;
	m_videoPath = qobject_cast<VideoPath*>( videoPath );
	Q_ASSERT( m_videoPath );
	recreateStream();
	if( m_videoPath->hasOutput() )
		xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 0 );
	else
		xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
	return true;
}

bool AbstractMediaProducer::addAudioPath( QObject* audioPath )
{
	if( m_audioPath )
		return false;
	m_audioPath = qobject_cast<AudioPath*>( audioPath );
	Q_ASSERT( m_audioPath );
	m_audioPath->addMediaProducer( this );
	recreateStream();
	if( m_audioPath->hasOutput() )
		xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 0 );
	else
		xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 1 );
	return true;
}

void AbstractMediaProducer::removeVideoPath( QObject* videoPath )
{
	Q_ASSERT( videoPath );
	if( m_videoPath == qobject_cast<VideoPath*>( videoPath ) )
	{
		m_videoPath = 0;
		recreateStream();
		xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
	}
}

void AbstractMediaProducer::removeAudioPath( QObject* audioPath )
{
	Q_ASSERT( audioPath );
	if( m_audioPath == qobject_cast<AudioPath*>( audioPath ) )
	{
		m_audioPath->removeMediaProducer( this );
		m_audioPath = 0;
		recreateStream();
		xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 1 );
	}
}

State AbstractMediaProducer::state() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_state;
}

bool AbstractMediaProducer::hasVideo() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return xine_get_stream_info( m_stream, XINE_STREAM_INFO_HAS_VIDEO );
}

bool AbstractMediaProducer::isSeekable() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return xine_get_stream_info( m_stream, XINE_STREAM_INFO_SEEKABLE );
}

qint64 AbstractMediaProducer::currentTime() const
{
	switch( state() )
	{
		case Phonon::PausedState:
		case Phonon::BufferingState:
		case Phonon::PlayingState:
			{
				int positiontime;
				if( xine_get_pos_length( stream(), 0, &positiontime, 0 ) == 1 )
				{
					if( m_currentTimeOverride != -1 )
					{
						if( positiontime > 0 )
							m_currentTimeOverride = -1;
						else
							positiontime = m_currentTimeOverride;
					}
				}
				else
				{
					positiontime = m_currentTimeOverride;
				}
				if( m_startTime == -1 )
					return positiontime;
				else
					return positiontime - m_startTime;
			}
			break;
		case Phonon::StoppedState:
		case Phonon::LoadingState:
			return 0;
		case Phonon::ErrorState:
			break;
	}
	return -1;
}

qint32 AbstractMediaProducer::tickInterval() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_tickInterval;
}

void AbstractMediaProducer::setTickInterval( qint32 newTickInterval )
{
	//kDebug( 610 ) << k_funcinfo << endl;
	m_tickInterval = newTickInterval;
	m_tickTimer->setInterval( newTickInterval );
}

QStringList AbstractMediaProducer::availableAudioStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String( "en" ) << QLatin1String( "de" );
	return ret;
}

QStringList AbstractMediaProducer::availableVideoStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String( "en" ) << QLatin1String( "de" );
	return ret;
}

QStringList AbstractMediaProducer::availableSubtitleStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String( "en" ) << QLatin1String( "de" );
	return ret;
}

QString AbstractMediaProducer::selectedAudioStream( const QObject* audioPath ) const
{
	// TODO
	return m_selectedAudioStream[ audioPath ];
}

QString AbstractMediaProducer::selectedVideoStream( const QObject* videoPath ) const
{
	// TODO
	return m_selectedVideoStream[ videoPath ];
}

QString AbstractMediaProducer::selectedSubtitleStream( const QObject* videoPath ) const
{
	// TODO
	return m_selectedSubtitleStream[ videoPath ];
}

void AbstractMediaProducer::selectAudioStream( const QString& streamName, const QObject* audioPath )
{
	// TODO
	if( availableAudioStreams().contains( streamName ) )
		m_selectedAudioStream[ audioPath ] = streamName;
}

void AbstractMediaProducer::selectVideoStream( const QString& streamName, const QObject* videoPath )
{
	// TODO
	if( availableVideoStreams().contains( streamName ) )
		m_selectedVideoStream[ videoPath ] = streamName;
}

void AbstractMediaProducer::selectSubtitleStream( const QString& streamName, const QObject* videoPath )
{
	// TODO
	if( availableSubtitleStreams().contains( streamName ) )
		m_selectedSubtitleStream[ videoPath ] = streamName;
}

void AbstractMediaProducer::play()
{
	//kDebug( 610 ) << k_funcinfo << endl;
	if( state() != PausedState )
	{
		int total;
		if( xine_get_pos_length( stream(), 0, &m_startTime, &total ) == 1 )
		{
			if( total > 0 && m_startTime < total && m_startTime >= 0 )
				m_startTime = -1;
		}
		else
			m_startTime = -1;
		m_currentTimeOverride = -1;
	}
	setState( Phonon::PlayingState );
	m_tickTimer->start();
}

void AbstractMediaProducer::pause()
{
	//kDebug( 610 ) << k_funcinfo << endl;
	m_tickTimer->stop();
	setState( Phonon::PausedState );
}

void AbstractMediaProducer::stop()
{
	//kDebug( 610 ) << k_funcinfo << endl;
	m_tickTimer->stop();
	setState( Phonon::StoppedState );
	m_startTime = -1;
}

void AbstractMediaProducer::seek( qint64 time )
{
	//kDebug( 610 ) << k_funcinfo << endl;
	if( !isSeekable() )
		return;

	switch( state() )
	{
		case Phonon::PausedState:
		case Phonon::BufferingState:
		case Phonon::PlayingState:
			kDebug( 610 ) << k_funcinfo << "seeking xine stream to " << time << endl;
			// xine_trick_mode aborts :(
			//if( 0 == xine_trick_mode( m_stream, XINE_TRICK_MODE_SEEK_TO_TIME, time ) )
			{
				xine_play( m_stream, 0, time );
				if( Phonon::PausedState == state() )
					// go back to paused speed after seek
					xine_set_param( m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
				m_currentTimeOverride = time;
			}
			break;
		case Phonon::StoppedState:
		case Phonon::ErrorState:
		case Phonon::LoadingState:
			return; // cannot seek
	}
}

void AbstractMediaProducer::setState( State newstate )
{
	if( newstate == m_state ) // no change
		return;
	State oldstate = m_state;
	m_state = newstate;
	kDebug( 610 ) << "reached " << newstate << " after " << oldstate << endl;
	switch( newstate )
	{
		case Phonon::PlayingState:
			reachedPlayingState();
			break;
		case Phonon::PausedState:
		case Phonon::BufferingState:
		case Phonon::StoppedState:
		case Phonon::ErrorState:
		case Phonon::LoadingState:
			if( oldstate == Phonon::PlayingState )
				leftPlayingState();
			break;
	}
	//kDebug( 610 ) << "emit stateChanged( " << newstate << ", " << oldstate << " )" << endl;
	emit stateChanged( newstate, oldstate );
}

void AbstractMediaProducer::emitTick()
{
	//kDebug( 610 ) << "emit tick( " << currentTime() << " )" << endl;
	emit tick( currentTime() );
}

}}
#include "abstractmediaproducer.moc"
// vim: sw=4 ts=4 noet
