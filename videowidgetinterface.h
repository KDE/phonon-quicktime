/*  This file is part of the KDE project
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

#ifndef VIDEOWIDGETINTERFACE_H
#define VIDEOWIDGETINTERFACE_H

#include <QObject>
#include <xine.h>
#include <QList>
#include "videopath.h"

namespace Phonon
{
namespace Xine
{

class VideoWidgetInterface
{
	public:
		virtual ~VideoWidgetInterface() {}
		virtual xine_video_port_t* videoPort() const = 0;

		virtual void clearWindow() = 0;
		virtual void setPath( VideoPath* vp ) = 0;
		virtual void unsetPath( VideoPath* vp ) = 0;
};

} // namespace Xine
} // namespace Phonon

Q_DECLARE_INTERFACE( Phonon::Xine::VideoWidgetInterface, "org.kde.Phonon.Xine.VideoWidgetInterface/0.1" )

#endif // VIDEOWIDGETINTERFACE_H
