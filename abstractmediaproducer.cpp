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

#include "abstractmediaproducer.h"
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
#include <seekthread.h>
#include <QMetaType>
#include "audioport.h"

namespace Phonon
{
namespace Xine
{
AbstractMediaProducer::AbstractMediaProducer(QObject *parent)
    : QObject( parent ),
    m_state(Phonon::LoadingState),
    m_stream(0),
    m_audioPath(0),
    m_videoPath(0),
    m_seeking(0),
    m_currentTimeOverride(-1)
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
    connect(&m_stream, SIGNAL(seekDone()), SLOT(seekDone()));
    connect(&m_stream, SIGNAL(tick(qint64)), SIGNAL(tick(qint64)));
}

void AbstractMediaProducer::seekDone()
{
    //kDebug(610) << k_funcinfo << endl;
    --m_seeking;
}

AbstractMediaProducer::~AbstractMediaProducer()
{
    if (m_audioPath) {
        m_audioPath->removeMediaProducer(this);
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

bool AbstractMediaProducer::addVideoPath(QObject *videoPath)
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

bool AbstractMediaProducer::addAudioPath(QObject *audioPath)
{
    if (m_audioPath) {
        return false;
    }

    m_audioPath = qobject_cast<AudioPath*>(audioPath);
    Q_ASSERT(m_audioPath);
    m_audioPath->addMediaProducer(this);
    m_stream.setAudioPort(m_audioPath->audioPort(&m_stream));

    return true;
}

void AbstractMediaProducer::removeVideoPath(QObject *videoPath)
{
    Q_ASSERT(videoPath);
    if (m_videoPath == qobject_cast<VideoPath*>(videoPath)) {
        m_stream.setVideoPort(0);
        m_videoPath->unsetMediaProducer(this);
        m_videoPath = 0;
    }
}

void AbstractMediaProducer::removeAudioPath(QObject *audioPath)
{
    Q_ASSERT(audioPath);
    if (m_audioPath == qobject_cast<AudioPath*>(audioPath)) {
        m_stream.setAudioPort(AudioPort());
        m_audioPath->removeMediaProducer(this);
        m_audioPath = 0;
    }
}

State AbstractMediaProducer::state() const
{
    return m_state;
}

bool AbstractMediaProducer::hasVideo() const
{
    return m_stream.hasVideo();
}

bool AbstractMediaProducer::isSeekable() const
{
    return m_stream.isSeekable();
}

qint64 AbstractMediaProducer::currentTime() const
{
    //kDebug(610) << k_funcinfo << kBacktrace() << endl;
    switch(m_stream.state()) {
        case Phonon::PausedState:
        case Phonon::BufferingState:
        case Phonon::PlayingState:
            {
                if (m_seeking) {
                    return m_currentTimeOverride;
                }
                const int current = m_stream.currentTime();
                if (m_currentTimeOverride >= 0) {
                    if (current <= 0) {
                        return m_currentTimeOverride;
                    } else {
                        m_currentTimeOverride = -1;
                    }
                }
                return current;
            }
            break;
        case Phonon::StoppedState:
        case Phonon::LoadingState:
            return 0;
        case Phonon::ErrorState:
            break;
    }
    return -1;
}

qint64 AbstractMediaProducer::totalTime() const
{
    const qint64 ret = stream().totalTime();
    //kDebug(610) << k_funcinfo << "returning " << ret << endl;
    return ret;
}

qint64 AbstractMediaProducer::remainingTime() const
{
    switch(m_stream.state()) {
        case Phonon::PausedState:
        case Phonon::BufferingState:
        case Phonon::PlayingState:
            {
                if (m_seeking) {
                    const qint64 ret = stream().totalTime() - m_currentTimeOverride;
                    //kDebug(610) << k_funcinfo << "returning " << ret << endl;
                    return ret;
                }
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

qint32 AbstractMediaProducer::tickInterval() const
{
    return m_tickInterval;
}

void AbstractMediaProducer::setTickInterval(qint32 newTickInterval)
{
    m_tickInterval = newTickInterval;
    m_stream.setTickInterval(m_tickInterval);
}

QStringList AbstractMediaProducer::availableAudioStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String("en") << QLatin1String("de");
	return ret;
}

QStringList AbstractMediaProducer::availableVideoStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String("en") << QLatin1String("de");
	return ret;
}

QStringList AbstractMediaProducer::availableSubtitleStreams() const
{
	// TODO
	QStringList ret;
	ret << QLatin1String("en") << QLatin1String("de");
	return ret;
}

QString AbstractMediaProducer::selectedAudioStream(const QObject* audioPath) const
{
	// TODO
	return m_selectedAudioStream[ audioPath ];
}

QString AbstractMediaProducer::selectedVideoStream(const QObject* videoPath) const
{
	// TODO
	return m_selectedVideoStream[ videoPath ];
}

QString AbstractMediaProducer::selectedSubtitleStream(const QObject* videoPath) const
{
	// TODO
	return m_selectedSubtitleStream[ videoPath ];
}

void AbstractMediaProducer::selectAudioStream(const QString& streamName, const QObject* audioPath)
{
	// TODO
	if(availableAudioStreams().contains(streamName))
		m_selectedAudioStream[ audioPath ] = streamName;
}

void AbstractMediaProducer::selectVideoStream(const QString& streamName, const QObject* videoPath)
{
	// TODO
	if(availableVideoStreams().contains(streamName))
		m_selectedVideoStream[ videoPath ] = streamName;
}

void AbstractMediaProducer::selectSubtitleStream(const QString& streamName, const QObject* videoPath)
{
	// TODO
	if(availableSubtitleStreams().contains(streamName))
		m_selectedSubtitleStream[ videoPath ] = streamName;
}

void AbstractMediaProducer::play()
{
    if (m_state == Phonon::PlayingState || m_state == Phonon::ErrorState) {
        return;
    }

    m_currentTimeOverride = -1;
    changeState(Phonon::BufferingState);
    m_stream.play();
}

void AbstractMediaProducer::pause()
{
    if (m_state == Phonon::PlayingState || m_state == Phonon::BufferingState) {
        m_stream.pause();
    }
}

void AbstractMediaProducer::stop()
{
    if (m_state == Phonon::PlayingState || m_state == Phonon::PausedState || m_state == Phonon::BufferingState) {
        m_stream.stop();
    }
}

void AbstractMediaProducer::seek(qint64 time)
{
    //kDebug(610) << k_funcinfo << time << endl;
    if (!isSeekable()) {
        return;
    }

    m_stream.seek(time);
    ++m_seeking;
    m_currentTimeOverride = time;
}

void AbstractMediaProducer::changeState(Phonon::State newstate)
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

void AbstractMediaProducer::handleStateChange(Phonon::State newstate, Phonon::State oldstate)
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
#include "abstractmediaproducer.moc"
// vim: sw=4 ts=4
