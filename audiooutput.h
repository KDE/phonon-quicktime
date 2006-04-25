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
#include <phonon/ifaces/audiooutput.h>
#include <QFile>

namespace Phonon
{
namespace Xine
{
	class AudioOutput : public AbstractAudioOutput, virtual public Ifaces::AudioOutput
	{
		Q_OBJECT
		public:
			AudioOutput( QObject* parent );
			virtual ~AudioOutput();

			// Attributes Getters:
			virtual QString name() const;
			virtual float volume() const;
			virtual int outputDevice() const;

			// Attributes Setters:
			virtual void setName( const QString& newName );
			virtual void setVolume( float newVolume );
			virtual void setOutputDevice( int newDevice );

			virtual void processBuffer( const QVector<float>& buffer );

			void openDevice();
			void closeDevice();

		Q_SIGNALS:
			void volumeChanged( float newVolume );

		private:
			float m_volume;
			QString m_name;
			int m_device;
			QFile m_dsp;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_AUDIOOUTPUT_H
