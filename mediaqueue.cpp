/*  This file is part of the KDE project
    Copyright (C) 2006 Matthias Kretz <kretz@kde.org>

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

#include "mediaqueue.h"
#include <kdebug.h>

namespace Phonon
{
namespace Xine
{

/*
 * ok, here is one thing that has been bugging me for a long time:
 * gapless playback. i guess i have finally thought of a nice way of
 * implementing it with minimum effort from ui.
 *
 * the magic involves two new stream parameters:
 *
 * XINE_PARAM_EARLY_FINISHED_EVENT
 * XINE_PARAM_GAPLESS_SWITCH
 *
 * if UI supports/wants gapless playback it should then set
 * XINE_PARAM_EARLY_FINISHED_EVENT. setting it causes libxine to deliver
 * the XINE_EVENT_UI_PLAYBACK_FINISHED as soon as it can, that is, it
 * will disable internal code that waits for the output audio and video
 * fifos to empty. set it once and forget.
 *
 * when UI receives XINE_EVENT_UI_PLAYBACK_FINISHED, it should set
 * XINE_PARAM_GAPLESS_SWITCH and then perform usual xine_open(),
 * xine_play() sequence. the gapless parameter will cause libxine to
 * disable a couple of code so it won't stop current playback, it won't
 * close audio driver and the new stream should play seamless.
 *
 * but here is the catch: XINE_PARAM_GAPLESS_SWITCH must _only_ be set
 * just before the desired gapless switch. it will be reset automatically
 * as soon as the xine_play() returns. setting it during playback will
 * cause bad seek behaviour.
 *
 * take care to reset the gapless switch if xine_open() fails.
 *
 * btw, frontends should check for version >= 1.1.1 before using this feature.
 */

MediaQueue::MediaQueue( QObject *parent )
	: MediaObject( parent )
	, m_doCrossfade( false )
{
	xine_set_param( stream(), XINE_PARAM_EARLY_FINISHED_EVENT, 1 );
}

MediaQueue::~MediaQueue()
{
}

void MediaQueue::setNextUrl( const KUrl& url )
{
	m_nextUrl = url;
}

void MediaQueue::setDoCrossfade( bool xfade )
{
	if( xfade )
	{
		kWarning( 610 ) << "crossfades with Xine are not implemented yet" << endl;
	}
	//m_doCrossfade = xfade;
}

void MediaQueue::setTimeBetweenMedia( qint32 time )
{
	m_timeBetweenMedia = time;
}

void MediaQueue::recreateStream()
{
	MediaObject::recreateStream();
	xine_set_param( stream(), XINE_PARAM_EARLY_FINISHED_EVENT, 1 );
}

bool MediaQueue::event( QEvent* ev )
{
	switch( ev->type() )
	{
		case Xine::MediaFinishedEvent:
			if( !m_doCrossfade )
			{
				xine_set_param( stream(), XINE_PARAM_GAPLESS_SWITCH, 1 );
				if( !xine_open( stream(), m_nextUrl.url().toUtf8() ) )
					xine_set_param( stream(), XINE_PARAM_GAPLESS_SWITCH, 0 );
				xine_play( stream(), 0, 0 );
				m_url = m_nextUrl;
				m_nextUrl.clear();
				emit needNextUrl();

				m_aboutToFinishNotEmitted = true;
				kDebug( 610 ) << "emit finished()" << endl;
				emit finished();
				ev->accept();
				return true;
			}
		default:
			break;
	}
	return MediaObject::event( ev );
}

} // namespace Xine
} // namespace Phonon

#include "mediaqueue.moc"
// vim: sw=4 ts=4 noet
