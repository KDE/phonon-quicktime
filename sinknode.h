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

#ifndef SINKNODE_H
#define SINKNODE_H

#include <Phonon/Global>

namespace Phonon
{
namespace Xine
{
class SourceNode;

class SinkNode
{
    public:
        SinkNode() : m_source(0) {}
        virtual ~SinkNode() {}
        virtual MediaStreamTypes inputMediaStreamTypes() const = 0;
        void setSource(SourceNode *s) { Q_ASSERT(m_source == 0); m_source = s; }
        void unsetSource(SourceNode *s) { Q_ASSERT(m_source == s); m_source = 0; }
        SourceNode *source() const { return m_source; }
        virtual void rewireTo(SourceNode *) = 0;
        virtual SourceNode *sourceInterface() { return 0; }
    private:
        SourceNode *m_source;
};

} // namespace Xine
} // namespace Phonon

Q_DECLARE_INTERFACE(Phonon::Xine::SinkNode, "XineSinkNode.phonon.kde.org")

#endif // SINKNODE_H
