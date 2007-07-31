/*  This file is part of the KDE project
    Copyright (C) 2006 Tim Beaulen <tbscope@gmail.com>
    Copyright (C) 2006-2007 Matthias Kretz <kretz@kde.org>

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

#include "audioport.h"
#include "bytestream.h"

#include <QEvent>
#include <QFile>
#include <QVector>
#include <QFile>
#include <QByteArray>
#include <QStringList>
#include <QMultiMap>
#include <QEvent>
#include <QtDebug>
#include <QMetaType>

#include <kdebug.h>
#include <klocale.h>

#include <cmath>

static const char *const green  = "\033[1;40;32m";
static const char *const blue   = "\033[1;40;34m";
static const char *const normal = "\033[0m";

namespace Phonon
{
namespace Xine
{
MediaObject::MediaObject(QObject *parent)
    : QObject(parent),
    m_state(Phonon::LoadingState),
    m_stream(),
    m_currentTitle(1),
    m_transitionTime(0),
    m_autoplayTitles(true),
    m_fakingBuffering(false),
    m_shouldFakeBufferingOnPlay(true)
{
    m_stream.moveToThread(&m_stream);
    m_stream.start();
    m_stream.useGaplessPlayback(true);

    qRegisterMetaType<QMultiMap<QString,QString> >("QMultiMap<QString,QString>");
    connect(&m_stream, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
            SLOT(handleStateChange(Phonon::State, Phonon::State)));
    connect(&m_stream, SIGNAL(metaDataChanged(const QMultiMap<QString, QString> &)),
            SIGNAL(metaDataChanged(const QMultiMap<QString, QString> &)));
    connect(&m_stream, SIGNAL(seekableChanged(bool)), SIGNAL(seekableChanged(bool)));
    connect(&m_stream, SIGNAL(hasVideoChanged(bool)), SIGNAL(hasVideoChanged(bool)));
    connect(&m_stream, SIGNAL(bufferStatus(int)), SIGNAL(bufferStatus(int)));
    connect(&m_stream, SIGNAL(tick(qint64)), SIGNAL(tick(qint64)));
    connect(&m_stream, SIGNAL(availableChaptersChanged(int)), SIGNAL(availableChaptersChanged(int)));
    connect(&m_stream, SIGNAL(chapterChanged(int)), SIGNAL(chapterChanged(int)));
    connect(&m_stream, SIGNAL(availableAnglesChanged(int)), SIGNAL(availableAnglesChanged(int)));
    connect(&m_stream, SIGNAL(angleChanged(int)), SIGNAL(angleChanged(int)));
    connect(&m_stream, SIGNAL(finished()), SLOT(handleFinished()), Qt::QueuedConnection);
    connect(&m_stream, SIGNAL(length(qint64)), SIGNAL(totalTimeChanged(qint64)), Qt::QueuedConnection);
    connect(&m_stream, SIGNAL(prefinishMarkReached(qint32)), SIGNAL(prefinishMarkReached(qint32)), Qt::QueuedConnection);
    connect(&m_stream, SIGNAL(availableTitlesChanged(int)), SLOT(handleAvailableTitlesChanged(int)));
    connect(&m_stream, SIGNAL(needNextUrl()), SLOT(needNextUrl()));
}

MediaObject::~MediaObject()
{
//X     FIXME
//X     foreach (AudioPath *p, m_audioPaths) {
//X         m_stream.removeAudioPostList(p->audioPostList());
//X         p->removeMediaObject(this);
//X     }
//X     if (m_videoPath) {
//X         m_videoPath->unsetMediaObject(this);
//X     }

    // we have to be sure that the event loop of m_stream is already started at this point, else the
    // quit function will be ignored
    m_stream.waitForEventLoop();
    if (m_bytestream) {
        m_bytestream->stop();
    }
    m_stream.closeBlocking();
    m_stream.quit();
    if (!m_stream.wait(2000)) {
        kWarning(610) << "XineStream hangs and is terminated." << endl;
        m_stream.wait();
        //m_stream.terminate();
    }

    // if stop() is called for a XineStream with empty m_mrl we provoke an error
    //stop();
}

State MediaObject::state() const
{
    return m_state;
}

bool MediaObject::hasVideo() const
{
    return m_stream.hasVideo();
}

MediaStreamTypes MediaObject::outputMediaStreamTypes() const
{
    if (hasVideo()) {
        return Phonon::Audio | Phonon::Video;
    }
    return Phonon::Audio;
}

bool MediaObject::isSeekable() const
{
    return m_stream.isSeekable();
}

qint64 MediaObject::currentTime() const
{
    //kDebug(610) << k_funcinfo << kBacktrace() << endl;
    switch(m_stream.state()) {
    case Phonon::PausedState:
    case Phonon::BufferingState:
    case Phonon::PlayingState:
        return m_stream.currentTime();
    case Phonon::StoppedState:
    case Phonon::LoadingState:
        return 0;
    case Phonon::ErrorState:
        break;
    }
    return -1;
}

qint64 MediaObject::totalTime() const
{
    const qint64 ret = stream().totalTime();
    //kDebug(610) << k_funcinfo << "returning " << ret << endl;
    return ret;
}

qint64 MediaObject::remainingTime() const
{
    switch(m_stream.state()) {
    case Phonon::PausedState:
    case Phonon::BufferingState:
    case Phonon::PlayingState:
        {
            const qint64 ret = stream().remainingTime();
            //kDebug(610) << k_funcinfo << "returning " << ret << endl;
            return ret;
        }
        break;
    case Phonon::StoppedState:
    case Phonon::LoadingState:
        //kDebug(610) << k_funcinfo << "returning 0" << endl;
        return 0;
    case Phonon::ErrorState:
        break;
    }
    //kDebug(610) << k_funcinfo << "returning -1" << endl;
    return -1;
}

qint32 MediaObject::tickInterval() const
{
    return m_tickInterval;
}

void MediaObject::setTickInterval(qint32 newTickInterval)
{
    m_tickInterval = newTickInterval;
    m_stream.setTickInterval(m_tickInterval);
}

/*
QList<AudioStreamDescription> MediaObject::availableAudioStreams() const
{
    // TODO
    QStringList ret;
    ret << QLatin1String("en") << QLatin1String("de");
    return ret;
}

QList<VideoStreamDescription> MediaObject::availableVideoStreams() const
{
    // TODO
    QStringList ret;
    ret << QLatin1String("en") << QLatin1String("de");
    return ret;
}

QList<SubtitleStreamDescription> MediaObject::availableSubtitleStreams() const
{
    // TODO
    QStringList ret;
    ret << QLatin1String("en") << QLatin1String("de");
    return ret;
}

AudioStreamDescription MediaObject::currentAudioStream(const QObject *audioPath) const
{
    // TODO
    return m_currentAudioStream[audioPath];
}

VideoStreamDescription MediaObject::currentVideoStream(const QObject *videoPath) const
{
    // TODO
    return m_currentVideoStream[videoPath];
}

SubtitleStreamDescription MediaObject::currentSubtitleStream(const QObject *videoPath) const
{
    // TODO
    return m_currentSubtitleStream[videoPath];
}

void MediaObject::setCurrentAudioStream(const QString &streamName, const QObject *audioPath)
{
    // TODO
    if(availableAudioStreams().contains(streamName))
        m_currentAudioStream[audioPath] = streamName;
}

void MediaObject::setCurrentVideoStream(const QString &streamName, const QObject *videoPath)
{
    // TODO
    if(availableVideoStreams().contains(streamName))
        m_currentVideoStream[videoPath] = streamName;
}

void MediaObject::setCurrentSubtitleStream(const QString &streamName, const QObject *videoPath)
{
    // TODO
    if(availableSubtitleStreams().contains(streamName))
        m_currentSubtitleStream[videoPath] = streamName;
}
*/

