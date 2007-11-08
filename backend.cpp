/* This file is part of the KDE project
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

#include "backend.h"
#include "mediaobject.h"
#include "effect.h"
#include "events.h"
#include "audiooutput.h"
#include "audiodataoutput.h"
#include "nullsink.h"
#include "visualization.h"
#include "volumefadereffect.h"
#include "videodataoutput.h"
#include "videowidget.h"
#include "wirecall.h"
#include "xinethread.h"
#include "keepreference.h"
#include "sinknode.h"
#include "sourcenode.h"

#include <kconfiggroup.h>
#include <kdebug.h>
#include <klocale.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

#include <QtCore/QByteArray>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#include <QtCore/QSet>
#include <QtCore/QVariant>
#include <QtDBus/QDBusConnection>
#include <QtGui/QApplication>

#include <phonon/audiodevice.h>
#include <phonon/audiodeviceenumerator.h>

K_PLUGIN_FACTORY(XineBackendFactory, registerPlugin<Phonon::Xine::Backend>();)
K_EXPORT_PLUGIN(XineBackendFactory("xinebackend"))

static Phonon::Xine::Backend *s_instance = 0;

namespace Phonon
{
namespace Xine
{

Backend *Backend::instance()
{
    Q_ASSERT(s_instance);
    return s_instance;
}

Backend::Backend(QObject *parent, const QVariantList &)
    : QObject(parent),
    m_config(XineBackendFactory::componentData().config()),
    m_useOss(Backend::Unknown),
    m_thread(0)
{
    Q_ASSERT(s_instance == 0);
    s_instance = this;

    m_xine.create();
    m_freeEngines << m_xine;

    setProperty("identifier",     QLatin1String("phonon_xine"));
    setProperty("backendName",    QLatin1String("Xine"));
    setProperty("backendComment", i18n("Phonon Xine Backend"));
    setProperty("backendVersion", QLatin1String("0.1"));
    setProperty("backendIcon",    QLatin1String("phonon-xine"));
    setProperty("backendWebsite", QLatin1String("http://multimedia.kde.org/"));

    KConfigGroup cg(m_config, "Settings");
    m_deinterlaceDVD = cg.readEntry("deinterlaceDVD", true);
    m_deinterlaceVCD = cg.readEntry("deinterlaceVCD", false);
    m_deinterlaceFile = cg.readEntry("deinterlaceFile", false);
    m_deinterlaceMethod = cg.readEntry("deinterlaceMethod", 0);

    signalTimer.setSingleShot(true);
    connect(&signalTimer, SIGNAL(timeout()), SLOT(emitAudioDeviceChange()));
    QDBusConnection::sessionBus().registerObject("/internal/PhononXine", this, QDBusConnection::ExportScriptableSlots);

    kDebug(610) << "Using Xine version " << xine_get_version_string();
}

Backend::~Backend()
{
    if (!m_cleanupObjects.isEmpty()) {
        Q_ASSERT(m_thread);
        QCoreApplication::postEvent(m_thread, new Event(Event::Cleanup));
        while (!m_cleanupObjects.isEmpty()) {
            XineThread::msleep(200); // static QThread::msleep, but that one is protected and XineThread is our friend
        }
    }
//X     QList<QObject *> cleanupObjects(m_cleanupObjects);
//X     const QList<QObject *>::Iterator end = cleanupObjects.end();
//X     QList<QObject *>::Iterator it = cleanupObjects.begin();
//X     while (it != end) {
//X         kDebug(610) << "delete" << *it;
//X         delete *it;
//X         ++it;
//X     }
    //qDeleteAll(cleanupObjects);
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
    }
    //kDebug(610) ;
    s_instance = 0;
}

XineEngine Backend::xineEngineForStream()
{
    XineEngine e;
    if (s_instance->m_freeEngines.isEmpty()) {
        e.create();
    } else {
        e = s_instance->m_freeEngines.takeLast();
    }
    s_instance->m_usedEngines << e;
    return e;
}

void Backend::returnXineEngine(const XineEngine &e)
{
    s_instance->m_usedEngines.removeAll(e);
    s_instance->m_freeEngines << e;
    if (s_instance->m_freeEngines.size() > 5) {
        s_instance->m_freeEngines.takeLast();
        s_instance->m_freeEngines.takeLast();
        s_instance->m_freeEngines.takeLast();
    }
}

QObject *Backend::createObject(BackendInterface::Class c, QObject *parent, const QList<QVariant> &args)
{
    switch (c) {
    case MediaObjectClass:
        return new MediaObject(parent);
    case VolumeFaderEffectClass:
        return new VolumeFaderEffect(parent);
    case AudioOutputClass:
        return new AudioOutput(parent);
    case AudioDataOutputClass:
        return new AudioDataOutput(parent);
    case VisualizationClass:
        return new Visualization(parent);
    case VideoDataOutputClass:
        return new VideoDataOutput(parent);
    case EffectClass:
        {
            Q_ASSERT(args.size() == 1);
            kDebug(610) << "creating Effect(" << args[0];
            Effect *e = new Effect(args[0].toInt(), parent);
            if (e->isValid()) {
                return e;
            }
            delete e;
            return 0;
        }
    case VideoWidgetClass:
        {
            VideoWidget *vw = new VideoWidget(qobject_cast<QWidget *>(parent));
            if (vw->isValid()) {
                return vw;
            }
            delete vw;
            return 0;
        }
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

bool Backend::supportsFourcc(quint32 fourcc) const
{
    switch(fourcc)
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

QStringList Backend::availableMimeTypes() const
{
    if (m_supportedMimeTypes.isEmpty())
    {
        char *mimeTypes_c = xine_get_mime_types(m_xine);
        QString mimeTypes(mimeTypes_c);
        free(mimeTypes_c);
        QStringList lstMimeTypes = mimeTypes.split(";", QString::SkipEmptyParts);
        foreach (const QString &mimeType, lstMimeTypes) {
            m_supportedMimeTypes << mimeType.left(mimeType.indexOf(':')).trimmed();
        }
        if (m_supportedMimeTypes.contains("application/ogg")) {
            m_supportedMimeTypes << QLatin1String("audio/x-vorbis+ogg") << QLatin1String("application/ogg");
        }
    }

    return m_supportedMimeTypes;
}

QList<int> Backend::objectDescriptionIndexes(ObjectDescriptionType type) const
{
    QList<int> list;
    switch(type)
    {
    case Phonon::AudioOutputDeviceType:
        return Backend::audioOutputIndexes();
/*
    case Phonon::AudioCaptureDeviceType:
        {
            QList<AudioDevice> devlist = AudioDeviceEnumerator::availableCaptureDevices();
            foreach (AudioDevice dev, devlist) {
                list << dev.index();
            }
        }
        break;
    case Phonon::VideoOutputDeviceType:
        {
            const char *const *outputPlugins = xine_list_video_output_plugins(m_xine);
            for (int i = 0; outputPlugins[i]; ++i)
                list << 40000 + i;
            break;
        }
    case Phonon::VideoCaptureDeviceType:
        list << 30000 << 30001;
        break;
    case Phonon::VisualizationType:
        break;
    case Phonon::AudioCodecType:
        break;
    case Phonon::VideoCodecType:
        break;
    case Phonon::ContainerFormatType:
        break;
        */
    case Phonon::EffectType:
        {
            const char *const *postPlugins = xine_list_post_plugins_typed(m_xine, XINE_POST_TYPE_AUDIO_FILTER);
            for (int i = 0; postPlugins[i]; ++i)
                list << 0x7F000000 + i;
            /*const char *const *postVPlugins = xine_list_post_plugins_typed(m_xine, XINE_POST_TYPE_VIDEO_FILTER);
            for (int i = 0; postVPlugins[i]; ++i) {
                list << 0x7E000000 + i;
            } */
        }
    }
    return list;
}

