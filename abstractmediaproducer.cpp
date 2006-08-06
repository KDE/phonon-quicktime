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
static const int SAMPLE_RATE = 44100;
static const float SAMPLE_RATE_FLOAT = 44100.0f;

AbstractMediaProducer::AbstractMediaProducer( QObject* parent, XineEngine* xe )
	: QObject( parent )
	, m_xine_engine( xe )
	, m_state( Phonon::LoadingState )
	, m_tickTimer( new QTimer( this ) )
	, m_startTime( -1 )
{
	m_stream = xine_stream_new( m_xine_engine->m_xine, m_xine_engine->m_audioPort, NULL /*m_videoPort*/ );

	connect( m_tickTimer, SIGNAL( timeout() ), SLOT( emitTick() ) );

	xine_event_queue_t* event_queue = xine_event_new_queue( m_stream );
	xine_event_create_listener_thread( event_queue, &m_xine_engine->xineEventListener, (void*)this );
	xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 1 );
	xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
}

AbstractMediaProducer::~AbstractMediaProducer()
{
	//kDebug() << k_funcinfo << endl;
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
		int tmp;
		if( xine_get_pos_length( stream(), &tmp, &m_startTime, &total ) == 1 )
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
	//kDebug() << k_funcinfo << endl;
	Q_ASSERT( videoPath );
	VideoPath* vp = qobject_cast<VideoPath*>( videoPath );
	Q_ASSERT( vp );
	Q_ASSERT( !m_videoPathList.contains( vp ) );
	m_videoPathList.append( vp );
	xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 0 );
	return true;
}

bool AbstractMediaProducer::addAudioPath( QObject* audioPath )
{
	//kDebug() << k_funcinfo << endl;
	Q_ASSERT( audioPath );
	AudioPath* ap = qobject_cast<AudioPath*>( audioPath );
	Q_ASSERT( ap );
	Q_ASSERT( !m_audioPathList.contains( ap ) );
	m_audioPathList.append( ap );
	ap->addMediaProducer( this );
	xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 0 );
	return true;
}

void AbstractMediaProducer::removeVideoPath( QObject* videoPath )
{
	Q_ASSERT( videoPath );
	VideoPath* vp = qobject_cast<VideoPath*>( videoPath );
	Q_ASSERT( vp );
	Q_ASSERT( m_videoPathList.contains( vp ) );
	m_videoPathList.removeAll( vp );
	if( m_audioPathList.isEmpty() )
		xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
}

void AbstractMediaProducer::removeAudioPath( QObject* audioPath )
{
	Q_ASSERT( audioPath );
	AudioPath* ap = qobject_cast<AudioPath*>( audioPath );
	Q_ASSERT( ap );
	Q_ASSERT( m_audioPathList.contains( ap ) );
	m_audioPathList.removeAll( ap );
	if( m_audioPathList.isEmpty() )
		xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 1 );
	ap->removeMediaProducer( this );
}

State AbstractMediaProducer::state() const
{
	//kDebug() << k_funcinfo << endl;
	return m_state;
}

bool AbstractMediaProducer::hasVideo() const
{
	//kDebug() << k_funcinfo << endl;
	return xine_get_stream_info( m_stream, XINE_STREAM_INFO_HAS_VIDEO );
}

bool AbstractMediaProducer::isSeekable() const
{
	//kDebug() << k_funcinfo << endl;
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
				int positionstream = 0;
				int positiontime = 0;
				int lengthtime = 0;

				if( xine_get_pos_length( stream(), &positionstream, &positiontime, &lengthtime ) == 1 )
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
	//kDebug() << k_funcinfo << endl;
	return m_tickInterval;
}

void AbstractMediaProducer::setTickInterval( qint32 newTickInterval )
{
	//kDebug() << k_funcinfo << endl;
	m_tickInterval = newTickInterval;
	m_tickTimer->setInterval( newTickInterval );
}