void MediaObject::play()
{
    kDebug(610) << green << "PLAY" << normal << endl;
    m_stream.play();
    if (m_shouldFakeBufferingOnPlay || m_state == Phonon::StoppedState || m_state == Phonon::LoadingState || m_state == Phonon::PausedState) {
        m_shouldFakeBufferingOnPlay = false;
        startToFakeBuffering();
    }
}

void MediaObject::pause()
{
    m_stream.pause();
}

void MediaObject::stop()
{
    //if (m_state == Phonon::PlayingState || m_state == Phonon::PausedState || m_state == Phonon::BufferingState) {
        m_stream.stop();
    //}
}

void MediaObject::seek(qint64 time)
{
    //kDebug(610) << k_funcinfo << time << endl;
    m_stream.seek(time);
}

QString MediaObject::errorString() const
{
    return m_stream.errorString();
}

Phonon::ErrorType MediaObject::errorType() const
{
    return m_stream.errorType();
}

void MediaObject::startToFakeBuffering()
{
    kDebug(610) << blue << "start faking" << normal << endl;
    m_fakingBuffering = true;
    if (m_state == Phonon::BufferingState) {
        return;
    } else if (m_state == Phonon::PlayingState) {
        // next time we reach StoppedState from LoadingState go right into BufferingState
        return;
    }

    kDebug(610) << "fake state change: reached BufferingState after " << m_state << endl;

    Phonon::State oldstate = m_state;
    m_state = Phonon::BufferingState;

    emit stateChanged(Phonon::BufferingState, oldstate);
}