QHash<QByteArray, QVariant> Backend::objectDescriptionProperties(ObjectDescriptionType type, int index) const
{
    //kDebug(610) << type << index;
    QHash<QByteArray, QVariant> ret;
    switch (type) {
    case Phonon::AudioOutputDeviceType:
        {
            ret.insert("name", Backend::audioOutputName(index));
            ret.insert("description", Backend::audioOutputDescription(index));
            QString icon = Backend::audioOutputIcon(index);
            if (!icon.isEmpty()) {
                ret.insert("icon", icon);
            }
            ret.insert("available", Backend::audioOutputAvailable(index));
            QVariant mixer = Backend::audioOutputMixerDevice(index);
            if (mixer.isValid()) {
                ret.insert("mixerDeviceId", mixer);
            }
            ret.insert("initialPreference", Backend::audioOutputInitialPreference(index));
        }
        break;
        /*
    case Phonon::AudioCaptureDeviceType:
        {
            QList<AudioDevice> devlist = AudioDeviceEnumerator::availableCaptureDevices();
            foreach (AudioDevice dev, devlist) {
                if (dev.index() == index) {
                    ret.insert("name", dev.cardName());
                    switch (dev.driver()) {
                    case Solid::AudioInterface::Alsa:
                        ret.insert("description", i18n("ALSA Capture Device"));
                        break;
                    case Solid::AudioInterface::OpenSoundSystem:
                        ret.insert("description", i18n("OSS Capture Device"));
                        break;
                    case Solid::AudioInterface::UnknownAudioDriver:
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
        //kDebug(610) << ret["name"];
        break;
    case Phonon::VideoOutputDeviceType:
        {
            const char *const *outputPlugins = xine_list_video_output_plugins(m_xine);
            for (int i = 0; outputPlugins[i]; ++i) {
                if (40000 + i == index) {
                    ret.insert("name", QLatin1String(outputPlugins[i]));
                    ret.insert("description", "");
                    // description should be the result of the following call, but it crashes.
                    // It looks like libxine initializes the plugin even when we just want the description...
                    //QLatin1String(xine_get_video_driver_plugin_description(m_xine, outputPlugins[i])));
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
        */
    case Phonon::EffectType:
        {
            const char *const *postPlugins = xine_list_post_plugins_typed(m_xine, XINE_POST_TYPE_AUDIO_FILTER);
            for (int i = 0; postPlugins[i]; ++i) {
                if (0x7F000000 + i == index) {
                    ret.insert("name", QLatin1String(postPlugins[i]));
                    ret.insert("description", QLatin1String(xine_get_post_plugin_description(m_xine, postPlugins[i])));
                    break;
                }
            }
            /*const char *const *postVPlugins = xine_list_post_plugins_typed(m_xine, XINE_POST_TYPE_VIDEO_FILTER);
            for (int i = 0; postVPlugins[i]; ++i) {
                if (0x7E000000 + i == index) {
                    ret.insert("name", QLatin1String(postPlugins[i]));
                    break;
                }
            } */
        }
    }
    return ret;
}

