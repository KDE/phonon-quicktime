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

#include "audioeffect.h"
#include <klocale.h>
#include <QVariant>
#include "xineengine.h"

namespace Phonon
{
namespace Xine
{
AudioEffect::AudioEffect( int effectId, QObject* parent )
    : QObject(parent),
    m_pluginName(0)
{
    const char *const *postPlugins = xine_list_post_plugins_typed(XineEngine::xine(), XINE_POST_TYPE_AUDIO_FILTER);
    if (effectId >= 0x7F000000) {
        effectId -= 0x7F000000;
        for(int i = 0; postPlugins[i]; ++i) {
            if (i == effectId) {
                // found it
                m_pluginName = postPlugins[i];
                break;
            }
        }
    }
}

AudioEffect::AudioEffect(const char *name, QObject *parent)
    : QObject(parent),
    m_pluginName(name)
{
}

AudioEffect::~AudioEffect()
{
    foreach (xine_post_t *post, m_plugins) {
        xine_post_dispose(XineEngine::xine(), post);
    }
}

bool AudioEffect::isValid() const
{
    return m_pluginName != 0;
}

xine_post_t *AudioEffect::newInstance(xine_audio_port_t *audioPort)
{
    if (m_pluginName) {
        xine_post_t *x = xine_post_init(XineEngine::xine(), m_pluginName, 1, &audioPort, 0);
        m_plugins << x;
        xine_post_in_t *paraInput = xine_post_input(x, "parameters");
        if (paraInput) {
            Q_ASSERT(paraInput->type == XINE_POST_DATA_PARAMETERS);
            Q_ASSERT(paraInput->data);
            m_pluginApis << reinterpret_cast<xine_post_api_t *>(paraInput->data);
            if (m_parameterList.isEmpty()) {
                // TODO: add EffectParameter objects
            }
        } else {
            m_pluginApis << 0;
        }
        return x;
    }
    return 0;
}

QVariant AudioEffect::value( int parameterId ) const
{
	return QVariant(); // invalid
}

void AudioEffect::setValue( int parameterId, QVariant newValue )
{
}

}} //namespace Phonon::Xine

#include "audioeffect.moc"
// vim: sw=4 ts=4