void MediaObject::handleStateChange(Phonon::State newstate, Phonon::State oldstate)
{
    if (m_state == newstate) {
        if (m_fakingBuffering) {
            Q_ASSERT(m_state == BufferingState);
            m_fakingBuffering = false;
            kDebug(610) << blue << "end faking" << normal << endl;
        }
        // BufferingState -> BufferingState, nothing to do
        return;
    } else if (m_state != oldstate) {
        Q_ASSERT(m_fakingBuffering);
        Q_ASSERT(m_state == BufferingState);
        if (newstate == PlayingState || newstate == ErrorState) {
            m_fakingBuffering = false;
            kDebug(610) << blue << "end faking" << normal << endl;
            oldstate = m_state;
        } else {
            // we're faking BufferingState and stay there until we either reach BufferingState,
            // PlayingState or ErrorState
            return;
        }
    } else if (oldstate == LoadingState && newstate == StoppedState && m_fakingBuffering) {
        newstate = BufferingState;
    }
    m_state = newstate;

    kDebug(610) << "reached " << newstate << " after " << oldstate << endl;
    emit stateChanged(newstate, oldstate);
}
void MediaObject::handleFinished()
{
//X     FIXME
//X     if (videoPath()) {
//X         videoPath()->streamFinished();
//X     }
    kDebug(610) << "emit finished()" << endl;
    emit finished();
}

MediaSource MediaObject::source() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_mediaSource;
}

qint32 MediaObject::prefinishMark() const
{
	//kDebug( 610 ) << k_funcinfo << endl;
	return m_prefinishMark;
}

qint32 MediaObject::transitionTime() const
{
    return m_transitionTime;
}

void MediaObject::setTransitionTime(qint32 newTransitionTime)
{
    if (m_transitionTime != newTransitionTime) {
        m_transitionTime = newTransitionTime;
        if (m_transitionTime == 0) {
            stream().useGaplessPlayback(true);
        } else if (m_transitionTime > 0) {
            // TODO: a gap of m_transitionTime milliseconds
            stream().useGaplessPlayback(true);
        } else {
            // TODO: a crossfade of milliseconds milliseconds
            stream().useGaplessPlayback(true);
        }
    }
}

void MediaObject::setNextSource(const MediaSource &source)
{
    if (source.type() == MediaSource::Invalid) {
        // the frontend is telling us that the play-queue is empty, so stop waiting for a new MRL
        // for gapless playback
        m_stream.gaplessSwitchTo(QByteArray());
        setSourceInternal(m_mediaSource, HardSwitch);
        return;
    }
    if (m_transitionTime < 0) {
        kError(610) << "crossfades are not supported with the xine backend" << endl;
    } else if (m_transitionTime > 0) {
        kError(610) << "defined gaps are not supported with the xine backend" << endl;
    }
    setSourceInternal(source, GaplessSwitch);
}

