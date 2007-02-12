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

#include "videopath.h"
#include "videoeffect.h"
#include "abstractvideooutput.h"
#include "videowidgetinterface.h"
#include "abstractmediaproducer.h"
#include "videodataoutput.h"
#include <xine.h>

namespace Phonon
{
namespace Xine
{

VideoPath::VideoPath( QObject* parent )
	: QObject( parent )
	, m_output( 0 )
	, m_producer( 0 )
{
}

VideoPath::~VideoPath()
{
	foreach( VideoEffect* ve, m_effects )
		ve->setPath( 0 );
	if( m_output )
		m_output->unsetPath( this );
}

void VideoPath::streamFinished()
{
}

bool VideoPath::hasOutput() const
{
	return ( m_output && m_output->videoPort() != 0 );
}

VideoWidgetInterface *VideoPath::videoPort() const
{
    if (m_output) {
        return m_output;
    }
    return 0;
}

void VideoPath::videoPortChanged()
{
    if (m_producer) {
        m_producer->setVideoPort(m_output);
    }
}

bool VideoPath::addOutput( QObject* videoOutput )
{
	VideoWidgetInterface *vwi = qobject_cast<VideoWidgetInterface*>( videoOutput );
	if( vwi )
	{
		if( m_output )
			return false;
		m_output = vwi;
		m_output->setPath( this );
		if( m_producer && m_output->videoPort() != 0 )
            m_producer->setVideoPort(m_output);
        connect(videoOutput, SIGNAL(videoPortChanged()), SLOT(videoPortChanged()));
		return true;
	}

	Xine::VideoDataOutput *vdo = qobject_cast<Xine::VideoDataOutput*>( videoOutput );
	Q_ASSERT( vdo );
	Q_ASSERT( !m_outputs.contains( vdo ) );
	m_outputs.append( vdo );
	vdo->addPath( this );
	return true;
}

bool VideoPath::removeOutput( QObject* videoOutput )
{
	VideoWidgetInterface *vwi = qobject_cast<VideoWidgetInterface*>( videoOutput );
	if( vwi && m_output == vwi )
	{
		m_output->unsetPath( this );
		m_output = 0;
		if( m_producer )
            m_producer->setVideoPort(0);
        disconnect(videoOutput, SIGNAL(videoPortChanged()), this, SLOT(videoPortChanged()));
		return true;
	}

	Xine::VideoDataOutput *vdo = qobject_cast<Xine::VideoDataOutput*>( videoOutput );
	Q_ASSERT( vdo );
	Q_ASSERT( m_outputs.removeAll( vdo ) == 1 );
	vdo->removePath( this );
	return true;
}

bool VideoPath::insertEffect( QObject* newEffect, QObject* insertBefore )
{
	Q_ASSERT( newEffect );
	VideoEffect* ve = qobject_cast<VideoEffect*>( newEffect );
	Q_ASSERT( ve );
	VideoEffect* before = 0;
	if( insertBefore )
	{
		before = qobject_cast<VideoEffect*>( insertBefore );
		Q_ASSERT( before );
		if( !m_effects.contains( before ) )
			return false;
		m_effects.insert( m_effects.indexOf( before ), ve );
	}
	else
		m_effects.append( ve );
	ve->setPath( this );

	return true;
}

bool VideoPath::removeEffect( QObject* effect )
{
	Q_ASSERT( effect );
	VideoEffect* ve = qobject_cast<VideoEffect*>( effect );
	Q_ASSERT( ve );
	if( m_effects.removeAll( ve ) > 0 )
	{
		ve->setPath( 0 );
		return true;
	}
	return false;
}

void VideoPath::setMediaProducer(AbstractMediaProducer *mp)
{
    Q_ASSERT(m_producer == 0);
    m_producer = mp;
}

void VideoPath::unsetMediaProducer( AbstractMediaProducer* mp )
{
    Q_ASSERT(m_producer == mp);
    m_producer = 0;
}

}}

#include "videopath.moc"
// vim: sw=4 ts=4
