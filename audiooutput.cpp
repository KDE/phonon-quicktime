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

#include "audiooutput.h"
#include <QVector>
#include <kdebug.h>

#include <config.h>
#include <sys/ioctl.h>
#include <iostream>
#include <QSet>
#include "audiopath.h"
#include "abstractmediaproducer.h"

namespace Phonon
{
namespace Xine
{
AudioOutput::AudioOutput( QObject* parent, XineEngine* xe )
	: AbstractAudioOutput( parent )
	, m_xine_engine( xe )
	, m_device( 1 )
	, m_audioPort( 0 )
{
}

AudioOutput::~AudioOutput()
{
}

float AudioOutput::volume() const
{
	return m_volume;
}

int AudioOutput::outputDevice() const
{
	return m_device;
}

void AudioOutput::setVolume( float newVolume )
{
	m_volume = newVolume;

	int xinevolume = int(m_volume * 100);
	if( xinevolume > 200) xinevolume = 200;
	if( xinevolume < 0) xinevolume = 0;

	QSet<xine_stream_t*> streams;
	foreach( AudioPath* ap, m_paths )
	{
		foreach( AbstractMediaProducer* mp, ap->producers() )
		{
			streams << mp->stream();
		}
	}
	foreach( xine_stream_t* stream, streams )
	{
		xine_set_param( stream, XINE_PARAM_AUDIO_AMP_LEVEL, xinevolume );
	}

	emit volumeChanged( m_volume );
}

void AudioOutput::setOutputDevice( int newDevice )
{
	m_device = newDevice;
	if( m_audioPort )
		xine_close_audio_driver( m_xine_engine->m_xine, m_audioPort );
	const char* const* outputPlugins = xine_list_audio_output_plugins( m_xine_engine->m_xine );
	kDebug( 610 ) << k_funcinfo << "use output plugin: " << outputPlugins[ newDevice - 10000 ] << endl;
	m_audioPort = xine_open_audio_driver( m_xine_engine->m_xine, outputPlugins[ newDevice - 10000 ], NULL );

	// notify the connected MediaProducers of the new device
	QSet<AbstractMediaProducer *> mps;
	foreach( AudioPath* ap, m_paths )
	{
		foreach( AbstractMediaProducer *mp, ap->producers() )
		{
			mps << mp;
		}
	}
	foreach( AbstractMediaProducer *mp, mps )
		mp->checkAudioOutput();
}

}} //namespace Phonon::Xine

#include "audiooutput.moc"
// vim: sw=4 ts=4 noet
