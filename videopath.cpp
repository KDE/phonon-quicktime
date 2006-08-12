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

namespace Phonon
{
namespace Xine
{

VideoPath::VideoPath( QObject* parent )
	: QObject( parent )
{
}

VideoPath::~VideoPath()
{
}

bool VideoPath::hasOutput() const
{
	//TODO implement
	return false;
	//return ( m_output && m_output->videoPort() != 0 );
}

xine_video_port_t *VideoPath::videoPort() const
{
	//TODO implement
	//if( m_output )
		//return m_output->videoPort();
	return 0;
}

bool VideoPath::addOutput( QObject* videoOutput )
{
	Q_ASSERT( videoOutput );
	Q_ASSERT( !m_outputs.contains( videoOutput ) );
	m_outputs.append( videoOutput );
	return true;
}

bool VideoPath::removeOutput( QObject* videoOutput )
{
	Q_ASSERT( videoOutput );
	Q_ASSERT( m_outputs.removeAll( videoOutput ) == 1 );
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

	return true;
}

bool VideoPath::removeEffect( QObject* effect )
{
	Q_ASSERT( effect );
	VideoEffect* ve = qobject_cast<VideoEffect*>( effect );
	Q_ASSERT( ve );
	if( m_effects.removeAll( ve ) > 0 )
		return true;
	return false;
}

void VideoPath::addMediaProducer( AbstractMediaProducer* mp )
{
	m_producers.append( mp );
}

void VideoPath::removeMediaProducer( AbstractMediaProducer* mp )
{
	m_producers.removeAll( mp );
}

}}

#include "videopath.moc"
// vim: sw=4 ts=4 noet
