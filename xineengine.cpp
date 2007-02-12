/*  This file is part of the KDE project
    Copyright (C) 2006 Tim Beaulen <tbscope@gmail.com>
    Copyright (C) 2006-2007 Matthias Kretz <kretz@kde.org>

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
#include <klocale.h>
#include "xineengine_p.h"
#include "backend.h"

namespace Phonon
{
namespace Xine
{
    XineEnginePrivate::XineEnginePrivate()
    {
        signalTimer.setSingleShot(true);
        connect(&signalTimer, SIGNAL(timeout()), SLOT(emitAudioDeviceChange()));
    }

    void XineEnginePrivate::emitAudioDeviceChange()
    {
        kDebug(610) << k_funcinfo << endl;
        emit objectDescriptionChanged(AudioOutputDeviceType);
    }

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

    XineFrameFormatChangeEvent::XineFrameFormatChangeEvent(int w, int h, int aspect, bool panScan)
        : QEvent(static_cast<QEvent::Type>(FrameFormatChangeEvent)),
        m_size(w, h),
        m_aspect(aspect),
        m_panScan(panScan)
    {
    }

    static XineEngine *s_instance = 0;

    XineEngine::XineEngine(const KSharedConfigPtr& _config)
        : m_xine(xine_new()),
        m_config(_config),
        d(new XineEnginePrivate)
    {
        Q_ASSERT(s_instance == 0);
        s_instance = this;
        KConfigGroup cg(m_config, "Settings");
        m_useOss = cg.readEntry("showOssDevices", false);
    }

    XineEngine::~XineEngine()
    {
        //kDebug(610) << k_funcinfo << endl;
        xine_exit(m_xine);
        m_xine = 0;
        s_instance = 0;
    }

    XineEngine *XineEngine::self()
    {
        Q_ASSERT(s_instance);
        return s_instance;
    }

    QObject *XineEngine::sender()
    {
        return self()->d;
    }

	xine_t* XineEngine::xine()
	{
		return self()->m_xine;
	}

    void XineEngine::xineEventListener(void *p, const xine_event_t *xineEvent)
    {
        if (!p || !xineEvent) {
            return;
        }
        //kDebug( 610 ) << "Xine event: " << xineEvent->type << QByteArray((char *)xineEvent->data, xineEvent->data_length) << endl;

        XineStream *xs = static_cast<XineStream *>(p);

        switch (xineEvent->type) {
            case XINE_EVENT_UI_SET_TITLE: /* request title display change in ui */
                QCoreApplication::postEvent(xs, new QEvent(static_cast<QEvent::Type>(Xine::NewMetaDataEvent)));
                break;
            case XINE_EVENT_UI_PLAYBACK_FINISHED: /* frontend can e.g. move on to next playlist entry */
                QCoreApplication::postEvent(xs, new QEvent(static_cast<QEvent::Type>(Xine::MediaFinishedEvent)));
                break;
            case XINE_EVENT_PROGRESS: /* index creation/network connections */
                {
                    xine_progress_data_t *progress = static_cast<xine_progress_data_t*>(xineEvent->data);
                    QCoreApplication::postEvent(xs, new XineProgressEvent(QString::fromUtf8(progress->description), progress->percent));
                }
                break;
            case XINE_EVENT_SPU_BUTTON: // the mouse pointer enter/leave a button, used to change the cursor
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
                break;
            case XINE_EVENT_UI_CHANNELS_CHANGED:    /* inform ui that new channel info is available */
                kDebug(610) << "XINE_EVENT_UI_CHANNELS_CHANGED" << endl;
                break;
            case XINE_EVENT_UI_MESSAGE:             /* message (dialog) for the ui to display */
                kDebug(610) << "XINE_EVENT_UI_MESSAGE" << endl;
                break;
            case XINE_EVENT_FRAME_FORMAT_CHANGE:    /* e.g. aspect ratio change during dvd playback */
                kDebug(610) << "XINE_EVENT_FRAME_FORMAT_CHANGE" << endl;
                {
                    VideoWidgetInterface *vw = xs->videoWidget();
                    if (vw) {
                        xine_format_change_data_t *data = static_cast<xine_format_change_data_t *>(xineEvent->data);
                        QCoreApplication::postEvent(vw->qobject(), new XineFrameFormatChangeEvent(data->width, data->height, data->aspect, data->pan_scan));
                    }
                }
                break;
            case XINE_EVENT_AUDIO_LEVEL:            /* report current audio level (l/r/mute) */
                kDebug(610) << "XINE_EVENT_AUDIO_LEVEL" << endl;
                break;
            case XINE_EVENT_QUIT:                   /* last event sent when stream is disposed */
                kDebug(610) << "XINE_EVENT_QUIT" << endl;
                break;
            case XINE_EVENT_UI_NUM_BUTTONS:         /* number of buttons for interactive menus */
                kDebug(610) << "XINE_EVENT_UI_NUM_BUTTONS" << endl;
                break;
            case XINE_EVENT_DROPPED_FRAMES:         /* number of dropped frames is too high */
                kDebug(610) << "XINE_EVENT_DROPPED_FRAMES" << endl;
                break;
            case XINE_EVENT_MRL_REFERENCE_EXT:      /* demuxer->frontend: MRL reference(s) for the real stream */
                kDebug(610) << "XINE_EVENT_MRL_REFERENCE_EXT" << endl;
                break;
