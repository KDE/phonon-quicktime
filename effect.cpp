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

#include "effect.h"
#include <klocale.h>
#include <QVariant>
#include "xineengine.h"
#include <QMutexLocker>

namespace Phonon
{
namespace Xine
{
Effect::Effect( int effectId, QObject* parent )
    : QObject(parent),
    m_plugin(0),
    m_pluginApi(0),
    m_pluginName(0),
    m_pluginParams(0)
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

Effect::Effect(const char *name, QObject *parent)
    : QObject(parent),
    m_plugin(0),
    m_pluginApi(0),
    m_pluginName(name),
    m_pluginParams(0)
{
}

Effect::~Effect()
{
    if (m_plugin) {
        xine_post_dispose(XineEngine::xine(), m_plugin);
    }
    free(m_pluginParams);
}

bool Effect::isValid() const
{
    return m_pluginName != 0;
}

xine_post_out_t *Effect::audioOutputPort() const
{
    if (!m_plugin) {
        // lazy initialization
        const_cast<Effect *>(this)->newInstance(XineEngine::nullPort());
    }
    xine_post_out_t *x = xine_post_output(m_plugin, "audio out");
    Q_ASSERT(x);
    return x;
}

MediaStreamTypes Effect::inputMediaStreamTypes() const
{
    return Phonon::Audio;
}

MediaStreamTypes Effect::outputMediaStreamTypes() const
{
    return Phonon::Audio;
}

void Effect::rewireTo(SourceNode *source)
{
    if (!m_plugin) {
        // lazy initialization
        const_cast<Effect *>(this)->newInstance(XineEngine::nullPort());
    }
    xine_post_in_t *x = xine_post_input(m_plugin, "audio in");
    Q_ASSERT(x);
    xine_post_wire(source->audioOutputPort(), x);
}

QList<EffectParameter> Effect::allParameters()
{
    ensureParametersReady();
    return m_parameterList;
}

EffectParameter Effect::parameter(int parameterIndex)
{
    ensureParametersReady();
    if (parameterIndex >= m_parameterList.size()) {
        return EffectParameter();
    }
    return m_parameterList[parameterIndex];
}

int Effect::parameterCount()
{
    ensureParametersReady();
    return m_parameterList.size();
}

void Effect::ensureParametersReady()
{
    if (m_parameterList.isEmpty() && !m_plugin) {
        newInstance(XineEngine::nullPort());
        if (m_plugin) {
            xine_post_dispose(XineEngine::xine(), m_plugin);
            m_plugin = 0;
            if (m_pluginApi) {
                // FIXME: how is it freed?
                m_pluginApi = 0;
            }
        }
    }
}

xine_post_t *Effect::newInstance(xine_audio_port_t *audioPort)
{
    Q_ASSERT(m_plugin == 0 && m_pluginApi == 0);
    QMutexLocker lock(&m_mutex);
    if (m_pluginName) {
        m_plugin = xine_post_init(XineEngine::xine(), m_pluginName, 1, &audioPort, 0);
        xine_post_in_t *paraInput = xine_post_input(m_plugin, "parameters");
        if (paraInput) {
            Q_ASSERT(paraInput->type == XINE_POST_DATA_PARAMETERS);
            Q_ASSERT(paraInput->data);
            m_pluginApi = reinterpret_cast<xine_post_api_t *>(paraInput->data);
            if (m_parameterList.isEmpty()) {
                xine_post_api_descr_t *desc = m_pluginApi->get_param_descr();
                Q_ASSERT(0 == m_pluginParams);
                m_pluginParams = static_cast<char *>(malloc(desc->struct_size));
                m_pluginApi->get_parameters(m_plugin, m_pluginParams);
                for (int i = 0; desc->parameter[i].type != POST_PARAM_TYPE_LAST; ++i) {
                    xine_post_api_parameter_t &p = desc->parameter[i];
                    switch (p.type) {
                    case POST_PARAM_TYPE_INT:          /* integer (or vector of integers)    */
                        addParameter(EffectParameter(i, p.name, EffectParameter::IntegerHint,
                                    *reinterpret_cast<int *>(m_pluginParams + p.offset),
                                    static_cast<int>(p.range_min), static_cast<int>(p.range_max), p.description));
                        break;
                    case POST_PARAM_TYPE_DOUBLE:       /* double (or vector of doubles)      */
                        addParameter(EffectParameter(i, p.name, 0,
                                    *reinterpret_cast<double *>(m_pluginParams + p.offset),
                                    p.range_min, p.range_max, p.description));
                        break;
                    case POST_PARAM_TYPE_CHAR:         /* char (or vector of chars = string) */
                    case POST_PARAM_TYPE_STRING:       /* (char *), ASCIIZ                   */
                    case POST_PARAM_TYPE_STRINGLIST:   /* (char **) list, NULL terminated    */
                        kWarning(610) << "char/string/stringlist parameter '" << p.name << "' not supported." << endl;
                        break;
                    case POST_PARAM_TYPE_BOOL:         /* integer (0 or 1)                   */
                        addParameter(EffectParameter(i, p.name, EffectParameter::ToggledHint,
                                    static_cast<bool>(*reinterpret_cast<int *>(m_pluginParams + p.offset)),
                                    QVariant(), QVariant(), p.description));
                        break;
                    case POST_PARAM_TYPE_LAST:         /* terminator of parameter list       */
                    default:
                        abort();
                    }
                }
            }
        }
        return m_plugin;
    }
    return 0;
}

QVariant Effect::parameterValue(int parameterIndex) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_plugin || !m_pluginApi) {
        return QVariant(); // invalid
    }

