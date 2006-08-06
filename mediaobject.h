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

namespace Phonon
{
namespace Xine
{
	class MediaObject : public AbstractMediaProducer, public MediaObjectInterface
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::MediaObjectInterface )
		public:
			MediaObject( QObject* parent, XineEngine* xe );
			~MediaObject();

		public slots:
			KUrl url() const;
			qint64 totalTime() const;
			//qint64 remainingTime() const;
			qint32 aboutToFinishTime() const;
			void setUrl( const KUrl& url );
			void setAboutToFinishTime( qint32 newAboutToFinishTime );

			void play();
			void pause();
			void seek( qint64 time );
			void stop();

		Q_SIGNALS:
			void finished();
			void aboutToFinish( qint32 msec );
			void length( qint64 length );

		protected:
			virtual void emitTick();
			virtual bool event( QEvent* ev );

		private slots:
			void emitAboutToFinish();

		private:
			XineEngine* m_xine_engine;
			KUrl m_url;
			qint32 m_aboutToFinishTime;
			bool m_aboutToFinishNotEmitted;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_MEDIAOBJECT_H
