/*  This file is part of the KDE project
    Copyright (C) 2006 Tim Beaulen <tbscope@gmail.com>

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
#ifndef Phonon_XINE_VIDEOPATH_H
#define Phonon_XINE_VIDEOPATH_H

#include <QObject>
#include <phonon/videoframe.h>
#include <QList>

namespace Phonon
{
namespace Xine
{
	class VideoEffect;
	class AbstractVideoOutput;

	class VideoPath : public QObject
	{
		Q_OBJECT
		public:
			VideoPath( QObject* parent );
			~VideoPath();

		public slots:
			bool addOutput( QObject* videoOutput );
			bool removeOutput( QObject* videoOutput );
			bool insertEffect( QObject* newEffect, QObject* insertBefore = 0 );
			bool removeEffect( QObject* effect );

		private:
			QList<VideoEffect*> m_effects;
			QList<QObject*> m_outputs;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_VIDEOPATH_H
