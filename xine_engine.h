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

#ifndef xine_engine
#define xine_engine

#include <kdemacros.h>
#include <xine.h>
#include <QEvent>
#include <QString>
#include <QSet>
#include <QStringList>
#include <kconfig.h>

#ifdef Q_OS_WIN
# ifdef MAKE_PHONONXINEENGINE_LIB
#  define PHONON_XINE_ENGINE_EXPORT KDE_EXPORT
# else
#  define PHONON_XINE_ENGINE_EXPORT KDE_IMPORT
# endif
#else
# define PHONON_XINE_ENGINE_EXPORT KDE_EXPORT
#endif

namespace Phonon
{
namespace Xine
{
	enum EventType
	{
		NewMetaDataEvent = 5400,
		MediaFinishedEvent = 5401,
		ProgressEvent = 5402
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

	class PHONON_XINE_ENGINE_EXPORT XineEngine
	{
		public:
			~XineEngine();

			static XineEngine* self();
			static xine_t* xine();
			static void xineEventListener( void*, const xine_event_t* );

            static QSet<int> audioOutputIndexes();
            static QString audioOutputName(int audioDevice);
            static QString audioOutputDescription(int audioDevice);
            static QString audioOutputIcon(int audioDevice);
            static QString audioDriverFor(int audioDevice);
            static QStringList alsaDevicesFor(int audioDevice);

		protected:
			XineEngine();

		private:
            void checkAudioOutputs();
            void addAudioOutput(int idx, const QString &n, const QString &desc, const QString &ic, const QString &dr, const QStringList &dev);
			static XineEngine* s_instance;
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
            KSharedConfig::Ptr m_config;
	};
}
}

#endif //xine_engine
// vim: sw=4 ts=4 tw=80 et
