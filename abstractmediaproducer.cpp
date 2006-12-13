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
#include <seekthread.h>

namespace Phonon
{
namespace Xine
{
AbstractMediaProducer::AbstractMediaProducer(QObject *parent)
    : QObject( parent ),
    m_stream(this),
    m_tickTimer(new QTimer(this)),
    m_startTime(-1),
    m_audioPath(0),
    m_videoPath(0),
    m_currentTimeOverride(-1)
{
    m_stream.start();
    connect(&m_stream, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
            SLOT(handleStateChange(Phonon::State, Phonon::State)));
    connect(&m_stream, SIGNAL(metaDataChanged(const QMultiMap<QString, QString>&)),
            SIGNAL(metaDataChanged(const QMultiMap<QString, QString>&)));

	connect( m_tickTimer, SIGNAL( timeout() ), SLOT( emitTick() ) );
}

AbstractMediaProducer::~AbstractMediaProducer()
{
    m_stream.quit();
    m_stream.wait();
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

bool AbstractMediaProducer::addVideoPath( QObject* videoPath )
{
	if( m_videoPath )
		return false;

	m_videoPath = qobject_cast<VideoPath*>( videoPath );
	Q_ASSERT( m_videoPath );
	m_videoPath->setMediaProducer( this );

	if( m_stream )
	{
		if( !recreateStream() )
			return false;
		if( m_videoPath->hasOutput() )
			xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 0 );
		else
			xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
	}
	return true;
}

bool AbstractMediaProducer::addAudioPath( QObject* audioPath )
{
	if( m_audioPath )
		return false;
	m_audioPath = qobject_cast<AudioPath*>( audioPath );
	Q_ASSERT( m_audioPath );
	m_audioPath->addMediaProducer( this );
	if( m_stream )
	{
		if( !recreateStream() )
			return false;
		if( m_audioPath->hasOutput() )
			xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 0 );
		else
			xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 1 );
	}
	return true;
}

void AbstractMediaProducer::removeVideoPath( QObject* videoPath )
{
	Q_ASSERT( videoPath );
	if( m_videoPath == qobject_cast<VideoPath*>( videoPath ) )
	{
		m_videoPath->unsetMediaProducer( this );
		m_videoPath = 0;
		if( m_stream )
		{
			recreateStream();
			xine_set_param( m_stream, XINE_PARAM_IGNORE_VIDEO, 1 );
		}
	}
}

void AbstractMediaProducer::removeAudioPath( QObject* audioPath )
{
	Q_ASSERT( audioPath );
	if( m_audioPath == qobject_cast<AudioPath*>( audioPath ) )
	{
		m_audioPath->removeMediaProducer( this );
		m_audioPath = 0;
		if( m_stream )
		{
			recreateStream();
			xine_set_param( m_stream, XINE_PARAM_IGNORE_AUDIO, 1 );
		}
	}
}

State AbstractMediaProducer::state() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_stream.state();
}

bool AbstractMediaProducer::hasVideo() const
{
    return m_stream.hasVideo();
}

bool AbstractMediaProducer::isSeekable() const
{
    return m_stream.isSeekable();
}

qint64 AbstractMediaProducer::currentTime() const
{
    switch(m_stream.state()) {
		case Phonon::PausedState:
		case Phonon::BufferingState:
		case Phonon::PlayingState:
			{
                int positiontime = m_stream.currentTime()
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
	delayedInit();

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
			kDebug( 610 ) << k_funcinfo << "seeking xine stream to " << time << "ms" << endl;
			// xine_trick_mode aborts :(
			//if( 0 == xine_trick_mode( m_stream, XINE_TRICK_MODE_SEEK_TO_TIME, time ) )
			{
				emit asyncSeek( m_stream, time, Phonon::PausedState == state() );
				/*
				xine_play( m_stream, 0, time );
				if( Phonon::PausedState == state() )
					// go back to paused speed after seek
					xine_set_param( m_stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE );
				*/
				m_currentTimeOverride = time;
			}
			break;
		case Phonon::StoppedState:
		case Phonon::ErrorState:
		case Phonon::LoadingState:
			return; // cannot seek
	}
}

void AbstractMediaProducer::handleStateChange(Phonon::State newstate, Phonon::State oldstate)
{
    kDebug(610) << "reached " << newstate << " after " << oldstate << endl;
    switch( newstate )
    {
        /*
		case Phonon::PlayingState:
			reachedPlayingState();
			break;
            */
        case Phonon::ErrorState:
            if(m_event_queue) {
                xine_event_dispose_queue(m_event_queue);
                m_event_queue = 0;
            }
            if(m_stream) {
                xine_dispose(m_stream);
                m_stream = 0;
            }
            // fall through
        case Phonon::BufferingState:
        case Phonon::PausedState:
        case Phonon::StoppedState:
        case Phonon::LoadingState:
            if (oldstate == Phonon::PlayingState) {
                leftPlayingState();
            }
            break;
    }
    emit stateChanged(newstate, oldstate);
}

void AbstractMediaProducer::reachedPlayingState()
{
    m_tickTimer->start();
}

void AbstractMediaProducer::leftPlayingState()
{
    m_tickTimer->stop();
}

void AbstractMediaProducer::emitTick()
{
	//kDebug( 610 ) << "emit tick( " << currentTime() << " )" << endl;
	emit tick( currentTime() );
}

}}
#include "abstractmediaproducer.moc"
// vim: sw=4 ts=4