bool Backend::startConnectionChange(QSet<QObject *> nodes)
{
    Q_UNUSED(nodes);
    // there's nothing we can do but hope the connection changes won't take too long so that buffers
    // would underrun. But we should be pretty safe the way xine works by not doing anything here.
    m_disconnections.clear();
    return true;
}

bool Backend::connectNodes(QObject *_source, QObject *_sink)
{
    kDebug(610);
    SourceNode *source = qobject_cast<SourceNode *>(_source);
    SinkNode *sink = qobject_cast<SinkNode *>(_sink);
    if (!source || !sink) {
        return false;
    }
    kDebug(610) << source->threadSafeObject().data() << "->" << sink->threadSafeObject().data();
    // what streams to connect - i.e. all both nodes support
    const MediaStreamTypes types = source->outputMediaStreamTypes() & sink->inputMediaStreamTypes();
    if (sink->source() != 0 || source->sinks().contains(sink)) {
        return false;
    }
    NullSink *nullSink = 0;
    foreach (SinkNode *otherSinks, source->sinks()) {
        if (otherSinks->inputMediaStreamTypes() & types) {
            if (nullSink) {
                kWarning(610) << "phonon-xine does not support splitting of audio or video streams into multiple outputs. The sink node is already connected to" << otherSinks->threadSafeObject().data();
                return false;
            } else {
                nullSink = dynamic_cast<NullSink *>(otherSinks);
                if (!nullSink) {
                    kWarning(610) << "phonon-xine does not support splitting of audio or video streams into multiple outputs. The sink node is already connected to" << otherSinks->threadSafeObject().data();
                    return false;
                }
            }
        }
    }
    if (nullSink) {
        m_disconnections << WireCall(source, nullSink);
        source->removeSink(nullSink);
        nullSink->unsetSource(source);
    }
    source->addSink(sink);
    sink->setSource(source);
    return true;
}

