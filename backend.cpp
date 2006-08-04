/* This file is part of the KDE project
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
#include "visualization.h"
#include "videopath.h"
#include "videoeffect.h"

#include <kgenericfactory.h>
#include "volumefadereffect.h"
#include <QSet>
#include "videodataoutput.h"

#include <kdebug.h>

typedef KGenericFactory<Phonon::Xine::Backend> XineBackendFactory;
K_EXPORT_COMPONENT_FACTORY( phonon_xine, XineBackendFactory( "xinebackend" ) )

namespace Phonon
{
namespace Xine
{

Backend::Backend( QObject* parent, const QStringList& )
	: QObject( parent )
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

QObject* Backend::createMediaObject( QObject* parent )
{
	return new MediaObject( parent, m_xine_engine );
}

QObject* Backend::createAvCapture( QObject* parent )
{
	return new AvCapture( parent, m_xine_engine );
}

QObject* Backend::createByteStream( QObject* parent )
{
	return new ByteStream( parent, m_xine_engine );
}

QObject* Backend::createAudioPath( QObject* parent )
{
	return new AudioPath( parent );
}

QObject* Backend::createAudioEffect( int effectId, QObject* parent )
{
	return new AudioEffect( effectId, parent );
}

QObject* Backend::createVolumeFaderEffect( QObject* parent )
{
	return new VolumeFaderEffect( parent );
}

QObject* Backend::createAudioOutput( QObject* parent )
{
	return new AudioOutput( parent, m_xine_engine );
}

QObject* Backend::createAudioDataOutput( QObject* parent )
{
	return new AudioDataOutput( parent );
}

QObject* Backend::createVisualization( QObject* parent )
{
	return new Visualization( parent );
}

QObject* Backend::createVideoPath( QObject* parent )
{
	return new VideoPath( parent );
}

QObject* Backend::createVideoEffect( int effectId, QObject* parent )
{
	return new VideoEffect( effectId, parent );
}

QObject* Backend::createVideoDataOutput( QObject* parent )
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

bool Backend::supportsFourcc( quint32 fourcc ) const
{
	switch( fourcc )
	{
		case 0x00000000:
			return true;
		default:
			return false;
	}
}

bool Backend::supportsSubtitles() const
{
	return true;
}

QStringList Backend::knownMimeTypes()
{
	if( m_supportedMimeTypes.isEmpty() )
	{
		char* mimeTypes_c = xine_get_mime_types( m_xine_engine->m_xine );
		QString mimeTypes( mimeTypes_c );
		free( mimeTypes_c );
		QStringList lstMimeTypes = mimeTypes.split( ";", QString::SkipEmptyParts );
		foreach( QString mimeType, lstMimeTypes )
			m_supportedMimeTypes << mimeType.left( mimeType.indexOf( ':' ) ).trimmed();
		if( m_supportedMimeTypes.contains( "application/x-ogg" ) )
			m_supportedMimeTypes << QLatin1String( "audio/vorbis" );
	}

	return m_supportedMimeTypes;
}

QSet<int> Backend::objectDescriptionIndexes( ObjectDescriptionType type ) const
{
	QSet<int> set;
	switch( type )
	{
		case Phonon::AudioOutputDeviceType:
			{
				// This will list the audio drivers, not the actual devices.
				const char* const* outputPlugins = xine_list_audio_output_plugins( m_xine_engine->m_xine );
				for( int i = 0; outputPlugins[i]; ++i )
					set << 10000 + i;
				break;
			}
		case Phonon::AudioCaptureDeviceType:
			set << 20000 << 20001;
			break;
		case Phonon::VideoOutputDeviceType:
			{
				const char* const* outputPlugins = xine_list_video_output_plugins( m_xine_engine->m_xine );
				for( int i = 0; outputPlugins[i]; ++i )
					set << 40000 + i;
				break;
			}
		case Phonon::VideoCaptureDeviceType:
			set << 30000 << 30001;
			break;
		case Phonon::VisualizationType:
			break;
		case Phonon::AudioCodecType:
			break;
		case Phonon::VideoCodecType:
			break;
		case Phonon::ContainerFormatType:
			break;
		case Phonon::AudioEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( m_xine_engine->m_xine, XINE_POST_TYPE_AUDIO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					set << 0x7F000000 + i;
				break;
			}
		case Phonon::VideoEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( m_xine_engine->m_xine, XINE_POST_TYPE_VIDEO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					set << 0x7E000000 + i;
				break;
			}
	}
	return set;
}

QString Backend::objectDescriptionName( ObjectDescriptionType type, int index ) const
{
	switch( type )
	{
		case Phonon::AudioOutputDeviceType:
			{
				const char* const* outputPlugins = xine_list_audio_output_plugins( m_xine_engine->m_xine );
				for( int i = 0; outputPlugins[i]; ++i )
					if( 10000 + i == index )
						return QLatin1String( outputPlugins[i] );
				break;
			}
		case Phonon::AudioCaptureDeviceType:
			switch( index )
			{
				case 20000:
					return "Soundcard";
				case 20001:
					return "DV";
			}
			break;
		case Phonon::VideoOutputDeviceType:
			{
				const char* const* outputPlugins = xine_list_video_output_plugins( m_xine_engine->m_xine );
				for( int i = 0; outputPlugins[i]; ++i )
					if( 40000 + i == index )
						return QLatin1String( outputPlugins[i] );
				break;
			}
		case Phonon::VideoCaptureDeviceType:
			switch( index )
			{
				case 30000:
					return "USB Webcam";
				case 30001:
					return "DV";
			}
			break;
		case Phonon::VisualizationType:
			break;
		case Phonon::AudioCodecType:
			break;
		case Phonon::VideoCodecType:
			break;
		case Phonon::ContainerFormatType:
			break;
		case Phonon::AudioEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( m_xine_engine->m_xine, XINE_POST_TYPE_AUDIO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					if( 0x7F000000 + i == index )
						return QLatin1String( postPlugins[i] );
				break;
			}
		case Phonon::VideoEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( m_xine_engine->m_xine, XINE_POST_TYPE_VIDEO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					if( 0x7E000000 + i == index )
						return QLatin1String( postPlugins[i] );
				break;
			}
	}
	return QString();
}

QString Backend::objectDescriptionDescription( ObjectDescriptionType type, int index ) const
{
	switch( type )
	{
		case Phonon::AudioOutputDeviceType:
			{
				const char* const* outputPlugins = xine_list_audio_output_plugins( m_xine_engine->m_xine );
				for( int i = 0; outputPlugins[i]; ++i )
					if( 10000 + i == index )
						return QLatin1String( xine_get_audio_driver_plugin_description( m_xine_engine->m_xine, outputPlugins[i] ) );
				break;
			}
		case Phonon::AudioCaptureDeviceType:
			break;
		case Phonon::VideoOutputDeviceType:
			{
				const char* const* outputPlugins = xine_list_video_output_plugins( m_xine_engine->m_xine );
				for( int i = 0; outputPlugins[i]; ++i )
					if( 40000 + i == index )
						return QLatin1String( xine_get_video_driver_plugin_description( m_xine_engine->m_xine, outputPlugins[i] ) );
				break;
			}
		case Phonon::VideoCaptureDeviceType:
			switch( index )
			{
				case 30000:
					return "first description";
				case 30001:
					return "second description";
			}
			break;
		case Phonon::VisualizationType:
			break;
		case Phonon::AudioCodecType:
			break;
		case Phonon::VideoCodecType:
			break;
		case Phonon::ContainerFormatType:
			break;
		case Phonon::AudioEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( m_xine_engine->m_xine, XINE_POST_TYPE_AUDIO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					if( 0x7F000000 + i == index )
						return QLatin1String( xine_get_post_plugin_description( m_xine_engine->m_xine, postPlugins[i] ) );
				break;
			}
		case Phonon::VideoEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( m_xine_engine->m_xine, XINE_POST_TYPE_VIDEO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					if( 0x7E000000 + i == index )
						return QLatin1String( postPlugins[i] );
				break;
			}
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
