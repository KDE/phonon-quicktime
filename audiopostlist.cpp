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

#include "audiopostlist.h"
#include <QList>
#include "audioport.h"
#include "xinestream.h"

#include <xine.h>
#include <QSharedData>
#include "audioeffect.h"

namespace Phonon
{
namespace Xine
{

static inline xine_post_out_t *outputFor(xine_post_t *post)
{
    xine_post_out_t *x = xine_post_output(post, "audio out");
    Q_ASSERT(x);
    return x;
}

static inline xine_post_in_t *inputFor(xine_post_t *post)
{
    xine_post_in_t *x = xine_post_input(post, "audio in");
    Q_ASSERT(x);
    return x;
}

class ListData
{
    public:
        inline ListData(AudioEffect *e, xine_post_t *x)
            : m_effect(e),
            m_post(x),
            m_next(0)
        {
        }

        // compare list items only by looking at m_effect
        bool operator==(const ListData &rhs) { return m_effect == rhs.m_effect; }

        AudioEffect *effect() const { return m_effect; }
        xine_post_t *post() const { return m_post; }

        // called from the xine thread
        void setOutput(xine_post_in_t *audioSink, const AudioPort &audioPort)
        {
            if (m_next == audioSink) {
                return;
            }
            m_next = audioSink;
            if (!m_post) {
                m_post = m_effect->newInstance(audioPort);
            }
            kDebug(610) << "xine_post_wire(" << outputFor(m_post) << ", " << audioSink << ")" << endl;
            xine_post_wire(outputFor(m_post), audioSink);
        }
        // called from the xine thread
        void setOutput(const AudioPort &audioPort)
        {
            // XXX hack
            if (m_next == reinterpret_cast<xine_post_in_t *>(-1)) {
                return;
            }
            m_next = reinterpret_cast<xine_post_in_t *>(-1);
            if (!m_post) {
                m_post = m_effect->newInstance(audioPort);
            } else {
                kDebug(610) << "xine_post_wire_audio_port(" << outputFor(m_post) << ", " << audioPort << ")" << endl;
                xine_post_wire_audio_port(outputFor(m_post), audioPort);
            }
        }

    private:
        AudioEffect *m_effect;
        xine_post_t *m_post;
        xine_post_in_t *m_next;
};

class AudioPostListData : public QSharedData
{
    public:
        QList<ListData> effects;
        AudioPort output;
        AudioPort newOutput;
        QList<XineStream *> streams;

        void needRewire(AudioPostList *o)
        {
            kDebug(610) << k_funcinfo << endl;
            foreach (XineStream *xs, streams) {
                kDebug(610) << xs << "->needRewire" << endl;
                xs->needRewire(o);
            }
        }
};

// called from the xine thread: safe to call xine functions that call back to the ByteStream input
// plugin
void AudioPostList::wireStream(xine_post_out_t *audioSource)
{
    if (d->newOutput.isValid()) {
        if (!d->effects.isEmpty()) {
            xine_post_t *next = 0;

            const QList<ListData>::Iterator begin = d->effects.begin();
            QList<ListData>::Iterator it = d->effects.end();
            --it;
            {
                ListData &effect = *it;
                effect.setOutput(d->newOutput);
                next = effect.post();
            }
            while (it != begin) {
                --it;
                ListData &effect = *it;
                effect.setOutput(inputFor(next), d->newOutput);
                next = effect.post();
            }
            kDebug(610) << "xine_post_wire(" << audioSource << ", " << inputFor(next) << ")" << endl;
            xine_post_wire(audioSource, inputFor(next));
        } else {
            kDebug(610) << "xine_post_wire_audio_port(" << audioSource << ", " << d->newOutput << ")" << endl;
            xine_post_wire_audio_port(audioSource, d->newOutput);
        }
        d->output = d->newOutput;
    } else {
        kDebug(610) << "no valid audio output given, no audio" << endl;
        //xine_set_param(stream, XINE_PARAM_IGNORE_AUDIO, 1);
    }
}

void AudioPostList::setAudioPort(const AudioPort &port)
{
    d->newOutput = port;
    d->needRewire(this);
}

const AudioPort &AudioPostList::audioPort() const
{
    return d->output;
}

bool AudioPostList::contains(AudioEffect *effect) const
{
    return d->effects.contains(ListData(effect, 0));
}

int AudioPostList::indexOf(AudioEffect *effect) const
{
    return d->effects.indexOf(ListData(effect, 0));
}

void AudioPostList::insert(int index, AudioEffect *effect)
{
    d->effects.insert(index, ListData(effect, 0));
    d->needRewire(this);
}

void AudioPostList::append(AudioEffect *effect)
{
    d->effects.append(ListData(effect, 0));
    d->needRewire(this);
}

int AudioPostList::removeAll(AudioEffect *effect)
{
    int removed = d->effects.removeAll(ListData(effect, 0));
    d->needRewire(this);
    return removed;
}

AudioPostList::AudioPostList()
    : d(new AudioPostListData)
{
    d->ref.ref();
}

AudioPostList::AudioPostList(const AudioPostList &rhs)
    :d (rhs.d)
{
    if (d) {
        d->ref.ref();
    }
}

AudioPostList &AudioPostList::operator=(const AudioPostList &rhs)
{
    if (d != rhs.d) {
        AudioPostListData *x = rhs.d;
        if (x) {
            x->ref.ref();
        }
        x = qAtomicSetPtr(&d, x);
        if (x && !x->ref.deref())
            delete x;
    }
    return *this;
}

AudioPostList::~AudioPostList()
{
    if (d && !d->ref.deref()) {
        delete d;
    }
}

void AudioPostList::addXineStream(XineStream *xs)
{
    Q_ASSERT(!d->streams.contains(xs));
    d->streams.append(xs);
}

void AudioPostList::removeXineStream(XineStream *xs)
{
    Q_ASSERT(d->streams.contains(xs));
    const int r = d->streams.removeAll(xs);
    Q_ASSERT(1 == r);
}

} // namespace Xine
} // namespace Phonon

// vim: sw=4 sts=4 et tw=100
