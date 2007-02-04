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

#include "xineengine.h"

//#include <kdebug.h>
#include "abstractmediaproducer.h"
#include <QCoreApplication>
#include <phonon/audiodeviceenumerator.h>
#include <phonon/audiodevice.h>
#include <solid/devicemanager.h>
#include <solid/device.h>
#include <solid/audiohw.h>
#include <QList>
#include <kconfiggroup.h>
#include "videowidgetinterface.h"

namespace Phonon
{
namespace Xine
{

	XineProgressEvent::XineProgressEvent( const QString& description, int percent )
		: QEvent( static_cast<QEvent::Type>( Xine::ProgressEvent ) )
		, m_description( description )
		, m_percent( percent )
	{
	}

	const QString& XineProgressEvent::description() 
	{
		return m_description;
	}

	int XineProgressEvent::percent()
	{
		return m_percent;
	}

    static XineEngine *s_instance = 0;

    XineEngine::XineEngine(const KSharedConfigPtr& _config)
        : m_xine(xine_new()),
        m_config(_config)
    {
        s_instance = this;
        KConfigGroup cg(m_config, "Settings");
        m_useOss = cg.readEntry("showOssDevices", false);
    }

    XineEngine::~XineEngine()
    {
        //kDebug(610) << k_funcinfo << endl;
        s_instance = 0;
        xine_exit(m_xine);
        m_xine = 0;
    }

    XineEngine *XineEngine::self()
    {
        Q_ASSERT(s_instance);
        return s_instance;
    }

	xine_t* XineEngine::xine()
	{
		return self()->m_xine;
	}

	void XineEngine::xineEventListener( void *p, const xine_event_t* xineEvent )
	{
		if( !p || !xineEvent )
			return;
		//kDebug( 610 ) << "Xine event: " << xineEvent->type << QByteArray( ( char* )xineEvent->data, xineEvent->data_length ) << endl;

        XineStream *xs = static_cast<XineStream *>(p);

		switch( xineEvent->type ) 
		{
			case XINE_EVENT_UI_SET_TITLE:
                QCoreApplication::postEvent(xs, new QEvent(static_cast<QEvent::Type>(Xine::NewMetaDataEvent)));
				break;
			case XINE_EVENT_UI_PLAYBACK_FINISHED:
                QCoreApplication::postEvent(xs, new QEvent(static_cast<QEvent::Type>(Xine::MediaFinishedEvent)));
				break;
			case XINE_EVENT_PROGRESS:
				{
					xine_progress_data_t* progress = static_cast<xine_progress_data_t*>( xineEvent->data );
                    QCoreApplication::postEvent(xs, new XineProgressEvent(QString::fromUtf8(progress->description), progress->percent));
				}
				break;
            case XINE_EVENT_SPU_BUTTON:
                {
                    VideoWidgetInterface *vw = xs->videoWidget();
                    if (vw) {
                        xine_spu_button_t *button = static_cast<xine_spu_button_t *>(xineEvent->data);
                        if (button->direction == 1) { // enter a button
                            QCoreApplication::postEvent(vw->qobject(), new QEvent(static_cast<QEvent::Type>(Xine::NavButtonInEvent)));
                        } else {
                            QCoreApplication::postEvent(vw->qobject(), new QEvent(static_cast<QEvent::Type>(Xine::NavButtonOutEvent)));
                        }
                    }
                }
		}
	}

    QSet<int> XineEngine::audioOutputIndexes()
    {
        XineEngine *that = self();
        that->checkAudioOutputs();
        QSet<int> set;
        for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
            //if (that->m_audioOutputInfos[i].available) {
                set << that->m_audioOutputInfos[i].index;
            //}
        }
        return set;
    }

