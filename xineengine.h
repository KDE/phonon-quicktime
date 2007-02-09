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

#ifndef XINEENGINE_H
#define XINEENGINE_H

#include <xine.h>
#include <QEvent>
#include <QString>
#include <QSet>
#include <QStringList>
#include <kconfig.h>
#include "phononxineexport.h"

namespace Phonon
{
    class AudioDevice;
namespace Xine
{
    class Backend;

	enum EventType
	{
		NewMetaDataEvent = 5400,
		MediaFinishedEvent = 5401,
        ProgressEvent = 5402,
        NavButtonInEvent = 5403,
        NavButtonOutEvent = 5404,
        AudioDeviceFailedEvent = 5405
	};

	class PHONON_XINE_ENGINE_EXPORT XineProgressEvent : public QEvent
	{
		public:
			XineProgressEvent( const QString& description, int percent );
			const QString& description();
			int percent();

		private:
			QString m_description;
			int m_percent;
	};

    class XineEnginePrivate;

	class PHONON_XINE_ENGINE_EXPORT XineEngine
	{
        friend class Phonon::Xine::Backend;
        friend class XineEnginePrivate;
		public:
			static XineEngine* self();
			static xine_t* xine();
			static void xineEventListener( void*, const xine_event_t* );

            static QSet<int> audioOutputIndexes();
            static QString audioOutputName(int audioDevice);
            static QString audioOutputDescription(int audioDevice);
            static QString audioOutputIcon(int audioDevice);
            static bool audioOutputAvailable(int audioDevice);
            static QString audioDriverFor(int audioDevice);
            static QStringList alsaDevicesFor(int audioDevice);

            static QObject *sender();

        protected:
            XineEngine(const KSharedConfigPtr &cfg);
            ~XineEngine();

		private:
            void checkAudioOutputs();
            void addAudioOutput(AudioDevice dev, QString driver);
            void addAudioOutput(int idx, const QString &n, const QString &desc, const QString &ic,
                    const QString &dr, const QStringList &dev);
			xine_t* m_xine;

            struct AudioOutputInfo
            {
                AudioOutputInfo(int idx, const QString &n, const QString &desc, const QString &ic, const QString &dr, const QStringList &dev)
                    : available(false), index(idx), name(n), description(desc), icon(ic), driver(dr), devices(dev)
                {}
                bool available;
                int index;
                QString name;
                QString description;
                QString icon;
                QString driver;
                QStringList devices;
                bool operator==(const AudioOutputInfo& rhs) { return name == rhs.name && driver == rhs.driver; }
            };
            QList<AudioOutputInfo> m_audioOutputInfos;
            KSharedConfigPtr m_config;
            bool m_useOss;
            XineEnginePrivate *d;
	};
}
}

#endif // XINEENGINE_H
// vim: sw=4 ts=4 tw=80 et
