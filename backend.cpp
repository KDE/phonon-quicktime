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
#include "volumefadereffect.h"
#include "brightnesscontrol.h"
#include "videodataoutput.h"

#include <kdebug.h>
#include <kgenericfactory.h>

#include <QSet>
#include <QVariant>

#include <phonon/audiodevice.h>
#include <phonon/audiodeviceenumerator.h>

typedef KGenericFactory<Phonon::Xine::Backend> XineBackendFactory;
K_EXPORT_COMPONENT_FACTORY(phonon_xine_threaded, XineBackendFactory("xinebackend"))

namespace Phonon
{
namespace Xine
{

Backend::Backend( QObject* parent, const QStringList& )
	: QObject( parent )
{
    XineEngine::setConfig(XineBackendFactory::componentData().config());
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
            {
                QList<AudioDevice> devlist = AudioDeviceEnumerator::availableCaptureDevices();
                foreach (AudioDevice dev, devlist) {
                    set << dev.index();
                }
            }
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

QHash<QByteArray, QVariant> Backend::objectDescriptionProperties(ObjectDescriptionType type, int index) const
{
    kDebug(610) << k_funcinfo << type << index << endl;
    QHash<QByteArray, QVariant> ret;
    switch (type) {
        case Phonon::AudioOutputDeviceType:
            {
                ret.insert("name", XineEngine::audioOutputName(index));
                ret.insert("description", XineEngine::audioOutputDescription(index));
                QString icon = XineEngine::audioOutputIcon(index);
                if (!icon.isEmpty()) {
                    ret.insert("icon", icon);
                }
                ret.insert("available", XineEngine::audioOutputAvailable(index));
            }
            break;
        case Phonon::AudioCaptureDeviceType:
            {
                QList<AudioDevice> devlist = AudioDeviceEnumerator::availableCaptureDevices();
                foreach (AudioDevice dev, devlist) {
                    if (dev.index() == index) {
                        ret.insert("name", dev.cardName());
                        switch (dev.driver()) {
                            case Solid::AudioHw::Alsa:
                                ret.insert("description", i18n("ALSA Capture Device"));
                                break;
                            case Solid::AudioHw::OpenSoundSystem:
                                ret.insert("description", i18n("OSS Capture Device"));
                                break;
                            case Solid::AudioHw::UnknownAudioDriver:
                                break;
                        }
                        ret.insert("icon", dev.iconName());
                        ret.insert("available", dev.isAvailable());
                        break;
                    }
                }
            }
            switch (index) {
                case 20000:
                    ret.insert("name", QLatin1String("Soundcard"));
                    break;
                case 20001:
                    ret.insert("name", QLatin1String("DV"));
                    break;
            }
            kDebug(610) << ret["name"] << endl;
            break;
        case Phonon::VideoOutputDeviceType:
            {
                const char *const *outputPlugins = xine_list_video_output_plugins(XineEngine::xine());
                for (int i = 0; outputPlugins[i]; ++i) {
                    if (40000 + i == index) {
                        ret.insert("name", QLatin1String(outputPlugins[i]));
                        ret.insert("description", QLatin1String(xine_get_video_driver_plugin_description(XineEngine::xine(), outputPlugins[i])));
                        break;
                    }
                }
            }
            break;
        case Phonon::VideoCaptureDeviceType:
            switch (index) {
                case 30000:
                    ret.insert("name", "USB Webcam");
                    ret.insert("description", "first description");
                    break;
                case 30001:
                    ret.insert("name", "DV");
                    ret.insert("description", "second description");
                    break;
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
                const char *const *postPlugins = xine_list_post_plugins_typed(XineEngine::xine(), XINE_POST_TYPE_AUDIO_FILTER);
                for (int i = 0; postPlugins[i]; ++i) {
                    if (0x7F000000 + i == index) {
                        ret.insert("name", QLatin1String(postPlugins[i]));
                        ret.insert("description", QLatin1String(xine_get_post_plugin_description(XineEngine::xine(), postPlugins[i])));
                        break;
                    }
                }
            }
            break;
        case Phonon::VideoEffectType:
            {
                const char *const *postPlugins = xine_list_post_plugins_typed(XineEngine::xine(), XINE_POST_TYPE_VIDEO_FILTER);
                for (int i = 0; postPlugins[i]; ++i) {
                    if (0x7E000000 + i == index) {
                        ret.insert("name", QLatin1String(postPlugins[i]));
                        break;
                    }
                }
            }
            break;
    }
    return ret;
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
