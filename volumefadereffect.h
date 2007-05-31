/*  This file is part of the KDE project
    Copyright (C) 2006 Tim Beaulen <tbscope@gmail.com>
    Copyright (C) 2006-2007 Matthias Kretz <kretz@kde.org>

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
#ifndef Phonon_XINE_VOLUMEFADEREFFECT_H
#define Phonon_XINE_VOLUMEFADEREFFECT_H

#include <QTime>
#include "audioeffect.h"
#include <phonon/volumefadereffect.h>
#include <QList>

#include <xine.h>

namespace Phonon
{
namespace Xine
{
	class VolumeFaderEffect : public AudioEffect
	{
		Q_OBJECT
		public:
			VolumeFaderEffect( QObject* parent );
			~VolumeFaderEffect();

            xine_post_t *newInstance(xine_audio_port_t *);

		public slots:
			float volume() const;
			void setVolume( float volume );
			Phonon::VolumeFaderEffect::FadeCurve fadeCurve() const;
			void setFadeCurve( Phonon::VolumeFaderEffect::FadeCurve curve );
			void fadeTo( float volume, int fadeTime );

            QVariant parameterValue(int parameterId) const;
            void setParameterValue(int parameterId, const QVariant &newValue);

		private:
            void getParameters() const;

            struct PluginParameters
            {
                Phonon::VolumeFaderEffect::FadeCurve fadeCurve;
                float currentVolume;
                float fadeTo;
                int fadeTime;
                PluginParameters(Phonon::VolumeFaderEffect::FadeCurve a, float b, float c, int d)
                    : fadeCurve(a), currentVolume(b), fadeTo(c), fadeTime(d) {}
            };

            mutable PluginParameters m_parameters;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_VOLUMEFADEREFFECT_H
