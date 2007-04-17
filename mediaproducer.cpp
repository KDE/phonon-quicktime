/*  This file is part of the KDE project
    Copyright (C) 2006 Tim Beaulen <tbscope@gmail.com>
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

#include "mediaproducer.h"
#include "videopath.h"
#include "audiopath.h"
#include <kdebug.h>
#include <QVector>
#include <cmath>
#include <QFile>
#include <QByteArray>
#include <QStringList>
#include <QMultiMap>
#include <QEvent>
#include <QtDebug>
#include <QMetaType>
#include "audioport.h"

namespace Phonon
{
namespace Xine
{
MediaProducer::MediaProducer(QObject *parent)
    : QObject( parent ),
    m_state(Phonon::LoadingState),
    m_stream(),
    m_videoPath(0),
    m_seeking(0)
{
    m_stream.moveToThread(&m_stream);
    m_stream.start();
    qRegisterMetaType<QMultiMap<QString,QString> >("QMultiMap<QString,QString>");
    qRegisterMetaType<Phonon::State>("Phonon::State");
    qRegisterMetaType<qint64>("qint64");
    connect(&m_stream, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
            SLOT(handleStateChange(Phonon::State, Phonon::State)));
    connect(&m_stream, SIGNAL(metaDataChanged(const QMultiMap<QString, QString>&)),
            SIGNAL(metaDataChanged(const QMultiMap<QString, QString>&)));
    connect(&m_stream, SIGNAL(seekableChanged(bool)), SIGNAL(seekableChanged(bool)));
    connect(&m_stream, SIGNAL(hasVideoChanged(bool)), SIGNAL(hasVideoChanged(bool)));
    connect(&m_stream, SIGNAL(bufferStatus(int)), SIGNAL(bufferStatus(int)));
    connect(&m_stream, SIGNAL(seekDone()), SLOT(seekDone()));
    connect(&m_stream, SIGNAL(tick(qint64)), SIGNAL(tick(qint64)));
    connect(&m_stream, SIGNAL(availableChaptersChanged(int)), SIGNAL(availableChaptersChanged(int)));
    connect(&m_stream, SIGNAL(chapterChanged(int)), SIGNAL(chapterChanged(int)));
}

void MediaProducer::seekDone()
{
    //kDebug(610) << k_funcinfo << endl;
    --m_seeking;
    if (0 == m_seeking) {
        emit tick(currentTime());
    }
}

MediaProducer::~MediaProducer()
{
    foreach (AudioPath *p, m_audioPaths) {
        m_stream.removeAudioPostList(p->audioPostList());
        p->removeMediaProducer(this);
    }
    if (m_videoPath) {
        m_videoPath->unsetMediaProducer(this);
    }

    // we have to be sure that the event loop of m_stream is already started at this point, else the
    // quit function will be ignored
    m_stream.waitForEventLoop();
    m_stream.quit();
    if (!m_stream.wait( 2000 )) {
        kWarning(610) << "XineStream hangs and is terminated." << endl;
        m_stream.wait();
        //m_stream.terminate();
    }
}

bool MediaProducer::addVideoPath(QObject *videoPath)
{
    if (m_videoPath) {
        return false;
    }

    m_videoPath = qobject_cast<VideoPath*>(videoPath);
    Q_ASSERT(m_videoPath);
    m_videoPath->setMediaProducer(this);
    m_stream.setVideoPort(m_videoPath->videoPort());

    return true;
}

bool MediaProducer::addAudioPath(QObject *audioPath)
{
    AudioPath *ap = qobject_cast<AudioPath*>(audioPath);
    Q_ASSERT(ap);
    Q_ASSERT(!m_audioPaths.contains(ap));
    m_audioPaths << ap;
    ap->addMediaProducer(this);
    m_stream.addAudioPostList(ap->audioPostList());
    //m_stream.setAudioPort(m_audioPath->audioPort(&m_stream));

    return true;
}

void MediaProducer::removeVideoPath(QObject *videoPath)
{
    Q_ASSERT(videoPath);
    if (m_videoPath == qobject_cast<VideoPath*>(videoPath)) {
        m_stream.setVideoPort(0);
        m_videoPath->unsetMediaProducer(this);
        m_videoPath = 0;
    }
}

void MediaProducer::removeAudioPath(QObject *audioPath)
{
    AudioPath *ap = qobject_cast<AudioPath*>(audioPath);
    Q_ASSERT(ap);
    const int count = m_audioPaths.removeAll(ap);
    Q_ASSERT(1 == count);
    m_stream.removeAudioPostList(ap->audioPostList());
    ap->removeMediaProducer(this);
}

State MediaProducer::state() const
{
    return m_state;
}

bool MediaProducer::hasVideo() const
{
    return m_stream.hasVideo();
}

bool MediaProducer::isSeekable() const
{
    return m_stream.isSeekable();
}

qint64 MediaProducer::currentTime() const
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

qint64 MediaProducer::totalTime() const
{
    const qint64 ret = stream().totalTime();
    //kDebug(610) << k_funcinfo << "returning " << ret << endl;
    return ret;
}

qint64 MediaProducer::remainingTime() const
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

qint32 MediaProducer::tickInterval() const
{
    return m_tickInterval;
}

void MediaProducer::setTickInterval(qint32 newTickInterval)
{
    m_tickInterval = newTickInterval;
    m_stream.setTickInterval(m_tickInterval);
}

QStringList MediaProducer::availableAudioStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String("en") << QLatin1String("de");
	return ret;
}

QStringList MediaProducer::availableVideoStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String("en") << QLatin1String("de");
	return ret;
}

QStringList MediaProducer::availableSubtitleStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String("en") << QLatin1String("de");
	return ret;
}

QString MediaProducer::currentAudioStream(const QObject* audioPath) const
{
	// TODO
	return m_currentAudioStream[ audioPath ];
}

QString MediaProducer::currentVideoStream(const QObject* videoPath) const
{
	// TODO
	return m_currentVideoStream[ videoPath ];
}

QString MediaProducer::currentSubtitleStream(const QObject* videoPath) const
{
	// TODO
	return m_currentSubtitleStream[ videoPath ];
}

void MediaProducer::setCurrentAudioStream(const QString& streamName, const QObject* audioPath)
{
	// TODO
	if(availableAudioStreams().contains(streamName))
        m_currentAudioStream[audioPath] = streamName;
}

void MediaProducer::setCurrentVideoStream(const QString& streamName, const QObject* videoPath)
{
	// TODO
	if(availableVideoStreams().contains(streamName))
        m_currentVideoStream[videoPath] = streamName;
}

void MediaProducer::setCurrentSubtitleStream(const QString& streamName, const QObject* videoPath)
{
	// TODO
	if(availableSubtitleStreams().contains(streamName))
        m_currentSubtitleStream[videoPath] = streamName;
}

void MediaProducer::play()
{
    if (m_state == Phonon::StoppedState || m_state == Phonon::LoadingState || m_state == Phonon::PausedState) {
        changeState(Phonon::BufferingState);
    }
    m_stream.play();
}

void MediaProducer::pause()
{
    m_stream.pause();
}

void MediaProducer::stop()
{
    //if (m_state == Phonon::PlayingState || m_state == Phonon::PausedState || m_state == Phonon::BufferingState) {
        m_stream.stop();
    //}
}

void MediaProducer::seek(qint64 time)
{
    //kDebug(610) << k_funcinfo << time << endl;
    if (!isSeekable()) {
        return;
    }

    m_stream.seek(time);
    ++m_seeking;
}

QString MediaProducer::errorString() const
{
    return m_stream.errorString();
}

Phonon::ErrorType MediaProducer::errorType() const
{
    return m_stream.errorType();
}

void MediaProducer::changeState(Phonon::State newstate)
{
    // this method is for "fake" state changes the following state changes are not "fakable":
    Q_ASSERT(newstate != Phonon::PlayingState);
    Q_ASSERT(m_state != Phonon::PlayingState);

    if (m_state == newstate) {
        return;
    }

    Phonon::State oldstate = m_state;
    m_state = newstate;

    /*
    if (newstate == Phonon::PlayingState) {
        reachedPlayingState();
    } else if (oldstate == Phonon::PlayingState) {
        leftPlayingState();
    }
    */

    kDebug(610) << "fake state change: reached " << newstate << " after " << oldstate << endl;
    emit stateChanged(newstate, oldstate);
}

void MediaProducer::handleStateChange(Phonon::State newstate, Phonon::State oldstate)
{
    if (m_state == newstate) {
        return;
    } else if (m_state != oldstate) {
        oldstate = m_state;
    }
    m_state = newstate;

    kDebug(610) << "reached " << newstate << " after " << oldstate << endl;
    if (newstate == Phonon::PlayingState) {
        reachedPlayingState();
    } else if (oldstate == Phonon::PlayingState) {
        leftPlayingState();
    }
    emit stateChanged(newstate, oldstate);
}

}}
#include "mediaproducer.moc"
// vim: sw=4 ts=4
