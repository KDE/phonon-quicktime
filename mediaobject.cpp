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
#include <QFile>

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
    : MediaObjectBase(parent),
    m_currentTrack(1),
    m_autoplayTracks(true)
{
    connect(&stream(), SIGNAL(availableTitlesChanged(int)), SLOT(availableTitlesChanged(int)));
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
    m_tracks.clear();
    m_media = Phonon::MediaObject::None;

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

QByteArray MediaObject::autoplayMrlsToTracks(const char *plugin, const char *defaultMrl)
{
    const int lastSize = m_tracks.size();
    m_tracks.clear();
    int num = 0;
    char **mrls = xine_get_autoplay_mrls(XineEngine::xine(), plugin, &num);
    for (int i = 0; i < num; ++i) {
        if (mrls[i]) {
            kDebug(610) << k_funcinfo << mrls[i] << endl;
            m_tracks << QByteArray(mrls[i]);
        }
    }
    if (lastSize != m_tracks.size()) {
        emit availableTracksChanged(m_tracks.size());
    }
    if (m_tracks.isEmpty()) {
        return defaultMrl;
    }
    m_currentTrack = 1;
    if (m_autoplayTracks) {
        stream().useGaplessPlayback(true);
        connect(&stream(), SIGNAL(needNextUrl()), this, SLOT(nextTrack()));
    } else {
        stream().useGaplessPlayback(false);
        disconnect(&stream(), SIGNAL(needNextUrl()), this, SLOT(nextTrack()));
    }
    return m_tracks.first();
}

void MediaObject::openMedia(Phonon::MediaObject::Media m, const QString &mediaDevice)
{
    m_tracks.clear();

    m_mediaDevice = QFile::encodeName(mediaDevice);
    if (!m_mediaDevice.isEmpty() && !m_mediaDevice.startsWith('/')) {
        kError(610) << "mediaDevice '" << mediaDevice << "' has to be an absolute path - starts with a /" << endl;
        m_mediaDevice.clear();
    }
    m_mediaDevice += '/';

    m_media = m;
    QByteArray mrl;
    switch (m) {
    case Phonon::MediaObject::None:
        setUrl(KUrl());
        return;
    case Phonon::MediaObject::CD:
        mrl = autoplayMrlsToTracks("CD", "cdda:/");
        break;
    case Phonon::MediaObject::DVD:
        mrl = "dvd:" + m_mediaDevice;
        break;
    case Phonon::MediaObject::VCD:
        mrl = autoplayMrlsToTracks("VCD", "vcd:/");
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
    case AddonInterface::ChapterInterface:
        if (stream().availableChapters() > 1) {
            return true;
        }
    }
    return false;
}

void MediaObject::availableTitlesChanged(int t)
{
    kDebug(610) << k_funcinfo << t << endl;
    if (m_media == Phonon::MediaObject::DVD) {
        QByteArray mrl = "dvd:" + m_mediaDevice;
        const int lastSize = m_tracks.size();
        m_tracks.clear();
        for (int i = 1; i <= t; ++i) {
            m_tracks << mrl + QByteArray::number(i);
        }
        if (m_tracks.size() != lastSize) {
            emit availableTracksChanged(m_tracks.size());
        }
    }
}

QVariant MediaObject::interfaceCall(Interface interface, int command, const QList<QVariant> &arguments)
{
    kDebug(610) << k_funcinfo << interface << ", " << command << endl;
    switch (interface) {
    case AddonInterface::ChapterInterface:
        switch (static_cast<AddonInterface::ChapterCommand>(command)) {
        case AddonInterface::availableChapters:
            return stream().availableChapters();
        case AddonInterface::chapter:
            return stream().currentChapter();
        case AddonInterface::setChapter:
            {
                if (arguments.isEmpty() || !arguments.first().canConvert(QVariant::Int)) {
                    kDebug(610) << "arguments invalid" << endl;
                    return false;
                }
                int c = arguments.first().toInt();
                int t = m_currentTrack - 1;
                if (t < 0) {
                    t = 0;
                }
                if (m_tracks.size() > t) {
                    QByteArray mrl = m_tracks[t] + '.' + QByteArray::number(c);
                    stream().setMrl(mrl, XineStream::KeepState);
                }
                return true;
            }
        }
        break;
    case AddonInterface::TrackInterface:
        switch (static_cast<AddonInterface::TrackCommand>(command)) {
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
                stream().setMrl(m_tracks[t - 1], m_autoplayTracks ? XineStream::KeepState : XineStream::StoppedState);
                if (m_media == Phonon::MediaObject::CD) {
                    emit trackChanged(m_currentTrack);
                }
                return true;
            }
        case AddonInterface::autoplayTracks:
            return m_autoplayTracks;
        case AddonInterface::setAutoplayTracks:
            {
                if (arguments.isEmpty() || !arguments.first().canConvert(QVariant::Bool)) {
                    kDebug(610) << "arguments invalid" << endl;
                    return false;
                }
                bool b = arguments.first().toBool();
                if (b == m_autoplayTracks) {
                    kDebug(610) << "setAutoplayTracks: no change" << endl;
                    return false;
                }
                m_autoplayTracks = b;
                if (b) {
                    kDebug(610) << "setAutoplayTracks: enable autoplay" << endl;
                    stream().useGaplessPlayback(true);
                    connect(&stream(), SIGNAL(needNextUrl()), this, SLOT(nextTrack()));
                } else {
                    kDebug(610) << "setAutoplayTracks: disable autoplay" << endl;
                    stream().useGaplessPlayback(false);
                    disconnect(&stream(), SIGNAL(needNextUrl()), this, SLOT(nextTrack()));
                }
                return true;
            }
        }
    }
    return QVariant();
}

void MediaObject::nextTrack()
{
    if (m_tracks.size() > m_currentTrack) {
        stream().gaplessSwitchTo(m_tracks[m_currentTrack]);
        ++m_currentTrack;
        emit trackChanged(m_currentTrack);
    } else {
        stream().gaplessSwitchTo(QByteArray());
    }
}

void MediaObjectBase::setAboutToFinishTime( qint32 newAboutToFinishTime )
{
    m_aboutToFinishTime = newAboutToFinishTime;
    stream().setAboutToFinishTime(newAboutToFinishTime);
}

}}

#include "mediaobject.moc"
// vim: sw=4 ts=4