#ifdef XINE_EVENT_AUDIO_DEVICE_FAILED
            case XINE_EVENT_AUDIO_DEVICE_FAILED:    /* audio device is gone */
                kDebug(610) << "XINE_EVENT_AUDIO_DEVICE_FAILED" << endl;
                {
                    AudioPort ap = xs->audioPort();
                    if (ap.audioOutput()) {
                        QCoreApplication::postEvent(ap.audioOutput(), new QEvent(static_cast<QEvent::Type>(Xine::AudioDeviceFailedEvent)));
                    }
                }
                break;
#endif
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
        if (m_audioOutputInfos.contains(info)) {
            m_audioOutputInfos.removeAll(info); // the latest is more up to date wrt availability
        }
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
            QObject::connect(AudioDeviceEnumerator::self(), SIGNAL(devicePlugged(const AudioDevice &)),
                    d, SLOT(devicePlugged(const AudioDevice &)));
            QObject::connect(AudioDeviceEnumerator::self(), SIGNAL(deviceUnplugged(const AudioDevice &)),
                    d, SLOT(deviceUnplugged(const AudioDevice &)));
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
                } else if (0 == strcmp(outputPlugins[i], "jack")) {
                    addAudioOutput(nextIndex++, i18n("Jack Audio Connection Kit"),
                            i18n("<p>JACK is a low-latency audio server. It can connect a number "
                                "of different applications to an audio device, as well as allowing "
                                "them to share audio between themselves.</p>"
                                "<p>JACK was designed from the ground up for professional audio "
                                "work, and its design focuses on two key areas: synchronous "
                                "execution of all clients, and low latency operation.</p>"),
                            outputPlugins[i], outputPlugins[i], QStringList());
                } else if (0 == strcmp(outputPlugins[i], "arts")) {
                    addAudioOutput(nextIndex++, i18n("aRts"),
                            i18n("<p>aRts is the old soundserver and media framework that was used "
                                "in KDE2 and KDE3. Its use is discuraged.</p>"),
                            outputPlugins[i], outputPlugins[i], QStringList());
                } else {
                    addAudioOutput(nextIndex++, outputPlugins[i],
                            xine_get_audio_driver_plugin_description(xine(), outputPlugins[i]),
                            outputPlugins[i], outputPlugins[i], QStringList());
                }
            }

            // now m_audioOutputInfos holds all devices this computer has ever seen
            foreach (AudioOutputInfo info, m_audioOutputInfos) {
                kDebug(610) << info.index << info.name << info.driver << info.devices << endl;
            }
        }
    }

    void XineEnginePrivate::devicePlugged(const AudioDevice &dev)
    {
        kDebug(610) << k_funcinfo << dev.cardName() << endl;
        if (!dev.isPlaybackDevice()) {
            return;
        }
        const char *const *outputPlugins = xine_list_audio_output_plugins(XineEngine::xine());
        switch (dev.driver()) {
            case Solid::AudioHw::Alsa:
                for (int i = 0; outputPlugins[i]; ++i) {
                    if (0 == strcmp(outputPlugins[i], "alsa")) {
                        s_instance->addAudioOutput(dev, QLatin1String("alsa"));
                        signalTimer.start();
                    }
                }
                break;
            case Solid::AudioHw::OpenSoundSystem:
                if (s_instance->m_useOss) {
                    for (int i = 0; outputPlugins[i]; ++i) {
                        if (0 == strcmp(outputPlugins[i], "oss")) {
                            s_instance->addAudioOutput(dev, QLatin1String("oss"));
                            signalTimer.start();
                        }
                    }
                }
                break;
            case Solid::AudioHw::UnknownAudioDriver:
                break;
        }
    }

    void XineEnginePrivate::deviceUnplugged(const AudioDevice &dev)
    {
        kDebug(610) << k_funcinfo << dev.cardName() << endl;
        if (!dev.isPlaybackDevice()) {
            return;
        }
        QString driver;
        QString postfix;
        switch (dev.driver()) {
            case Solid::AudioHw::Alsa:
                driver = "alsa";
                if (s_instance->m_useOss) {
                    postfix = QLatin1String(" (ALSA)");
                }
                break;
            case Solid::AudioHw::OpenSoundSystem:
                driver = "oss";
                postfix = QLatin1String(" (OSS)");
                break;
            case Solid::AudioHw::UnknownAudioDriver:
                break;
        }
        XineEngine::AudioOutputInfo info(dev.index(), dev.cardName() + postfix, QString(), dev.iconName(),
                driver, dev.deviceIds());
        if (s_instance->m_audioOutputInfos.removeAll(info)) {
            signalTimer.start();
        } else {
            kDebug(610) << k_funcinfo << "told to remove " << dev.cardName() + postfix <<
                " with driver " << driver << " but the device was not present in m_audioOutputInfos"
                << endl;
        }
    }
} // namespace Xine
} // namespace Phonon

#include "xineengine_p.moc"
// vim: sw=4 ts=4 tw=100 et
