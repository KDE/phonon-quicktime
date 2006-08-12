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

#include "xine_engine.h"

#include <kdebug.h>
#include "abstractmediaproducer.h"
#include <QCoreApplication>

namespace Phonon
{
namespace Xine
{
	XineEngine::XineEngine()
	{
		m_xine = 0;
	}

	void XineEngine::xineEventListener( void *p, const xine_event_t* xineEvent )
	{
		if( !p || !xineEvent )
			return;
		kDebug() << "Xine event: " << xineEvent->type << QByteArray( ( char* )xineEvent->data, xineEvent->data_length ) << endl;

		AbstractMediaProducer* mp = static_cast<AbstractMediaProducer*>( p );

		switch( xineEvent->type ) 
		{
			case XINE_EVENT_UI_SET_TITLE:
				QCoreApplication::postEvent( mp, new QEvent( static_cast<QEvent::Type>( Xine::NewMetaDataEvent ) ) );
				break;
			case XINE_EVENT_UI_PLAYBACK_FINISHED:
				QCoreApplication::postEvent( mp, new QEvent( static_cast<QEvent::Type>( Xine::MediaFinishedEvent ) ) );
				break;
			case XINE_EVENT_PROGRESS:
				{
					xine_progress_data_t* progress = static_cast<xine_progress_data_t*>( xineEvent->data );
					QCoreApplication::postEvent( mp, new XineProgressEvent( QString::fromUtf8( progress->description ), progress->percent ) );
				}
				break;
		}
	}
}
}
