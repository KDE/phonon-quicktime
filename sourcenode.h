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

#ifndef SOURCENODE_H
#define SOURCENODE_H

#include <Phonon/Global>
#include <QtCore/QSet>
#include <xine.h>

namespace Phonon
{
namespace Xine
{
class SinkNode;

class SourceNode
{
    public:
        virtual ~SourceNode() {}
        virtual MediaStreamTypes outputMediaStreamTypes() const = 0;
        void addSink(SinkNode *s) { Q_ASSERT(!m_sinks.contains(s)); m_sinks << s; }
        void removeSink(SinkNode *s) { Q_ASSERT(m_sinks.contains(s)); m_sinks.remove(s); }
        QSet<SinkNode *> sinks() const { return m_sinks; }
        virtual SinkNode *sinkInterface() { return 0; }
        virtual xine_post_out_t *audioOutputPort() const { return 0; }
        virtual xine_post_out_t *videoOutputPort() const { return 0; }
    private:
        QSet<SinkNode *> m_sinks;
};
} // namespace Xine
} // namespace Phonon

Q_DECLARE_INTERFACE(Phonon::Xine::SourceNode, "XineSourceNode.phonon.kde.org")

#endif // SOURCENODE_H
