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

#include "backend.h"
#include "mediaobject.h"
#include "avcapture.h"
#include "bytestream.h"
#include "audiopath.h"
#include "audioeffect.h"
#include "audiooutput.h"
#include "audiodataoutput.h"
#include "videopath.h"
#include "videoeffect.h"

#include <kgenericfactory.h>
#include "volumefadereffect.h"
#include <QSet>
#include "videodataoutput.h"

#include <kdebug.h>

typedef KGenericFactory<Phonon::Xine::Backend, Phonon::Ifaces::Backend> XineBackendFactory;
K_EXPORT_COMPONENT_FACTORY( phonon_xine, XineBackendFactory( "xinebackend" ) )

namespace Phonon
{
namespace Xine
{

Backend::Backend( QObject* parent, const QStringList& )
	: Ifaces::Backend( parent )
{
	char configfile[2048];

	m_xine_engine = new XineEngine();

	m_xine_engine->m_xine = xine_new();
	xine_engine_set_param( m_xine_engine->m_xine, XINE_ENGINE_PARAM_VERBOSITY, 99 );
	sprintf(configfile, "%s%s", xine_get_homedir(), "/.xine/config");
	xine_config_load( m_xine_engine->m_xine, configfile );
	xine_init( m_xine_engine->m_xine );

	kDebug() << "Using Xine version " << xine_get_version_string() << endl;

	// testing
	//xine_video_port_t* m_videoPort = xine_open_video_driver( m_xine, "auto", 1, NULL );
	m_xine_engine->m_audioPort = xine_open_audio_driver( m_xine_engine->m_xine, "auto", NULL );
	m_xine_engine->m_stream = xine_stream_new( m_xine_engine->m_xine, m_xine_engine->m_audioPort, NULL /*m_videoPort*/ );
}

Backend::~Backend()
{
	if( m_xine_engine->m_xine) xine_exit( m_xine_engine->m_xine);
}

Ifaces::MediaObject*      Backend::createMediaObject( QObject* parent )
{
	return new MediaObject( parent, m_xine_engine );
}

Ifaces::AvCapture*        Backend::createAvCapture( QObject* parent )
{
	return new AvCapture( parent, m_xine_engine );
}

Ifaces::ByteStream*       Backend::createByteStream( QObject* parent )
{
	return new ByteStream( parent, m_xine_engine );
}

Ifaces::AudioPath*        Backend::createAudioPath( QObject* parent )
{
	return new AudioPath( parent );
}

Ifaces::AudioEffect*      Backend::createAudioEffect( int effectId, QObject* parent )
{
	return new AudioEffect( effectId, parent );
}

Ifaces::VolumeFaderEffect*      Backend::createVolumeFaderEffect( QObject* parent )
{
	return new VolumeFaderEffect( parent );
}

Ifaces::AudioOutput*      Backend::createAudioOutput( QObject* parent )
{
	return new AudioOutput( parent, m_xine_engine );
}

Ifaces::AudioDataOutput*  Backend::createAudioDataOutput( QObject* parent )
{
	return new AudioDataOutput( parent );
}

Ifaces::VideoPath*        Backend::createVideoPath( QObject* parent )
{
	return new VideoPath( parent );
}

Ifaces::VideoEffect*      Backend::createVideoEffect( int effectId, QObject* parent )
{
	return new VideoEffect( effectId, parent );
}

Ifaces::VideoDataOutput*  Backend::createVideoDataOutput( QObject* parent )
{
	return new VideoDataOutput( parent );
}

bool Backend::supportsVideo() const
{
	return true;
}

bool Backend::supportsOSD() const
{
	return true;
}

bool Backend::supportsSubtitles() const
{
	return true;
}

const QStringList& Backend::knownMimeTypes() const
{
	if( m_supportedMimeTypes.isEmpty() )
	{
		QString mimeTypes = xine_get_mime_types( m_xine_engine->m_xine );
		QStringList lstMimeTypes = mimeTypes.split( ";", QString::SkipEmptyParts );
		foreach( QString mimeType, lstMimeTypes )
		{
			const_cast<Backend*>( this )->m_supportedMimeTypes << mimeType.split( ":" )[0].toLatin1();
		}
	}

	return m_supportedMimeTypes;
}

QSet<int> Backend::audioOutputDeviceIndexes() const
{
	// This will list the audio drivers, not the actual devices.
	const char* const* outputPlugins = xine_list_audio_output_plugins( m_xine_engine->m_xine );
	int i = 0;

	QSet<int> set;

	while( outputPlugins[i] )
	{
		kDebug() << 10000 + i << " = " << outputPlugins[i] << endl;
		set << 10000 + i;
		++i;
	}

	return set;
}

QString Backend::audioOutputDeviceName( int index ) const
{
	const char* const* outputPlugins = xine_list_audio_output_plugins( m_xine_engine->m_xine );
	int i = 0;

	while( outputPlugins[i] )
	{
		if( 10000 + i == index )
		{
			QString name = outputPlugins[i];
			return name.toLatin1();
		}
		++i;
	}

	return QString();
}

QString Backend::audioOutputDeviceDescription( int index ) const
{
	const char* const* outputPlugins = xine_list_audio_output_plugins( m_xine_engine->m_xine );
	int i = 0;

	while( outputPlugins[i] )
	{
		if( 10000 + i == index )
		{
			QString description = xine_get_audio_driver_plugin_description( m_xine_engine->m_xine, outputPlugins[i] );
			return description.toLatin1();
		}
		++i;
	}

	return QString();
}

QSet<int> Backend::audioCaptureDeviceIndexes() const
{
	QSet<int> set;
	set << 20000 << 20001;
	return set;
}

QString Backend::audioCaptureDeviceName( int index ) const
{
	switch( index )
	{
		case 20000:
			return "Soundcard";
		case 20001:
			return "DV";
		default:
			return QString();
	}
}

QString Backend::audioCaptureDeviceDescription( int index ) const
{
	switch( index )
	{
		case 20000:
			return "first description";
		case 20001:
			return "second description";
		default:
			return QString();
	}
}

int Backend::audioCaptureDeviceVideoIndex( int index ) const
{
	switch( index )
	{
		case 20001:
			return 30001;
		default:
			return -1;
	}
}

QSet<int> Backend::videoOutputDeviceIndexes() const
{
	const char* const* outputPlugins = xine_list_video_output_plugins( m_xine_engine->m_xine );
	int i = 0;

	QSet<int> set;

	while( outputPlugins[i] )
	{
		kDebug() << 40000 + i << " = " << outputPlugins[i] << endl;
		set << 40000 + i;
		++i;
	}

	return set;
}

QString Backend::videoOutputDeviceName( int index ) const
{
	const char* const* outputPlugins = xine_list_video_output_plugins( m_xine_engine->m_xine );
	int i = 0;

	while( outputPlugins[i] )
	{
		if( 40000 + i == index )
		{
			QString name = outputPlugins[i];
			return name.toLatin1();
		}
		++i;
	}

	return QString();
}

QString Backend::videoOutputDeviceDescription( int index ) const
{
	const char* const* outputPlugins = xine_list_video_output_plugins( m_xine_engine->m_xine );
	int i = 0;

	while( outputPlugins[i] )
	{
		if( 40000 + i == index )
		{
			QString description = xine_get_video_driver_plugin_description( m_xine_engine->m_xine, outputPlugins[i] );
			return description.toLatin1();
		}
		++i;
	}

	return QString();
}

QSet<int> Backend::videoCaptureDeviceIndexes() const
{
	QSet<int> set;
	set << 30000 << 30001;
	return set;
}

QString Backend::videoCaptureDeviceName( int index ) const
{
	switch( index )
	{
		case 30000:
			return "USB Webcam";
		case 30001:
			return "DV";
		default:
			return QString();
	}
}

QString Backend::videoCaptureDeviceDescription( int index ) const
{
	switch( index )
	{
		case 30000:
			return "first description";
		case 30001:
			return "second description";
		default:
			return QString();
	}
}

int Backend::videoCaptureDeviceAudioIndex( int index ) const
{
	switch( index )
	{
		case 30001:
			return 20001;
		default:
			return -1;
	}
}

QSet<int> Backend::audioEffectIndexes() const
{
	QSet<int> ret;
	ret << 0x7F000001;
	return ret;
}

QString Backend::audioEffectName( int index ) const
{
	switch( index )
	{
		case 0x7F000001:
			return "Delay";
	}
	return QString();
}

QString Backend::audioEffectDescription( int index ) const
{
	switch( index )
	{
		case 0x7F000001:
			return "Simple delay effect with time, feedback and level controls.";
	}
	return QString();
}

QSet<int> Backend::videoEffectIndexes() const
{
	QSet<int> ret;
	ret << 0x7E000001;
	return ret;
}

QString Backend::videoEffectName( int index ) const
{
	switch( index )
	{
		case 0x7E000001:
			return "VideoEffect1";
	}
	return QString();
}

QString Backend::videoEffectDescription( int index ) const
{
	switch( index )
	{
		case 0x7E000001:
			return "Description 1";
	}
	return QString();
}

const char* Backend::uiLibrary() const
{
	return "phonon_xineui";
}

void Backend::freeSoundcardDevices()
{
}

}}

#include "backend.moc"

// vim: sw=4 ts=4 noet
