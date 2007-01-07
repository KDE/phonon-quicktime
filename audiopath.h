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
#ifndef Phonon_XINE_AUDIOPATH_H
#define Phonon_XINE_AUDIOPATH_H

#include <QObject>
#include <QList>
#include <xine.h>
#include "audioport.h"

namespace Phonon
{
namespace Xine
{
	class AudioEffect;
	class AbstractAudioOutput;
	class AbstractMediaProducer;
	class AudioOutput;
class XineStream;

	class AudioPath : public QObject
	{
		Q_OBJECT
		public:
			AudioPath( QObject* parent );
			~AudioPath();

			void addMediaProducer( AbstractMediaProducer* mp );
			void removeMediaProducer( AbstractMediaProducer* mp );
			QList<AbstractMediaProducer*> producers() { return m_producers; }

			bool hasOutput() const;
            AudioPort audioPort(XineStream *forStream) const;
			void updateVolume( AbstractMediaProducer* mp ) const;

		public slots:
			bool addOutput( QObject* audioOutput );
			bool removeOutput( QObject* audioOutput );
			bool insertEffect( QObject* newEffect, QObject* insertBefore = 0 );
			bool removeEffect( QObject* effect );

		private:
			AudioOutput *m_output;
			QList<AudioEffect*> m_effects;
			QList<AbstractAudioOutput*> m_outputs;
			QList<AbstractMediaProducer*> m_producers;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_AUDIOPATH_H
