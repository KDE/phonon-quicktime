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
AudioOutput::AudioOutput( QObject* parent )
	: AbstractAudioOutput( parent )
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

void AudioOutput::updateVolume( AbstractMediaProducer* mp ) const
{
	int xinevolume = int(m_volume * 100);
	if( xinevolume > 200) xinevolume = 200;
	if( xinevolume < 0) xinevolume = 0;

	xine_stream_t* stream = mp->stream();
	if( stream )
		xine_set_param( stream, XINE_PARAM_AUDIO_AMP_LEVEL, xinevolume );
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
		if( stream ) // avoid xine crash when passing a null-pointer
			xine_set_param( stream, XINE_PARAM_AUDIO_AMP_LEVEL, xinevolume );
	}

	emit volumeChanged( m_volume );
}

void AudioOutput::setOutputDevice( int newDevice )
{
	m_device = newDevice;
	xine_audio_port_t* oldAudioPort = m_audioPort;

	const char* const* outputPlugins = xine_list_audio_output_plugins( XineEngine::xine() );
	kDebug( 610 ) << k_funcinfo << "use output plugin: " << outputPlugins[ newDevice - 10000 ] << endl;
	m_audioPort = xine_open_audio_driver( XineEngine::xine(), outputPlugins[ newDevice - 10000 ], NULL );
	if( !m_audioPort )
	{
		m_audioPort = oldAudioPort;
		return; //false;
	}

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

	if( oldAudioPort )
		xine_close_audio_driver( XineEngine::xine(), oldAudioPort );
}

}} //namespace Phonon::Xine

#include "audiooutput.moc"
// vim: sw=4 ts=4 noet