void MediaObject::setSource(const MediaSource &source)
{
    setSourceInternal(source, HardSwitch);
}

void MediaObject::setSourceInternal(const MediaSource &source, HowToSetTheUrl how)
{
	//kDebug( 610 ) << k_funcinfo << endl;
    m_titles.clear();
    m_mediaSource = source;

    switch (source.type()) {
    case MediaSource::Invalid:
        stop();
        break;
    case MediaSource::LocalFile:
    case MediaSource::Url:
        if (source.url().scheme() == QLatin1String("kbytestream")) {
            m_mediaSource = MediaSource();
            kError(610) << "do not ever use kbytestream:/ URLs with MediaObject!" << endl;
            m_shouldFakeBufferingOnPlay = false;
            stream().setMrl(QByteArray());
            stream().setError(Phonon::NormalError, i18n("Cannot open media data at '<i>%1</i>'", source.url().toString(QUrl::RemovePassword)));
            return;
        }
        switch (how) {
        case GaplessSwitch:
            m_stream.gaplessSwitchTo(source.url());
            break;
        case HardSwitch:
            m_shouldFakeBufferingOnPlay = true;
            m_stream.setUrl(source.url());
            break;
        }
        break;
    case MediaSource::Disc:
        {
            m_mediaDevice = QFile::encodeName(source.deviceName());
            if (!m_mediaDevice.isEmpty() && !m_mediaDevice.startsWith('/')) {
                kError(610) << "mediaDevice '" << m_mediaDevice << "' has to be an absolute path - starts with a /" << endl;
                m_mediaDevice.clear();
            }
            m_mediaDevice += '/';

            QByteArray mrl;
            switch (source.discType()) {
                case Phonon::NoDisc:
                    kFatal(610) << "I should never get to see a MediaSource that is a disc but doesn't specify which one" << endl;
                    return;
                case Phonon::Cd:
                    mrl = autoplayMrlsToTitles("CD", "cdda:/");
                    break;
                case Phonon::Dvd:
                    mrl = "dvd:" + m_mediaDevice;
                    break;
                case Phonon::Vcd:
                    mrl = autoplayMrlsToTitles("VCD", "vcd:/");
                    break;
                default:
                    kError(610) << "media " << source.discType() << " not implemented" << endl;
                    return;
            }
            switch (how) {
            case GaplessSwitch:
                m_stream.gaplessSwitchTo(mrl);
                break;
            case HardSwitch:
                m_shouldFakeBufferingOnPlay = true;
                m_stream.setMrl(mrl);
                break;
            }
        }
        break;
    case MediaSource::Stream:
        {
            // m_bytestream may not be deleted, the xine input plugin takes ownership and will
            // delete it when xine frees the input plugin
            m_bytestream = new ByteStream(source, this);
            switch (how) {
            case GaplessSwitch:
                m_stream.gaplessSwitchTo(m_bytestream->mrl());
                break;
            case HardSwitch:
                m_shouldFakeBufferingOnPlay = true;
                m_stream.setMrl(m_bytestream->mrl());
                break;
            }
        }
        break;
    }
//X     if (state() != Phonon::LoadingState) {
//X         stop();
//X     }
}

//X void MediaObject::openMedia(Phonon::MediaObject::Media m, const QString &mediaDevice)
//X {
//X     m_titles.clear();
//X 
//X }

QByteArray MediaObject::autoplayMrlsToTitles(const char *plugin, const char *defaultMrl)
{
    const int lastSize = m_titles.size();
    m_titles.clear();
    int num = 0;
    char **mrls = xine_get_autoplay_mrls(XineEngine::xine(), plugin, &num);
    for (int i = 0; i < num; ++i) {
        if (mrls[i]) {
            kDebug(610) << k_funcinfo << mrls[i] << endl;
            m_titles << QByteArray(mrls[i]);
        }
    }
    if (lastSize != m_titles.size()) {
        emit availableTitlesChanged(m_titles.size());
    }
    if (m_titles.isEmpty()) {
        return defaultMrl;
    }
    m_currentTitle = 1;
    if (m_autoplayTitles) {
        stream().useGaplessPlayback(true);
    } else {
        stream().useGaplessPlayback(false);
    }
    return m_titles.first();
}

