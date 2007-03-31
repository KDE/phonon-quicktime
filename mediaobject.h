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
#ifndef Phonon_XINE_MEDIAOBJECT_H
#define Phonon_XINE_MEDIAOBJECT_H

#include "abstractmediaproducer.h"
#include <kurl.h>

#include <xine.h>

#include "xineengine.h"
#include <phonon/mediaobjectinterface.h>
#include <QByteArray>
#include <QList>

class KUrl;

namespace Phonon
{
namespace Xine
{
    class MediaObjectBase : public AbstractMediaProducer
    {
        Q_OBJECT
        public:
            MediaObjectBase(QObject* parent);
            ~MediaObjectBase();

        public slots:
            qint32 aboutToFinishTime() const;
            void setAboutToFinishTime(qint32 newAboutToFinishTime);

        signals:
            void finished();
            void aboutToFinish(qint32 msec);
            void length(qint64 length);

        private slots:
            void handleFinished();

        private:
            qint32 m_aboutToFinishTime;
    };

    class MediaObject : public MediaObjectBase, public MediaObjectInterface
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::MediaObjectInterface )
		public:
			MediaObject( QObject* parent );

		public slots:
			KUrl url() const;
            qint64 totalTime() const { return AbstractMediaProducer::totalTime(); }
            qint64 remainingTime() const { return AbstractMediaProducer::remainingTime(); }
            void setAboutToFinishTime(qint32 newAboutToFinishTime) { MediaObjectBase::setAboutToFinishTime(newAboutToFinishTime); }
            qint32 aboutToFinishTime() const { return MediaObjectBase::aboutToFinishTime(); }
			void setUrl( const KUrl& url );
            void openMedia(Phonon::MediaObject::Media m);

            bool hasInterface(AddonInterface::Interface i) const;
            QVariant interfaceCall(AddonInterface::Interface, int, const QList<QVariant> &);

		protected:
			KUrl m_url;
            QList<QByteArray> m_tracks;
            int m_currentTrack;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_MEDIAOBJECT_H
