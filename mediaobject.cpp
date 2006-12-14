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

#include "mediaobject.h"
#include <QTimer>
#include <kdebug.h>

#include "xine_engine.h"
#include <QEvent>

namespace Phonon
{
namespace Xine
{
MediaObject::MediaObject(QObject *parent)
    : AbstractMediaProducer(parent),
    m_aboutToFinishNotEmitted(true),
    m_aboutToFinishTimer(0)
{
    connect(&stream(), SIGNAL(finished()), SLOT(handleFinished()));
    connect(&stream(), SIGNAL(length(qint64)), SIGNAL(length(qint64)));
}

void MediaObject::handleFinished()
{
    m_aboutToFinishNotEmitted = true;
    if (videoPath()) {
        videoPath()->streamFinished();
    }
    kDebug(610) << "emit finished()" << endl;
    emit finished();
}

MediaObject::~MediaObject()
{
	//kDebug( 610 ) << k_funcinfo << endl;
	stop();
}

KUrl MediaObject::url() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_url;
}

qint64 MediaObject::totalTime() const
{
    return stream().totalTime();
}

qint64 MediaObject::remainingTime() const
{
    return stream().remainingTime();
}

qint32 MediaObject::aboutToFinishTime() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_aboutToFinishTime;
}

//#define DISABLE_MEDIAOBJECT

void MediaObject::setUrl( const KUrl& url )
{
	//kDebug( 610 ) << k_funcinfo << endl;
#ifdef DISABLE_MEDIAOBJECT
	Q_UNUSED( url );
	setState( Phonon::ErrorState );
#else
    if (state() != Phonon::LoadingState) {
        stop();
    }
    stream().setUrl(url);
    m_url = url;
#endif
}

void MediaObject::setAboutToFinishTime( qint32 newAboutToFinishTime )
{
    m_aboutToFinishTime = newAboutToFinishTime;
    if (m_aboutToFinishTime > 0) {
        const qint64 time = currentTime();
        const qint64 total = totalTime();
        if (time < total - m_aboutToFinishTime) { // not about to finish
            m_aboutToFinishNotEmitted = true;
            if (state() == Phonon::PlayingState)
                emitAboutToFinishIn( total - m_aboutToFinishTime - time );
        }
    }
}

void MediaObject::seek(qint64 time)
{
    if (!isSeekable()) {
        return;
    }

    AbstractMediaProducer::seek(time);

    const int total = stream().totalTime();
    if (m_aboutToFinishTime > 0 && time < total - m_aboutToFinishTime) { // not about to finish
        m_aboutToFinishNotEmitted = true;
        emitAboutToFinishIn(total - m_aboutToFinishTime - time);
    }
}

void MediaObject::emitTick()
{
    AbstractMediaProducer::emitTick();
    if (m_aboutToFinishNotEmitted && m_aboutToFinishTime > 0) {
        const int remainingTime = stream().remainingTime();
        const int timeToAboutToFinishSignal = remainingTime - m_aboutToFinishTime;
        if (timeToAboutToFinishSignal <= tickInterval()) { // about to finish
            if (timeToAboutToFinishSignal > 100) {
                emitAboutToFinishIn(timeToAboutToFinishSignal);
            } else {
                m_aboutToFinishNotEmitted = false;
                kDebug(610) << "emitting aboutToFinish( " << remainingTime << " )" << endl;
                emit aboutToFinish(remainingTime);
            }
        }
    }
}

void MediaObject::reachedPlayingState()
{
    if (m_aboutToFinishTime > 0) {
        emitAboutToFinishIn(stream().remainingTime() - m_aboutToFinishTime);
    }
    AbstractMediaProducer::reachedPlayingState();
}

void MediaObject::leftPlayingState()
{
    m_aboutToFinishNotEmitted = true;
    if (m_aboutToFinishTimer) {
        m_aboutToFinishTimer->stop();
    }
    AbstractMediaProducer::leftPlayingState();
}

void MediaObject::emitAboutToFinishIn(int timeToAboutToFinishSignal)
{
    Q_ASSERT(m_aboutToFinishTime > 0);
    if (!m_aboutToFinishTimer) {
        m_aboutToFinishTimer = new QTimer(this);
        m_aboutToFinishTimer->setSingleShot(true);
        connect(m_aboutToFinishTimer, SIGNAL(timeout()), SLOT(emitAboutToFinish()));
    }
    m_aboutToFinishTimer->start(timeToAboutToFinishSignal);
}

void MediaObject::emitAboutToFinish()
{
    if (m_aboutToFinishNotEmitted && m_aboutToFinishTime > 0) {
        const int remainingTime = stream().remainingTime();

        if (remainingTime <= m_aboutToFinishTime + 150) {
            m_aboutToFinishNotEmitted = false;
            emit aboutToFinish(remainingTime);
        } else {
            emitAboutToFinishIn(remainingTime - m_aboutToFinishTime);
        }
    }
}

}}

#include "mediaobject.moc"
// vim: sw=4 ts=4
