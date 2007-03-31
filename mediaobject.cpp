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

void MediaObject::openMedia(Phonon::MediaObject::Media m)
{
    QByteArray mrl;
    switch (m) {
    case Phonon::MediaObject::None:
        setUrl(KUrl());
        return;
    case Phonon::MediaObject::CD:
        mrl = "cdda:/";
        {
            m_tracks.clear();
            int num = 0;
            char **mrls = xine_get_autoplay_mrls(XineEngine::xine(), "CD", &num);
            for (int i = 0; i < num; ++i) {
                if (mrls[i]) {
                    m_tracks << QByteArray(mrls[i]);
                }
            }
            if (!m_tracks.isEmpty()) {
                mrl = m_tracks.first();
                m_currentTrack = 1;
            }
        }
        break;
    case Phonon::MediaObject::DVD:
        mrl = "dvd:/";
        {
            m_tracks.clear();
            int num = 0;
            char **mrls = xine_get_autoplay_mrls(XineEngine::xine(), "DVD", &num);
            for (int i = 0; i < num; ++i) {
                if (mrls[i]) {
                    m_tracks << QByteArray(mrls[i]);
                }
            }
            if (!m_tracks.isEmpty()) {
                mrl = m_tracks.first();
                m_currentTrack = 1;
            }
        }
        break;
    case Phonon::MediaObject::DVB:
        mrl = "dvb:/";
        break;
    case Phonon::MediaObject::VCD:
        mrl = "vcd:/";
        {
            m_tracks.clear();
            int num = 0;
            char **mrls = xine_get_autoplay_mrls(XineEngine::xine(), "VCD", &num);
            for (int i = 0; i < num; ++i) {
                if (mrls[i]) {
                    m_tracks << QByteArray(mrls[i]);
                }
            }
            if (!m_tracks.isEmpty()) {
                mrl = m_tracks.first();
                m_currentTrack = 1;
            }
        }
        break;
    default:
        kError(610) << "media " << m << " not implemented" << endl;
        return;
    }
    m_url.clear();
    stream().setMrl(mrl);
}

bool MediaObject::hasInterface(Interface interface) const
{
    switch (interface) {
    case AddonInterface::TrackInterface:
        if (m_tracks.size() > 1) {
            return true;
        }
    }
    return false;
}

QVariant MediaObject::interfaceCall(Interface interface, int command, const QList<QVariant> &arguments)
{
    kDebug(610) << k_funcinfo << interface << ", " << command << endl;
    switch (interface) {
    case AddonInterface::TrackInterface:
        switch (command) {
        case AddonInterface::availableTracks:
            kDebug(610) << m_tracks.size() << endl;
            return m_tracks.size();
        case AddonInterface::track:
            kDebug(610) << m_currentTrack << endl;
            return m_currentTrack;
        case AddonInterface::setTrack:
            {
                if (arguments.isEmpty() || !arguments.first().canConvert(QVariant::Int)) {
                    kDebug(610) << "arguments invalid" << endl;
                    return false;
                }
                int t = arguments.first().toInt();
                if (t > m_tracks.size()) {
                    kDebug(610) << "invalid track" << endl;
                    return false;
                }
                if (m_currentTrack == t) {
                    kDebug(610) << "no track change" << endl;
                    return true;
                }
                kDebug(610) << "change track from " << m_currentTrack << " to " << t << endl;
                m_currentTrack = t;
                //State s = state();
                stream().setMrl(m_tracks[t - 1]);
                //if (s == PlayingState || s == BufferingState) {
                    //stream().play();
                //}
                return true;
            }
        }
    }
    return QVariant();
}

void MediaObjectBase::setAboutToFinishTime( qint32 newAboutToFinishTime )
{
    m_aboutToFinishTime = newAboutToFinishTime;
    stream().setAboutToFinishTime(newAboutToFinishTime);
}

}}

#include "mediaobject.moc"
// vim: sw=4 ts=4
