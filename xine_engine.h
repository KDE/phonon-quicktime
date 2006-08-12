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

#include <xine.h>
#include <QEvent>
#include <QString>

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

	class XineProgressEvent : public QEvent
	{
		public:
			XineProgressEvent( const QString& description, int percent )
				: QEvent( static_cast<QEvent::Type>( Xine::ProgressEvent ) )
				, m_description( description )
				, m_percent( percent )
			{
			}

			const QString& description() { return m_description; }
			int percent() { return m_percent; }

		private:
			QString m_description;
			int m_percent;
	};

	class XineEngine
	{
		public:
			XineEngine();

			static void xineEventListener( void*, const xine_event_t* );
			xine_t* m_xine;
	};
}
}

#endif //xine_engine