QStringList AbstractMediaProducer::availableAudioStreams() const
{
	QStringList ret;
	ret << QLatin1String( "en" ) << QLatin1String( "de" );
	return ret;
}

QStringList AbstractMediaProducer::availableVideoStreams() const
{
	QStringList ret;
	ret << QLatin1String( "en" ) << QLatin1String( "de" );
	return ret;
}

QStringList AbstractMediaProducer::availableSubtitleStreams() const
{
	QStringList ret;
	ret << QLatin1String( "en" ) << QLatin1String( "de" );
	return ret;
}

QString AbstractMediaProducer::selectedAudioStream( const QObject* audioPath ) const
{
	return m_selectedAudioStream[ audioPath ];
}

QString AbstractMediaProducer::selectedVideoStream( const QObject* videoPath ) const
{
	return m_selectedVideoStream[ videoPath ];
}

QString AbstractMediaProducer::selectedSubtitleStream( const QObject* videoPath ) const
{
	return m_selectedSubtitleStream[ videoPath ];
}

void AbstractMediaProducer::selectAudioStream( const QString& streamName, const QObject* audioPath )
{
	if( availableAudioStreams().contains( streamName ) )
		m_selectedAudioStream[ audioPath ] = streamName;
}

void AbstractMediaProducer::selectVideoStream( const QString& streamName, const QObject* videoPath )
{
	if( availableVideoStreams().contains( streamName ) )
		m_selectedVideoStream[ videoPath ] = streamName;
}

void AbstractMediaProducer::selectSubtitleStream( const QString& streamName, const QObject* videoPath )
{
	if( availableSubtitleStreams().contains( streamName ) )
		m_selectedSubtitleStream[ videoPath ] = streamName;
}

void AbstractMediaProducer::play()
{
	//kDebug() << k_funcinfo << endl;
	int total;
	int tmp;
	if( xine_get_pos_length( stream(), &tmp, &m_startTime, &total ) == 1 )
	{
		if( total > 0 && m_startTime < total && m_startTime >= 0 )
			m_startTime = -1;
	}
	else
		m_startTime = -1;
	setState( Phonon::PlayingState );
	m_tickTimer->start();
}

void AbstractMediaProducer::pause()
{
	//kDebug() << k_funcinfo << endl;
	m_tickTimer->stop();
	setState( Phonon::PausedState );
}

void AbstractMediaProducer::stop()
{
	//kDebug() << k_funcinfo << endl;
	m_tickTimer->stop();
	setState( Phonon::StoppedState );
}

void AbstractMediaProducer::seek( qint64 time )
{
	//kDebug() << k_funcinfo << endl;
	if( !isSeekable() )
		return;

	switch( state() )
	{
		case Phonon::PausedState:
			xine_play( m_stream, 0, time );
			xine_set_param( m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
			break;
		case Phonon::BufferingState:
		case Phonon::PlayingState:
			xine_play( m_stream, 0, time );
			break;
		case Phonon::StoppedState:
		case Phonon::ErrorState:
		case Phonon::LoadingState:
			return; // cannot seek
	}
}

void AbstractMediaProducer::setState( State newstate )
{
	if( newstate == m_state )
		return;
	State oldstate = m_state;
	m_state = newstate;
	switch( newstate )
	{
		case Phonon::PausedState:
		case Phonon::BufferingState:
		case Phonon::PlayingState:
		case Phonon::StoppedState:
		case Phonon::ErrorState:
		case Phonon::LoadingState:
			break;
	}
	//kDebug() << "emit stateChanged( " << newstate << ", " << oldstate << " )" << endl;
	emit stateChanged( newstate, oldstate );
}

void AbstractMediaProducer::emitTick()
{
	//kDebug( 604 ) << "emit tick( " << currentTime() << " )" << endl;
	emit tick( currentTime() );
}

}}
#include "abstractmediaproducer.moc"
// vim: sw=4 ts=4 noet
