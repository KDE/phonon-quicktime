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

#include "xine_engine.h"
#include <phonon/mediaobjectinterface.h>

class KUrl;
class QTimer;

namespace Phonon
{
namespace Xine
{
	class MediaObject : public AbstractMediaProducer, public MediaObjectInterface
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::MediaObjectInterface )
		public:
			MediaObject( QObject* parent );
			~MediaObject();

		public slots:
			KUrl url() const;
			qint64 totalTime() const;
            qint64 remainingTime() const;
			qint32 aboutToFinishTime() const;
			void setUrl( const KUrl& url );
			void setAboutToFinishTime( qint32 newAboutToFinishTime );

			void seek( qint64 time );

		Q_SIGNALS:
			void finished();
			void aboutToFinish( qint32 msec );
            void length(qint64 length);

		protected:
			virtual void emitTick();
			virtual void reachedPlayingState();
			virtual void leftPlayingState();

			KUrl m_url;
			bool m_aboutToFinishNotEmitted;

		private slots:
            void handleFinished();
			void emitAboutToFinish();

		private:
			void emitAboutToFinishIn( int timeToAboutToFinishSignal );

			qint32 m_aboutToFinishTime;
			QTimer *m_aboutToFinishTimer;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_MEDIAOBJECT_H
