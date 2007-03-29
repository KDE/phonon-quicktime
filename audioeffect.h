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
#include "audiopostlist.h"
#include <phonon/effectparameter.h>
#include <QList>
#include <xine.h>
#include <QMutex>

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

            bool isValid() const;

            /**
             * calls xine_post_init for one input, the given audio port and no
             * video port
             *
             * \warning called from the xine thread
             */
            virtual xine_post_t *newInstance(xine_audio_port_t *);

		public slots:
            QList<EffectParameter> parameterList() const { const_cast<AudioEffect *>(this)->ensureParametersReady(); return m_parameterList; }
			QVariant value( int parameterId ) const;
			void setValue( int parameterId, QVariant newValue );

        protected:
            virtual void ensureParametersReady();
            AudioEffect(const char *name, QObject *parent);
            void addParameter(const EffectParameter &p) { m_parameterList << p; }

            QList<xine_post_t *> m_plugins;
            QList<xine_post_api_t *> m_pluginApis;

        private:
            mutable QMutex m_mutex;
            const char *m_pluginName;
            char *m_pluginParams;
            AudioPostList m_postList;
            QList<Phonon::EffectParameter> m_parameterList;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80
#endif // Phonon_XINE_AUDIOEFFECT_H
