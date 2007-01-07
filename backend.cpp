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
#include "mediaqueue.h"
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
#include "brightnesscontrol.h"
#include <QVariant>

typedef KGenericFactory<Phonon::Xine::Backend> XineBackendFactory;
K_EXPORT_COMPONENT_FACTORY( phonon_xine_threaded, XineBackendFactory( "xinebackend" ) )

namespace Phonon
{
namespace Xine
{

Backend::Backend( QObject* parent, const QStringList& )
	: QObject( parent )
{
	char configfile[2048];

    xine_engine_set_param(XineEngine::xine(), XINE_ENGINE_PARAM_VERBOSITY, 99);
	sprintf(configfile, "%s%s", xine_get_homedir(), "/.xine/config");
	xine_config_load( XineEngine::xine(), configfile );
	xine_init( XineEngine::xine() );

	kDebug( 610 ) << "Using Xine version " << xine_get_version_string() << endl;

	// testing
	//xine_video_port_t* m_videoPort = xine_open_video_driver( m_xine, "auto", 1, NULL );
}

Backend::~Backend()
{
	delete XineEngine::self();
}

QObject* Backend::createObject0(BackendInterface::Class0 c, QObject *parent)
{
    switch (c) {
        case MediaObjectClass:
            return new MediaObject(parent);
        case MediaQueueClass:
            return new MediaQueue(parent);
        case AvCaptureClass:
            return new AvCapture(parent);
        case ByteStreamClass:
            return new ByteStream(parent);
        case AudioPathClass:
            return new AudioPath(parent);
        case VolumeFaderEffectClass:
            return new VolumeFaderEffect(parent);
        case AudioOutputClass:
            return new AudioOutput(parent);
        case AudioDataOutputClass:
            return new AudioDataOutput(parent);
        case VisualizationClass:
            return new Visualization(parent);
        case VideoPathClass:
            return new VideoPath(parent);
        case BrightnessControlClass:
            return new BrightnessControl(parent);
        case VideoDataOutputClass:
            return new VideoDataOutput(parent);
    }
    return 0;
}

QObject* Backend::createObject1(BackendInterface::Class1 c, QObject *parent, QVariant arg1)
{
    switch (c) {
        case AudioEffectClass:
            return new AudioEffect(arg1.toInt(), parent);
        case VideoEffectClass:
            return new VideoEffect(arg1.toInt(), parent);
    }
    return 0;
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

QStringList Backend::knownMimeTypes() const
{
	if( m_supportedMimeTypes.isEmpty() )
	{
		char* mimeTypes_c = xine_get_mime_types( XineEngine::xine() );
		QString mimeTypes( mimeTypes_c );
		free( mimeTypes_c );
		QStringList lstMimeTypes = mimeTypes.split( ";", QString::SkipEmptyParts );
		foreach( QString mimeType, lstMimeTypes )
			m_supportedMimeTypes << mimeType.left( mimeType.indexOf( ':' ) ).trimmed();
		if( m_supportedMimeTypes.contains( "application/x-ogg" ) )
			m_supportedMimeTypes << QLatin1String( "audio/vorbis" ) << QLatin1String( "application/ogg" );
	}

	return m_supportedMimeTypes;
}

QSet<int> Backend::objectDescriptionIndexes( ObjectDescriptionType type ) const
{
	QSet<int> set;
	switch( type )
	{
		case Phonon::AudioOutputDeviceType:
            return XineEngine::audioOutputIndexes();
		case Phonon::AudioCaptureDeviceType:
			set << 20000 << 20001;
			break;
		case Phonon::VideoOutputDeviceType:
			{
				const char* const* outputPlugins = xine_list_video_output_plugins( XineEngine::xine() );
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
				const char* const* postPlugins = xine_list_post_plugins_typed( XineEngine::xine(), XINE_POST_TYPE_AUDIO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					set << 0x7F000000 + i;
				break;
			}
		case Phonon::VideoEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( XineEngine::xine(), XINE_POST_TYPE_VIDEO_FILTER );
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
            return XineEngine::audioOutputName(index);
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
				const char* const* outputPlugins = xine_list_video_output_plugins( XineEngine::xine() );
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
				const char* const* postPlugins = xine_list_post_plugins_typed( XineEngine::xine(), XINE_POST_TYPE_AUDIO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					if( 0x7F000000 + i == index )
						return QLatin1String( postPlugins[i] );
				break;
			}
		case Phonon::VideoEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( XineEngine::xine(), XINE_POST_TYPE_VIDEO_FILTER );
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
            return XineEngine::audioOutputDescription(index);
		case Phonon::AudioCaptureDeviceType:
			break;
		case Phonon::VideoOutputDeviceType:
			{
				const char* const* outputPlugins = xine_list_video_output_plugins( XineEngine::xine() );
				for( int i = 0; outputPlugins[i]; ++i )
					if( 40000 + i == index )
						return QLatin1String( xine_get_video_driver_plugin_description( XineEngine::xine(), outputPlugins[i] ) );
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
				const char* const* postPlugins = xine_list_post_plugins_typed( XineEngine::xine(), XINE_POST_TYPE_AUDIO_FILTER );
				for( int i = 0; postPlugins[i]; ++i )
					if( 0x7F000000 + i == index )
						return QLatin1String( xine_get_post_plugin_description( XineEngine::xine(), postPlugins[i] ) );
				break;
			}
		case Phonon::VideoEffectType:
			{
				const char* const* postPlugins = xine_list_post_plugins_typed( XineEngine::xine(), XINE_POST_TYPE_VIDEO_FILTER );
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

// vim: sw=4 ts=4