    QString XineEngine::audioOutputName(int audioDevice)
    {
        XineEngine *that = self();
        that->checkAudioOutputs();
        for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
            if (that->m_audioOutputInfos[i].index == audioDevice) {
                return that->m_audioOutputInfos[i].name;
            }
        }
        return QString();
    }

    QString XineEngine::audioOutputDescription(int audioDevice)
    {
        XineEngine *that = self();
        that->checkAudioOutputs();
        for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
            if (that->m_audioOutputInfos[i].index == audioDevice) {
                return that->m_audioOutputInfos[i].description;
            }
        }
        return QString();
    }

    QString XineEngine::audioOutputIcon(int audioDevice)
    {
        XineEngine *that = self();
        that->checkAudioOutputs();
        for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
            if (that->m_audioOutputInfos[i].index == audioDevice) {
                return that->m_audioOutputInfos[i].icon;
            }
        }
        return QString();
    }
    bool XineEngine::audioOutputAvailable(int audioDevice)
    {
        XineEngine *that = self();
        that->checkAudioOutputs();
        for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
            if (that->m_audioOutputInfos[i].index == audioDevice) {
                return that->m_audioOutputInfos[i].available;
            }
        }
        return false;
    }

    QString XineEngine::audioDriverFor(int audioDevice)
    {
        XineEngine *that = self();
        that->checkAudioOutputs();
        for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
            if (that->m_audioOutputInfos[i].index == audioDevice) {
                return that->m_audioOutputInfos[i].driver;
            }
        }
        return QString();
    }

    QStringList XineEngine::alsaDevicesFor(int audioDevice)
    {
        XineEngine *that = self();
        that->checkAudioOutputs();
        for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
            if (that->m_audioOutputInfos[i].index == audioDevice) {
                if (that->m_audioOutputInfos[i].driver == "alsa") { // only for ALSA
                    return that->m_audioOutputInfos[i].devices;
                }
            }
        }
        return QStringList();
    }

    void XineEngine::addAudioOutput(AudioDevice dev, QString driver)
    {
        QString postfix;
        if (m_useOss) {
            if (dev.driver() == Solid::AudioHw::Alsa) {
                postfix = QLatin1String(" (ALSA)");
            } else if (dev.driver() == Solid::AudioHw::OpenSoundSystem) {
                postfix = QLatin1String(" (OSS)");
            }
        }
        AudioOutputInfo info(dev.index(), dev.cardName() + postfix,
                QString(), dev.iconName(), driver, dev.deviceIds());
        info.available = dev.isAvailable();
        m_audioOutputInfos << info;
    }

    void XineEngine::addAudioOutput(int index, const QString &name, const QString &description,
            const QString &icon, const QString &driver, const QStringList &deviceIds)
    {
        AudioOutputInfo info(index, name, description, icon, driver, deviceIds);
        const int listIndex = m_audioOutputInfos.indexOf(info);
        if (listIndex == -1) {
            info.available = true;
            m_audioOutputInfos << info;
            KConfigGroup config(m_config, QLatin1String("AudioOutputDevice_") + QString::number(index));
            config.writeEntry("name", name);
            config.writeEntry("description", description);
            config.writeEntry("driver", driver);
            config.writeEntry("icon", icon);
        } else {
            m_audioOutputInfos[listIndex].devices = deviceIds;
            m_audioOutputInfos[listIndex].available = true;
        }
    }

    void XineEngine::checkAudioOutputs()
    {
        kDebug(610) << k_funcinfo << endl;
        if (m_audioOutputInfos.isEmpty()) {
            kDebug(610) << "isEmpty" << endl;
            QStringList groups = m_config->groupList();
            int nextIndex = 10000;
            foreach (QString group, groups) {
                if (group.startsWith("AudioOutputDevice")) {
                    const int index = group.right(group.size() - 18).toInt();
                    if (index >= nextIndex) {
                        nextIndex = index + 1;
                    }
                    KConfigGroup config(m_config, group);
                    m_audioOutputInfos << AudioOutputInfo(index,
                            config.readEntry("name", QString()),
                            config.readEntry("description", QString()),
                            config.readEntry("icon", QString()),
                            config.readEntry("driver", QString()),
                            QStringList()); // the device list can change and needs to be queried
                                            // from the actual hardware configuration
                }
            }

            // This will list the audio drivers, not the actual devices.
            const char *const *outputPlugins = xine_list_audio_output_plugins(xine());
            for (int i = 0; outputPlugins[i]; ++i) {
                kDebug(610) << "outputPlugin: " << outputPlugins[i] << endl;
                if (0 == strcmp(outputPlugins[i], "alsa")) {
                    QList<AudioDevice> alsaDevices = AudioDeviceEnumerator::availablePlaybackDevices();
                    foreach (AudioDevice dev, alsaDevices) {
                        if (dev.driver() == Solid::AudioHw::Alsa) {
                            addAudioOutput(dev, QLatin1String("alsa"));
                        }
                    }
                } else if (0 == strcmp(outputPlugins[i], "none") || 0 == strcmp(outputPlugins[i], "file")) {
                    // ignore these devices
                } else if (0 == strcmp(outputPlugins[i], "oss")) {
                    if (m_useOss) {
                        QList<AudioDevice> audioDevices = AudioDeviceEnumerator::availablePlaybackDevices();
                        foreach (AudioDevice dev, audioDevices) {
                            if (dev.driver() == Solid::AudioHw::OpenSoundSystem) {
                                addAudioOutput(dev, QLatin1String("oss"));
                            }
                        }
                    }
                } else {
                    addAudioOutput(nextIndex++, outputPlugins[i],
                            xine_get_audio_driver_plugin_description(xine(), outputPlugins[i]),
                            QString(), outputPlugins[i], QStringList());
                }
            }

            // now m_audioOutputInfos holds all devices this computer has ever seen
            foreach (AudioOutputInfo info, m_audioOutputInfos) {
                kDebug(610) << info.index << info.name << info.driver << info.devices << endl;
            }
        }
    }
}
}

// vim: sw=4 ts=4 tw=80 et
