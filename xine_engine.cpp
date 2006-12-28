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

//#include <kdebug.h>
#include "abstractmediaproducer.h"
#include <QCoreApplication>

namespace Phonon
{
namespace Xine
{

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

	XineEngine* XineEngine::s_instance = 0;

	XineEngine::XineEngine()
		: m_xine( xine_new() )
	{
	}

	XineEngine::~XineEngine()
	{
		xine_exit( m_xine );
		m_xine = 0;
	}

	XineEngine* XineEngine::self()
	{
		if( !s_instance )
			s_instance = new XineEngine();
		return s_instance;
	}

	xine_t* XineEngine::xine()
	{
		return self()->m_xine;
	}

	void XineEngine::xineEventListener( void *p, const xine_event_t* xineEvent )
	{
		if( !p || !xineEvent )
			return;
		//kDebug( 610 ) << "Xine event: " << xineEvent->type << QByteArray( ( char* )xineEvent->data, xineEvent->data_length ) << endl;

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
			case XINE_EVENT_SPU_BUTTON:
				{
					xine_spu_button_t* button = static_cast<xine_spu_button_t*>(xineEvent->data);
					if ( button->direction == 1 ) /* enter a button */
						QCoreApplication::postEvent( mp, new QEvent( static_cast<QEvent::Type>( Xine::NavButtonIn ) ) );
					else
						QCoreApplication::postEvent( mp, new QEvent( static_cast<QEvent::Type>( Xine::NavButtonOut ) ) );
				}
		}
	}
}
}
