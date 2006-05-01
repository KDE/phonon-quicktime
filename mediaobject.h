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
#include <phonon/ifaces/mediaobject.h>
#include <kurl.h>

#include <xine.h>

#include "xine_engine.h"

class KUrl;

namespace Phonon
{
namespace Xine
{
	class MediaObject : public AbstractMediaProducer, virtual public Ifaces::MediaObject
	{
		Q_OBJECT
		public:
			MediaObject( QObject* parent, XineEngine* xe );
			virtual ~MediaObject();
			virtual KUrl url() const;
			virtual long totalTime() const;
			//virtual long remainingTime() const;
			virtual long aboutToFinishTime() const;
			virtual void setUrl( const KUrl& url );
			virtual void setAboutToFinishTime( long newAboutToFinishTime );

			virtual void play();
			virtual void pause();
			virtual void seek( long time );
			//void setXineEngine(xine_t* xe) {m_xine = xe;}

		public Q_SLOTS:
			virtual void stop();

		Q_SIGNALS:
			void finished();
			void aboutToFinish( long msec );
			void length( long length );

		protected:
			virtual void emitTick();

		private:
			//static void xineEventListener( void*, const xine_event_t* );
			XineEngine* m_xine_engine;
			KUrl m_url;
			long m_aboutToFinishTime;
			bool m_aboutToFinishNotEmitted;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_MEDIAOBJECT_H