bool Backend::disconnectNodes(QObject *_source, QObject *_sink)
{
    kDebug(610);
    SourceNode *source = qobject_cast<SourceNode *>(_source);
    SinkNode *sink = qobject_cast<SinkNode *>(_sink);
    if (!source || !sink) {
        return false;
    }
    const MediaStreamTypes types = source->outputMediaStreamTypes() & sink->inputMediaStreamTypes();
    if (!source->sinks().contains(sink) || sink->source() != source) {
        return false;
    }
    m_disconnections << WireCall(source, sink);
    source->removeSink(sink);
    sink->unsetSource(source);
    return true;
}

bool Backend::endConnectionChange(QSet<QObject *> nodes)
{
    QList<WireCall> wireCallsUnordered;
    QList<WireCall> wireCalls;
    KeepReference<> *keep = new KeepReference<>();

    // first we need to find all vertices of the subgraphs formed by the given nodes that are not
    // source nodes but don't have a sink node connected and connect them to the NullSink, otherwise
    // disconnections won't work
    QSet<QObject *> nullSinks;
    foreach (QObject *q, nodes) {
        SourceNode *source = qobject_cast<SourceNode *>(q);
        if (source && source->sinks().isEmpty()) {
            SinkNode *sink = qobject_cast<SinkNode *>(q);
            if (!sink || (sink && sink->source())) {
                NullSink *nullsink = new NullSink(q);
                source->addSink(nullsink);
                nullsink->setSource(source);
                nullSinks << nullsink;
            }
        }
    }
    nodes += nullSinks;

    // Now that we know (by looking at the subgraph of nodes formed by the given nodes) what has to
    // be rewired we go over the nodes in order (from sink to source) and rewire them (all called
    // from the xine thread).
    foreach (QObject *q, nodes) {
        SourceNode *source = qobject_cast<SourceNode *>(q);
        if (source) {
            //keep->addObject(source->threadSafeObject());
            foreach (SinkNode *sink, source->sinks()) {
                WireCall w(source, sink);
                if (wireCallsUnordered.contains(w)) {
                    Q_ASSERT(!wireCalls.contains(w));
                    wireCalls << w;
                } else {
                    wireCallsUnordered << w;
                }
            }
        }
        SinkNode *sink = qobject_cast<SinkNode *>(q);
        if (sink) {
            keep->addObject(sink->threadSafeObject());
            if (sink->source()) {
                WireCall w(sink->source(), sink);
                if (wireCallsUnordered.contains(w)) {
                    Q_ASSERT(!wireCalls.contains(w));
                    wireCalls << w;
                } else {
                    wireCallsUnordered << w;
                }
            }
            sink->findXineEngine();
        }
        ConnectNotificationInterface *connectNotify = qobject_cast<ConnectNotificationInterface *>(q);
        if (connectNotify) {
            // the object wants to know when the graph has changed
            connectNotify->graphChanged();
        }
    }
    if (!wireCalls.isEmpty()) {
        qSort(wireCalls);
    }
    QCoreApplication::postEvent(XineThread::instance(), new RewireEvent(wireCalls, m_disconnections));
    m_disconnections.clear();
    keep->ready();
    return true;
}

