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

#include "volumefadereffect.h"
#include "xineengine.h"
#include <klocale.h>

namespace Phonon
{
namespace Xine
{

enum ParameterIds {
    VolumeParameter = 0,
    FadeCurveParameter = 1,
    FadeToParameter = 2,
    FadeTimeParameter = 3,
    StartFadeParameter = 4
};

VolumeFaderEffect::VolumeFaderEffect( QObject* parent )
    : Effect("KVolumeFader", parent),
    m_parameters(Phonon::VolumeFaderEffect::Fade3Decibel, 1.0f, 1.0f, 0)
{
    const QVariant one = 1.0;
    const QVariant dZero = 0.0;
    const QVariant iZero = 0;
    addParameter(EffectParameter(VolumeParameter, i18n("Volume"), 0, one, dZero, one));
    addParameter(EffectParameter(FadeCurveParameter, i18n("Fade Curve"),
                EffectParameter::IntegerHint, iZero, iZero, 3));
    addParameter(EffectParameter(FadeToParameter, i18n("Fade To Volume"), 0, one, dZero, one));
    addParameter(EffectParameter(FadeTimeParameter, i18n("Fade Time"),
                EffectParameter::IntegerHint, iZero, iZero, 10000));
    addParameter(EffectParameter(StartFadeParameter, i18n("Start Fade"),
                EffectParameter::ToggledHint, iZero, iZero, 1));
}

VolumeFaderEffect::~VolumeFaderEffect()
{
}

QVariant VolumeFaderEffect::parameterValue(int parameterId) const
{
    kDebug(610) << k_funcinfo << parameterId << endl;
    switch (static_cast<ParameterIds>(parameterId)) {
        case VolumeParameter:
            return static_cast<double>(volume());
        case FadeCurveParameter:
            return static_cast<int>(fadeCurve());
        case FadeToParameter:
            return static_cast<double>(m_parameters.fadeTo);
        case FadeTimeParameter:
            return m_parameters.fadeTime;
        case StartFadeParameter:
            return 0;
    }
    kError(610) << k_funcinfo << "request for unknown parameter " << parameterId << endl;
    return QVariant();
}

void VolumeFaderEffect::setParameterValue(int parameterId, const QVariant &newValue)
{
    kDebug(610) << k_funcinfo << parameterId << newValue << endl;
    switch (static_cast<ParameterIds>(parameterId)) {
        case VolumeParameter:
            setVolume(newValue.toDouble());
            break;
        case FadeCurveParameter:
            setFadeCurve(static_cast<Phonon::VolumeFaderEffect::FadeCurve>(newValue.toInt()));
            break;
        case FadeToParameter:
            m_parameters.fadeTo = newValue.toDouble();
            break;
        case FadeTimeParameter:
            m_parameters.fadeTime = newValue.toInt();
            break;
        case StartFadeParameter:
            if (newValue.toBool()) {
                fadeTo(m_parameters.fadeTo, m_parameters.fadeTime);
            }
            break;
        default:
            kError(610) << k_funcinfo << "request for unknown parameter " << parameterId << endl;
            break;
    }
}

xine_post_t *VolumeFaderEffect::newInstance(xine_audio_port_t *audioPort)
{
    Q_ASSERT(0 == m_plugin);
    kDebug(610) << k_funcinfo << audioPort << " fadeTime = " << m_parameters.fadeTime << endl;
    m_plugin = xine_post_init(XineEngine::xine(), "KVolumeFader", 1, &audioPort, 0);
    xine_post_in_t *paraInput = xine_post_input(m_plugin, "parameters");
    Q_ASSERT(paraInput);
    Q_ASSERT(paraInput->type == XINE_POST_DATA_PARAMETERS);
    Q_ASSERT(paraInput->data);
    m_pluginApi = reinterpret_cast<xine_post_api_t *>(paraInput->data);
    m_pluginApi->set_parameters(m_plugin, &m_parameters);
    return m_plugin;
}

void VolumeFaderEffect::getParameters() const
{
    if (m_pluginApi) {
        m_pluginApi->get_parameters(m_plugin, &m_parameters);
    }
}

float VolumeFaderEffect::volume() const
{
    //kDebug(610) << k_funcinfo << endl;
    getParameters();
    return m_parameters.currentVolume;
}

void VolumeFaderEffect::setVolume( float volume )
{
    //kDebug(610) << k_funcinfo << volume << endl;
    m_parameters.currentVolume = volume;
}

Phonon::VolumeFaderEffect::FadeCurve VolumeFaderEffect::fadeCurve() const
{
    //kDebug(610) << k_funcinfo << endl;
    getParameters();
    return m_parameters.fadeCurve;
}

void VolumeFaderEffect::setFadeCurve( Phonon::VolumeFaderEffect::FadeCurve curve )
{
    //kDebug(610) << k_funcinfo << curve << endl;
    m_parameters.fadeCurve = curve;
}

void VolumeFaderEffect::fadeTo( float volume, int fadeTime )
{
    //kDebug(610) << k_funcinfo << volume << fadeTime << endl;
    m_parameters.fadeTo = volume;
    m_parameters.fadeTime = fadeTime;
    if (m_pluginApi) {
        m_pluginApi->set_parameters(m_plugin, &m_parameters);
    }
}

}} //namespace Phonon::Xine

#include "volumefadereffect.moc"
// vim: sw=4 ts=4
