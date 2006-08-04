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
#ifndef Phonon_XINE_AUDIOOUTPUT_H
#define Phonon_XINE_AUDIOOUTPUT_H

#include "abstractaudiooutput.h"
#include <QFile>

#include "xine_engine.h"
#include <xine.h>

namespace Phonon
{
namespace Xine
{
	class AudioOutput : public AbstractAudioOutput
	{
		Q_OBJECT
		public:
			AudioOutput( QObject* parent, XineEngine* xe );
			~AudioOutput();

		public slots:
			// Attributes Getters:
			float volume() const;
			int outputDevice() const;

			// Attributes Setters:
			void setVolume( float newVolume );
			void setOutputDevice( int newDevice );

		Q_SIGNALS:
			void volumeChanged( float newVolume );

		private:
			XineEngine* m_xine_engine;
			float m_volume;
			int m_device;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_AUDIOOUTPUT_H
