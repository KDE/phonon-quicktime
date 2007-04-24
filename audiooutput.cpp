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

#include "audiooutput.h"
#include <QVector>
#include <kdebug.h>

#include <sys/ioctl.h>
#include <iostream>
#include <QSet>
#include "audiopath.h"
#include "mediaobject.h"
#include "backend.h"

namespace Phonon
{
namespace Xine
{
AudioOutput::AudioOutput(QObject *parent)
    : AbstractAudioOutput(parent),
    m_device(1)
{
}

AudioOutput::~AudioOutput()
{
    //kDebug(610) << k_funcinfo << endl;
}

qreal AudioOutput::volume() const
{
	return m_volume;
}

int AudioOutput::outputDevice() const
{
	return m_device;
}

void AudioOutput::updateVolume(MediaObject *mp) const
{
    int xinevolume = static_cast<int>(m_volume * 100);
    if (xinevolume > 200) {
        xinevolume = 200;
    } else if (xinevolume < 0) {
        xinevolume = 0;
    }

    mp->stream().setVolume(xinevolume);
}

void AudioOutput::setVolume(qreal newVolume)
{
	m_volume = newVolume;

    int xinevolume = static_cast<int>(m_volume * 100);
    if (xinevolume > 200) {
        xinevolume = 200;
    } else if (xinevolume < 0) {
        xinevolume = 0;
    }

    QSet<XineStream*> streams;
	foreach( AudioPath* ap, m_paths )
	{
        foreach(MediaObject *mp, ap->mediaObjects()) {
            streams << &mp->stream();
		}
	}
    foreach (XineStream *stream, streams) {
        stream->setVolume(xinevolume);
	}

	emit volumeChanged( m_volume );
}

AudioPort AudioOutput::audioPort() const
{
    return m_audioPort;
}

bool AudioOutput::setOutputDevice(int newDevice)
{
    m_device = newDevice;
    m_audioPort = AudioPort(m_device);
    if (!m_audioPort.isValid()) {
        kDebug(610) << "new audio port is invalid" << endl;
        return false;
    }
    emit audioPortChanged(m_audioPort);
    return true;
}

bool AudioOutput::event(QEvent *ev)
{
    switch (ev->type()) {
        case Xine::AudioDeviceFailedEvent:
            ev->accept();
            emit audioDeviceFailed();
            return true;
        default:
            return AbstractAudioOutput::event(ev);
    }
}

}} //namespace Phonon::Xine

#include "audiooutput.moc"
// vim: sw=4 ts=4