void Backend::freeSoundcardDevices()
{
}

void Backend::emitAudioDeviceChange()
{
    kDebug(610);
    emit objectDescriptionChanged(AudioOutputDeviceType);
}

bool Backend::deinterlaceDVD()
{
    return s_instance->m_deinterlaceDVD;
}

bool Backend::deinterlaceVCD()
{
    return s_instance->m_deinterlaceVCD;
}

bool Backend::deinterlaceFile()
{
    return s_instance->m_deinterlaceFile;
}

int Backend::deinterlaceMethod()
{
    return s_instance->m_deinterlaceMethod;
}

QList<int> Backend::audioOutputIndexes()
{
    Backend *that = instance();
    that->checkAudioOutputs();
    QList<int> list;
    for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
        list << that->m_audioOutputInfos[i].index;
    }
    return list;
}

QString Backend::audioOutputName(int audioDevice)
{
    Backend *that = instance();
    that->checkAudioOutputs();
    for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
        if (that->m_audioOutputInfos[i].index == audioDevice) {
            switch (that->m_useOss) {
            case Backend::True: // postfix
                if (that->m_audioOutputInfos[i].driver == "oss") {
                    return i18n("%1 (OSS)", that->m_audioOutputInfos[i].name);
                } else if (that->m_audioOutputInfos[i].driver == "alsa") {
                    return i18n("%1 (ALSA)", that->m_audioOutputInfos[i].name);
                }
                // no postfix: fall through
            case Backend::False: // no postfix
            case Backend::Unknown: // no postfix
                return that->m_audioOutputInfos[i].name;
            }
        }
    }
    return QString();
}

QString Backend::audioOutputDescription(int audioDevice)
{
    Backend *that = instance();
    that->checkAudioOutputs();
    for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
        if (that->m_audioOutputInfos[i].index == audioDevice) {
            return that->m_audioOutputInfos[i].description;
        }
    }
    return QString();
}

QString Backend::audioOutputIcon(int audioDevice)
{
    Backend *that = instance();
    that->checkAudioOutputs();
    for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
        if (that->m_audioOutputInfos[i].index == audioDevice) {
            return that->m_audioOutputInfos[i].icon;
        }
    }
    return QString();
}
bool Backend::audioOutputAvailable(int audioDevice)
{
    Backend *that = instance();
    that->checkAudioOutputs();
    for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
        if (that->m_audioOutputInfos[i].index == audioDevice) {
            return that->m_audioOutputInfos[i].available;
        }
    }
    return false;
}

QVariant Backend::audioOutputMixerDevice(int audioDevice)
{
    Backend *that = instance();
    that->checkAudioOutputs();
    for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
        if (that->m_audioOutputInfos[i].index == audioDevice) {
            if (that->m_audioOutputInfos[i].driver == "alsa") {
                return that->m_audioOutputInfos[i].mixerDevice;
            }
            break;
        }
    }
    return QVariant();
}

int Backend::audioOutputInitialPreference(int audioDevice)
{
    Backend *that = instance();
    that->checkAudioOutputs();

    for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
        if (that->m_audioOutputInfos[i].index == audioDevice) {
            return that->m_audioOutputInfos[i].initialPreference;
        }
    }
    return 0;
}

QByteArray Backend::audioDriverFor(int audioDevice)
{
    Backend *that = instance();
    that->checkAudioOutputs();
    for (int i = 0; i < that->m_audioOutputInfos.size(); ++i) {
        if (that->m_audioOutputInfos[i].index == audioDevice) {
            return that->m_audioOutputInfos[i].driver;
        }
    }
    return QByteArray();
}

