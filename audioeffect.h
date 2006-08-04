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
#ifndef Phonon_XINE_AUDIOEFFECT_H
#define Phonon_XINE_AUDIOEFFECT_H

#include <QObject>

namespace Phonon
{
namespace Xine
{
	class AudioEffect : public QObject
	{
		Q_OBJECT
		public:
			AudioEffect( int effectId, QObject* parent );
			~AudioEffect();

		public slots:
			QVariant value( int parameterId ) const;
			void setValue( int parameterId, QVariant newValue );
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_AUDIOEFFECT_H
