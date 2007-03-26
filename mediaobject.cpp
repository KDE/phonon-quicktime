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
#include <kdebug.h>
#include <klocale.h>

#include "xineengine.h"
#include <QEvent>

namespace Phonon
{
namespace Xine
{
MediaObjectBase::MediaObjectBase(QObject *parent)
    : AbstractMediaProducer(parent)
{
    connect(&stream(), SIGNAL(finished()), SLOT(handleFinished()), Qt::QueuedConnection);
    connect(&stream(), SIGNAL(length(qint64)), SIGNAL(length(qint64)), Qt::QueuedConnection);
    connect(&stream(), SIGNAL(aboutToFinish(qint32)), SIGNAL(aboutToFinish(qint32)), Qt::QueuedConnection);
}

MediaObject::MediaObject(QObject *parent)
    : MediaObjectBase(parent)
{
}

void MediaObjectBase::handleFinished()
{
    if (videoPath()) {
        videoPath()->streamFinished();
    }
    kDebug(610) << "emit finished()" << endl;
    emit finished();
}

MediaObjectBase::~MediaObjectBase()
{
	//kDebug( 610 ) << k_funcinfo << endl;
    // if stop() is called for a XineStream with empty m_mrl we provoke an error
    //stop();
}

KUrl MediaObject::url() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_url;
}

qint32 MediaObjectBase::aboutToFinishTime() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_aboutToFinishTime;
}


void MediaObject::setUrl( const KUrl& url )
{
	//kDebug( 610 ) << k_funcinfo << endl;
    if (state() != Phonon::LoadingState) {
        stop();
    }
    if (url.scheme() == QLatin1String("kbytestream")) {
        kError(610) << "do not ever use kbytestream:/ URLs with MediaObject!" << endl;
        stream().setMrl(QByteArray());
        stream().setError(Phonon::NormalError, i18n("Cannot open media data at '<i>%1</i>'", url.prettyUrl()));
        return;
    }
    stream().setUrl(url);
    m_url = url;
}

void MediaObjectBase::setAboutToFinishTime( qint32 newAboutToFinishTime )
{
    m_aboutToFinishTime = newAboutToFinishTime;
    stream().setAboutToFinishTime(newAboutToFinishTime);
}

}}

#include "mediaobject.moc"
// vim: sw=4 ts=4