QStringList Backend::alsaDevicesFor(int audioDevice)
{
    Backend *that = instance();
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

void Backend::ossSettingChanged(bool useOss)
{
    const Backend::UseOss tmp = useOss ? Backend::True : Backend::False;
    if (tmp == s_instance->m_useOss) {
        return;
    }
    s_instance->m_useOss = tmp;
    if (useOss) {
        // add OSS devices if xine supports OSS output
        const char *const *outputPlugins = xine_list_audio_output_plugins(m_xine);
        for (int i = 0; outputPlugins[i]; ++i) {
            if (0 == strcmp(outputPlugins[i], "oss")) {
                QList<AudioDevice> audioDevices = AudioDeviceEnumerator::availablePlaybackDevices();
                foreach (const AudioDevice &dev, audioDevices) {
                    if (dev.driver() == Solid::AudioInterface::OpenSoundSystem) {
                        s_instance->addAudioOutput(dev, "oss");
                    }
                }
                signalTimer.start();
                return;
            }
        }
    } else {
        // remove all OSS devices
        QList<Backend::AudioOutputInfo>::iterator it = s_instance->m_audioOutputInfos.begin();
        while (it != s_instance->m_audioOutputInfos.end()) {
            if (it->driver == "oss") {
                it = s_instance->m_audioOutputInfos.erase(it);
            } else {
                ++it;
            }
        }
        signalTimer.start();
    }
}

void Backend::addAudioOutput(AudioDevice dev, const QByteArray &driver)
{
    QString mixerDevice;
    int initialPreference = dev.initialPreference();
    if (dev.driver() == Solid::AudioInterface::Alsa) {
        initialPreference += 100;
        foreach (QString id, dev.deviceIds()) {
            const int idx = id.indexOf(QLatin1String("CARD="));
            if (idx > 0) {
                id = id.mid(idx + 5);
                const int commaidx = id.indexOf(QLatin1Char(','));
                if (commaidx > 0) {
                    id = id.left(commaidx);
                }
                mixerDevice = QLatin1String("hw:") + id;
                break;
            }
            mixerDevice = id;
        }
    } else if (!dev.deviceIds().isEmpty()) {
        initialPreference += 50;
        mixerDevice = dev.deviceIds().first();
    }
    const QString description = dev.deviceIds().isEmpty() ?
        i18n("<html>This device is currently not available (either it is unplugged or the "
                "driver is not loaded).</html>") :
        i18n("<html>This will try the following devices and use the first that works: "
                "<ol><li>%1</li></ol></html>", dev.deviceIds().join("</li><li>"));
    AudioOutputInfo info(dev.index(), initialPreference, dev.cardName(),
            description, dev.iconName(), driver, dev.deviceIds(), mixerDevice);
    info.available = dev.isAvailable();
    if (m_audioOutputInfos.contains(info)) {
        m_audioOutputInfos.removeAll(info); // the latest is more up to date wrt availability
    }
    m_audioOutputInfos << info;
}

void Backend::addAudioOutput(int index, int initialPreference, const QString &name, const QString &description,
        const QString &icon, const QByteArray &driver, const QStringList &deviceIds, const QString &mixerDevice)
{
    AudioOutputInfo info(index, initialPreference, name, description, icon, driver, deviceIds, mixerDevice);
    const int listIndex = m_audioOutputInfos.indexOf(info);
    if (listIndex == -1) {
        info.available = true;
        m_audioOutputInfos << info;
//X         KConfigGroup config(m_config, QLatin1String("AudioOutputDevice_") + QString::number(index));
//X         config.writeEntry("name", name);
//X         config.writeEntry("description", description);
//X         config.writeEntry("driver", driver);
//X         config.writeEntry("icon", icon);
//X         config.writeEntry("initialPreference", initialPreference);
    } else {
        AudioOutputInfo &infoInList = m_audioOutputInfos[listIndex];
        if (infoInList.icon != icon || infoInList.initialPreference != initialPreference) {
//X             KConfigGroup config(m_config, QLatin1String("AudioOutputDevice_") + QString::number(infoInList.index));

//X             config.writeEntry("icon", icon);
//X             config.writeEntry("initialPreference", initialPreference);

            infoInList.icon = icon;
            infoInList.initialPreference = initialPreference;
        }
        infoInList.devices = deviceIds;
        infoInList.mixerDevice = mixerDevice;
        infoInList.available = true;
    }
}

void Backend::checkAudioOutputs()
{
    if (m_audioOutputInfos.isEmpty()) {
        kDebug(610) << "isEmpty";
        connect(AudioDeviceEnumerator::self(), SIGNAL(devicePlugged(const AudioDevice &)),
                this, SLOT(devicePlugged(const AudioDevice &)));
        connect(AudioDeviceEnumerator::self(), SIGNAL(deviceUnplugged(const AudioDevice &)),
                this, SLOT(deviceUnplugged(const AudioDevice &)));
        int nextIndex = 10000;
//X         QStringList groups = m_config->groupList();
//X         foreach (QString group, groups) {
//X             if (group.startsWith("AudioOutputDevice_")) {
//X                 const int index = group.right(group.size() - 18/*strlen("AudioOutputDevice_") */).toInt();
//X                 if (index >= nextIndex) {
//X                     nextIndex = index + 1;
//X                 }
//X                 KConfigGroup config(m_config, group);
//X                 m_audioOutputInfos << AudioOutputInfo(index,
//X                         config.readEntry("initialPreference", 0),
//X                         config.readEntry("name", QString()),
//X                         config.readEntry("description", QString()),
//X                         config.readEntry("icon", QString()),
//X                         config.readEntry("driver", QByteArray()),
//X                         QStringList(), QString()); // the device list can change and needs to be queried
//X                                         // from the actual hardware configuration
//X             }
//X         }

        // This will list the audio drivers, not the actual devices.
        const char *const *outputPlugins = xine_list_audio_output_plugins(m_xine);
        for (int i = 0; outputPlugins[i]; ++i) {
            kDebug(610) << "outputPlugin: " << outputPlugins[i];
            if (0 == strcmp(outputPlugins[i], "alsa")) {
                if (m_useOss == Backend::Unknown) {
                    m_useOss = KConfigGroup(m_config, "Settings").readEntry("showOssDevices", false) ? Backend::True : Backend::False;
                    if (m_useOss == Backend::False) {
                        // remove all OSS devices
                        QList<AudioOutputInfo>::iterator it = m_audioOutputInfos.begin();
                        while (it != m_audioOutputInfos.end()) {
                            if (it->driver == "oss") {
                                it = m_audioOutputInfos.erase(it);
                            } else {
                                ++it;
                            }
                        }
                    }
                }

                QList<AudioDevice> alsaDevices = AudioDeviceEnumerator::availablePlaybackDevices();
                foreach (const AudioDevice &dev, alsaDevices) {
                    if (dev.driver() == Solid::AudioInterface::Alsa) {
                        addAudioOutput(dev, "alsa");
                    }
                }
            } else if (0 == strcmp(outputPlugins[i], "none") || 0 == strcmp(outputPlugins[i], "file")) {
                // ignore these devices
            } else if (0 == strcmp(outputPlugins[i], "oss")) {
                if (m_useOss) {
                    QList<AudioDevice> audioDevices = AudioDeviceEnumerator::availablePlaybackDevices();
                    foreach (const AudioDevice &dev, audioDevices) {
                        if (dev.driver() == Solid::AudioInterface::OpenSoundSystem) {
                            addAudioOutput(dev, "oss");
                        }
                    }
                }
            } else if (0 == strcmp(outputPlugins[i], "jack")) {
                addAudioOutput(nextIndex++, -10, i18n("Jack Audio Connection Kit"),
                        i18n("<html><p>JACK is a low-latency audio server. It can connect a number "
                            "of different applications to an audio device, as well as allowing "
                            "them to share audio between themselves.</p>"
                            "<p>JACK was designed from the ground up for professional audio "
                            "work, and its design focuses on two key areas: synchronous "
                            "execution of all clients, and low latency operation.</p></html>"),
                        /*icon name */"audio-backend-jack", outputPlugins[i], QStringList(),
                        QString());
            } else if (0 == strcmp(outputPlugins[i], "arts")) {
                addAudioOutput(nextIndex++, -100, i18n("aRts"),
                        i18n("<html><p>aRts is the old soundserver and media framework that was used "
                            "in KDE2 and KDE3. Its use is discuraged.</p></html>"),
                        /*icon name */"audio-backend-arts", outputPlugins[i], QStringList(), QString());
            } else if (0 == strcmp(outputPlugins[i], "pulseaudio")) {
                addAudioOutput(nextIndex++, 10, i18n("PulseAudio"),
                        xine_get_audio_driver_plugin_description(m_xine, outputPlugins[i]),
                        /*icon name */"audio-backend-pulseaudio", outputPlugins[i], QStringList(), QString());
            } else if (0 == strcmp(outputPlugins[i], "esd")) {
                addAudioOutput(nextIndex++, 10, i18n("Esound (ESD)"),
                        xine_get_audio_driver_plugin_description(m_xine, outputPlugins[i]),
                        /*icon name */"audio-backend-esd", outputPlugins[i], QStringList(), QString());
            } else {
                addAudioOutput(nextIndex++, -20, outputPlugins[i],
                        xine_get_audio_driver_plugin_description(m_xine, outputPlugins[i]),
                        /*icon name */outputPlugins[i], outputPlugins[i], QStringList(),
                        QString());
            }
        }

        qSort(m_audioOutputInfos);

        // now m_audioOutputInfos holds all devices this computer has ever seen
        foreach (const AudioOutputInfo &info, m_audioOutputInfos) {
            kDebug(610) << info.index << info.name << info.driver << info.devices;
        }
    }
}

void Backend::devicePlugged(const AudioDevice &dev)
{
    kDebug(610) << dev.cardName();
    if (!dev.isPlaybackDevice()) {
        return;
    }
    const char *const *outputPlugins = xine_list_audio_output_plugins(m_xine);
    switch (dev.driver()) {
    case Solid::AudioInterface::Alsa:
        for (int i = 0; outputPlugins[i]; ++i) {
            if (0 == strcmp(outputPlugins[i], "alsa")) {
                s_instance->addAudioOutput(dev, "alsa");
                signalTimer.start();
            }
        }
        qSort(s_instance->m_audioOutputInfos);
        break;
    case Solid::AudioInterface::OpenSoundSystem:
        if (s_instance->m_useOss) {
            for (int i = 0; outputPlugins[i]; ++i) {
                if (0 == strcmp(outputPlugins[i], "oss")) {
                    s_instance->addAudioOutput(dev, "oss");
                    signalTimer.start();
                }
            }
        }
        qSort(s_instance->m_audioOutputInfos);
        break;
    case Solid::AudioInterface::UnknownAudioDriver:
        break;
    }
}

void Backend::deviceUnplugged(const AudioDevice &dev)
{
    kDebug(610) << dev.cardName();
    if (!dev.isPlaybackDevice()) {
        return;
    }
    QByteArray driver;
    switch (dev.driver()) {
    case Solid::AudioInterface::Alsa:
        driver = "alsa";
        break;
    case Solid::AudioInterface::OpenSoundSystem:
        driver = "oss";
        break;
    case Solid::AudioInterface::UnknownAudioDriver:
        break;
    }
    Backend::AudioOutputInfo info(dev.index(), 0, dev.cardName(), QString(), dev.iconName(),
            driver, dev.deviceIds(), QString());
    const int indexOfInfo = s_instance->m_audioOutputInfos.indexOf(info);
    if (indexOfInfo < 0) {
        kDebug(610) << "told to remove " << dev.cardName() <<
            " with driver " << driver << " but the device was not present in m_audioOutputInfos";
        return;
    }
    const Backend::AudioOutputInfo oldInfo = s_instance->m_audioOutputInfos.takeAt(indexOfInfo);
    Q_ASSERT(!s_instance->m_audioOutputInfos.contains(info));
    info.initialPreference = oldInfo.initialPreference;
    s_instance->m_audioOutputInfos << info; // now the device is listed as not available
    qSort(s_instance->m_audioOutputInfos);
    signalTimer.start();
}

}}

#include "backend.moc"

// vim: sw=4 ts=4