    xine_post_api_descr_t *desc = m_pluginApi->get_param_descr();
    Q_ASSERT(m_pluginParams);
    m_pluginApi->get_parameters(m_plugin, m_pluginParams);
    int i = 0;
    for (; i < parameterIndex && desc->parameter[i].type != POST_PARAM_TYPE_LAST; ++i);
    if (i == parameterIndex) {
        xine_post_api_parameter_t &p = desc->parameter[i];
        switch (p.type) {
            case POST_PARAM_TYPE_INT:          /* integer (or vector of integers)    */
                return *reinterpret_cast<int *>(m_pluginParams + p.offset);
            case POST_PARAM_TYPE_DOUBLE:       /* double (or vector of doubles)      */
                return *reinterpret_cast<double *>(m_pluginParams + p.offset);
            case POST_PARAM_TYPE_CHAR:         /* char (or vector of chars = string) */
            case POST_PARAM_TYPE_STRING:       /* (char *), ASCIIZ                   */
            case POST_PARAM_TYPE_STRINGLIST:   /* (char **) list, NULL terminated    */
                kWarning(610) << "char/string/stringlist parameter '" << p.name << "' not supported." << endl;
                return QVariant();
            case POST_PARAM_TYPE_BOOL:         /* integer (0 or 1)                   */
                return static_cast<bool>(*reinterpret_cast<int *>(m_pluginParams + p.offset));
            case POST_PARAM_TYPE_LAST:         /* terminator of parameter list       */
                break;
            default:
                abort();
        }
    }
    kError(610) << "invalid parameterIndex passed to Effect::value" << endl;
    return QVariant();
}

void Effect::setParameterValue(int parameterIndex, const QVariant &newValue)
{
    QMutexLocker lock(&m_mutex);
    if (!m_plugin || !m_pluginApi) {
        return;
    }

    xine_post_api_descr_t *desc = m_pluginApi->get_param_descr();
    Q_ASSERT(m_pluginParams);
    int i = 0;
    for (; i < parameterIndex && desc->parameter[i].type != POST_PARAM_TYPE_LAST; ++i);
    if (i == parameterIndex) {
        xine_post_api_parameter_t &p = desc->parameter[i];
        switch (p.type) {
            case POST_PARAM_TYPE_INT:          /* integer (or vector of integers)    */
                {
                    int *value = reinterpret_cast<int *>(m_pluginParams + p.offset);
                    *value = newValue.toInt();
                }
                break;
            case POST_PARAM_TYPE_DOUBLE:       /* double (or vector of doubles)      */
                {
                    double *value = reinterpret_cast<double *>(m_pluginParams + p.offset);
                    *value = newValue.toDouble();
                }
                break;
            case POST_PARAM_TYPE_CHAR:         /* char (or vector of chars = string) */
            case POST_PARAM_TYPE_STRING:       /* (char *), ASCIIZ                   */
            case POST_PARAM_TYPE_STRINGLIST:   /* (char **) list, NULL terminated    */
                kWarning(610) << "char/string/stringlist parameter '" << p.name << "' not supported." << endl;
                return;
            case POST_PARAM_TYPE_BOOL:         /* integer (0 or 1)                   */
                {
                   int *value = reinterpret_cast<int *>(m_pluginParams + p.offset);
                   *value = newValue.toBool() ? 1 : 0;
                }
                break;
            case POST_PARAM_TYPE_LAST:         /* terminator of parameter list       */
                kError(610) << "invalid parameterIndex passed to Effect::setValue" << endl;
                break;
            default:
                abort();
        }
        m_pluginApi->set_parameters(m_plugin, m_pluginParams);
    } else {
        kError(610) << "invalid parameterIndex passed to Effect::setValue" << endl;
    }
}

}} //namespace Phonon::Xine

#include "moc_effect.cpp"
