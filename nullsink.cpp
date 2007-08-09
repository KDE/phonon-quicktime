/*  This file is part of the KDE project
    Copyright (C) 2007 Matthias Kretz <kretz@kde.org>

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

#include "nullsink.h"
#include "xineengine.h"
#include "audioport.h"
#include "sourcenode.h"

namespace Phonon
{
namespace Xine
{

NullSinkXT::NullSinkXT()
    : m_videoPort(0)
{
}

void NullSinkXT::rewireTo(SourceNodeXT *source)
{
    xine_post_out_t *audioSource = source->audioOutputPort();
    xine_post_out_t *videoSource = source->videoOutputPort();
    if (audioSource) {
        if (!m_audioPort.isValid()) {
            m_audioPort.d->port = XineEngine::nullPort();
        }
        xine_post_wire_audio_port(audioSource, m_audioPort);
    }
    if (videoSource) {
        if (!m_videoPort) {
            m_videoPort = XineEngine::nullVideoPort();
        }
        xine_post_wire_video_port(videoSource, m_videoPort);
    }
}

class NullSinkPrivate
{
    public:
        NullSink instance;
};
K_GLOBAL_STATIC(NullSinkPrivate, s_nullSinkPrivate)

NullSink *NullSink::instance()
{
    return &s_nullSinkPrivate->instance;
}

} // namespace Xine
} // namespace Phonon

#include "moc_nullsink.cpp"
#include "xineengine.h"
