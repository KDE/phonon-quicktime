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

#ifndef PHONON_AUDIOPOSTLIST_H
#define PHONON_AUDIOPOSTLIST_H

#include "phononxineexport.h"
typedef struct xine_post_out_s xine_post_out_t;

namespace Phonon
{
namespace Xine
{

class XineStream;
class AudioPostListData;
class AudioEffect;
class AudioPort;

class PHONON_XINE_ENGINE_EXPORT AudioPostList
{
    public:
        AudioPostList();
        AudioPostList(const AudioPostList &);
        AudioPostList &operator=(const AudioPostList &);
        ~AudioPostList();
        bool operator==(const AudioPostList &rhs) const { return d == rhs.d; }

        void addXineStream(XineStream *);
        void removeXineStream(XineStream *);

        // QList interface
        bool contains(AudioEffect *) const;
        int indexOf(AudioEffect *) const;
        void insert(int index, AudioEffect *);
        void append(AudioEffect *);
        int removeAll(AudioEffect *);

        void setAudioPort(const AudioPort &);
        const AudioPort &audioPort() const;

        // called from the xine thread
        void wireStream(xine_post_out_t *audioSource);

    private:
        AudioPostListData *d;
};

} // namespace Xine
} // namespace Phonon

#endif // PHONON_AUDIOPOSTLIST_H