bool MediaObject::hasInterface(Interface interface) const
{
    switch (interface) {
    case AddonInterface::TitleInterface:
        if (m_titles.size() > 1) {
            return true;
        }
    case AddonInterface::ChapterInterface:
        if (stream().availableChapters() > 1) {
            return true;
        }
    }
    return false;
}

void MediaObject::handleAvailableTitlesChanged(int t)
{
    kDebug(610) << k_funcinfo << t << endl;
    if (m_mediaSource.discType() == Phonon::Dvd) {
        QByteArray mrl = "dvd:" + m_mediaDevice;
        const int lastSize = m_titles.size();
        m_titles.clear();
        for (int i = 1; i <= t; ++i) {
            m_titles << mrl + QByteArray::number(i);
        }
        if (m_titles.size() != lastSize) {
            emit availableTitlesChanged(m_titles.size());
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
                int t = m_currentTitle - 1;
                if (t < 0) {
                    t = 0;
                }
                if (m_titles.size() > t) {
                    QByteArray mrl = m_titles[t] + '.' + QByteArray::number(c);
                    stream().setMrl(mrl, XineStream::KeepState);
                }
                return true;
            }
        }
        break;
    case AddonInterface::TitleInterface:
        switch (static_cast<AddonInterface::TitleCommand>(command)) {
        case AddonInterface::availableTitles:
            kDebug(610) << m_titles.size() << endl;
            return m_titles.size();
        case AddonInterface::title:
            kDebug(610) << m_currentTitle << endl;
            return m_currentTitle;
        case AddonInterface::setTitle:
            {
                if (arguments.isEmpty() || !arguments.first().canConvert(QVariant::Int)) {
                    kDebug(610) << "arguments invalid" << endl;
                    return false;
                }
                int t = arguments.first().toInt();
                if (t > m_titles.size()) {
                    kDebug(610) << "invalid title" << endl;
                    return false;
                }
                if (m_currentTitle == t) {
                    kDebug(610) << "no title change" << endl;
                    return true;
                }
                kDebug(610) << "change title from " << m_currentTitle << " to " << t << endl;
                m_currentTitle = t;
                stream().setMrl(m_titles[t - 1], m_autoplayTitles ? XineStream::KeepState : XineStream::StoppedState);
                if (m_mediaSource.discType() == Phonon::Cd) {
                    emit titleChanged(m_currentTitle);
                }
                return true;
            }
        case AddonInterface::autoplayTitles:
            return m_autoplayTitles;
        case AddonInterface::setAutoplayTitles:
            {
                if (arguments.isEmpty() || !arguments.first().canConvert(QVariant::Bool)) {
                    kDebug(610) << "arguments invalid" << endl;
                    return false;
                }
                bool b = arguments.first().toBool();
                if (b == m_autoplayTitles) {
                    kDebug(610) << "setAutoplayTitles: no change" << endl;
                    return false;
                }
                m_autoplayTitles = b;
                if (b) {
                    kDebug(610) << "setAutoplayTitles: enable autoplay" << endl;
                    stream().useGaplessPlayback(true);
                } else {
                    kDebug(610) << "setAutoplayTitles: disable autoplay" << endl;
                    stream().useGaplessPlayback(false);
                }
                return true;
            }
        }
    }
    return QVariant();
}

void MediaObject::needNextUrl()
{
    if (m_mediaSource.type() == MediaSource::Disc && m_titles.size() > m_currentTitle) {
        stream().gaplessSwitchTo(m_titles[m_currentTitle]);
        ++m_currentTitle;
        emit titleChanged(m_currentTitle);
        return;
    }
    emit aboutToFinish();
}

void MediaObject::setPrefinishMark( qint32 newPrefinishMark )
{
    m_prefinishMark = newPrefinishMark;
    stream().setPrefinishMark(newPrefinishMark);
}

}}

#include "mediaobject.moc"
// vim: sw=4 ts=4
