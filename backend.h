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

#ifndef Phonon_XINE_BACKEND_H
#define Phonon_XINE_BACKEND_H

#include <QtCore/QList>
#include <QtCore/QList>
#include <QtCore/QTimer>
#include <QtCore/QPair>
#include <QtCore/QPointer>
#include <QtCore/QStringList>

#include <xine.h>
#include <xine/xineutils.h>

#include "xineengine.h"
#include <QObject>
#include <phonon/objectdescription.h>
#ifndef Q_MOC_RUN
#include <phonon/backendinterface.h>
#endif
#include <KSharedConfig>

namespace Phonon
{
namespace Xine
{
    enum MediaStreamType {
        Audio = 1,
        Video = 2,
        StillImage = 4,
        Subtitle = 8,
        AllMedia = 0xFFFFFFFF
    };
    Q_DECLARE_FLAGS(MediaStreamTypes, MediaStreamType)
} // namespace Xine
} // namespace Phonon
Q_DECLARE_OPERATORS_FOR_FLAGS(Phonon::Xine::MediaStreamTypes)

namespace Phonon
{
#ifdef Q_MOC_RUN
class BackendInterface { enum Class; };
}
Q_DECLARE_INTERFACE(Phonon::BackendInterface, "BackendInterface3.phonon.kde.org")
namespace Phonon
{
#endif
class AudioDevice;
namespace Xine
{

class WireCall;
class XineThread;
class Backend : public QObject, public Phonon::BackendInterface
{
    Q_OBJECT
    Q_INTERFACES(Phonon::BackendInterface)
    Q_CLASSINFO("D-Bus Interface", "org.kde.phonon.XineBackendInternal")
    public:
        static Backend *instance();
        Backend(QObject *parent, const QVariantList &args);
        ~Backend();

        QObject *createObject(BackendInterface::Class, QObject *parent, const QList<QVariant> &args);

        Q_INVOKABLE bool supportsVideo() const;
        Q_INVOKABLE bool supportsOSD() const;
        Q_INVOKABLE bool supportsFourcc(quint32 fourcc) const;
        Q_INVOKABLE bool supportsSubtitles() const;

        Q_INVOKABLE void freeSoundcardDevices();

        QSet<int> objectDescriptionIndexes(ObjectDescriptionType) const;
        QHash<QByteArray, QVariant> objectDescriptionProperties(ObjectDescriptionType, int) const;

        bool startConnectionChange(QSet<QObject *>);
        bool connectNodes(QObject *, QObject *);
        bool disconnectNodes(QObject *, QObject *);
        bool endConnectionChange(QSet<QObject *>);

        QStringList availableMimeTypes() const;

    // phonon-xine internal:
        static void addCleanupObject(QObject *o) { instance()->m_cleanupObjects << o; }
        static void removeCleanupObject(QObject *o) { instance()->m_cleanupObjects.removeAll(o); }

        static bool deinterlaceDVD();
        static bool deinterlaceVCD();
        static bool deinterlaceFile();
        static int deinterlaceMethod();

        static QSet<int> audioOutputIndexes();
        static QString audioOutputName(int audioDevice);
        static QString audioOutputDescription(int audioDevice);
        static QString audioOutputIcon(int audioDevice);
        static bool audioOutputAvailable(int audioDevice);
        static QVariant audioOutputMixerDevice(int audioDevice);
        static int audioOutputInitialPreference(int audioDevice);
        static QByteArray audioDriverFor(int audioDevice);
        static QStringList alsaDevicesFor(int audioDevice);

        static XineEngine xine() { return instance()->m_xine; }
        static void returnXineEngine(const XineEngine &);
        static XineEngine xineEngineForStream();

    public slots:
        Q_SCRIPTABLE void ossSettingChanged(bool);

    signals:
        void objectDescriptionChanged(ObjectDescriptionType);

    private slots:
        void devicePlugged(const AudioDevice &);
        void deviceUnplugged(const AudioDevice &);
        void emitAudioDeviceChange();

    private:
        void checkAudioOutputs();
        void addAudioOutput(AudioDevice dev, const QByteArray &driver);
        void addAudioOutput(int idx, int initialPreference, const QString &n,
                const QString &desc, const QString &ic, const QByteArray &dr,
                const QStringList &dev, const QString &mixerDevice);

        mutable QStringList m_supportedMimeTypes;
        struct AudioOutputInfo
        {
            AudioOutputInfo(int idx, int ip, const QString &n, const QString &desc, const QString &ic,
                    const QByteArray &dr, const QStringList &dev, const QString &mdev)
                : available(false), index(idx), initialPreference(ip), name(n),
                description(desc), icon(ic), driver(dr), devices(dev), mixerDevice(mdev) {}

            bool available;
            int index;
            int initialPreference;
            QString name;
            QString description;
            QString icon;
            QByteArray driver;
            QStringList devices;
            QString mixerDevice;
            bool operator==(const AudioOutputInfo &rhs) { return name == rhs.name && driver == rhs.driver; }
        };
        QList<AudioOutputInfo> m_audioOutputInfos;
        QList<QObject *> m_cleanupObjects;
        KSharedConfigPtr m_config;
        int m_deinterlaceMethod : 8;
        enum UseOss {
            False = 0,
            True = 1,
            Unknown = 2
        };
        UseOss m_useOss : 2;
        bool m_deinterlaceDVD : 1;
        bool m_deinterlaceVCD : 1;
        bool m_deinterlaceFile : 1;
        XineThread *m_thread;
        XineEngine m_xine;
        QTimer signalTimer;
        QList<WireCall> m_disconnections;

        QList<XineEngine> m_usedEngines;
        QList<XineEngine> m_freeEngines;

        friend class XineThread;
};
}} // namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_BACKEND_H
