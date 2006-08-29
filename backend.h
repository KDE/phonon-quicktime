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

#ifndef Phonon_XINE_BACKEND_H
#define Phonon_XINE_BACKEND_H

#include <QList>
#include <QPointer>
#include <QStringList>

#include <xine.h>
#include <xine/xineutils.h>

#include "xine_engine.h"
#include <QObject>
#include <phonon/objectdescription.h>

class KUrl;

namespace Phonon
{
namespace Xine
{
	class AudioOutput;

	class Backend : public QObject
	{
		Q_OBJECT
		public:
			Backend( QObject* parent, const QStringList& args );
			~Backend();

			Q_INVOKABLE QObject* createMediaObject( QObject* parent );
			Q_INVOKABLE QObject* createAvCapture( QObject* parent );
			Q_INVOKABLE QObject* createByteStream( QObject* parent );

			Q_INVOKABLE QObject* createAudioPath( QObject* parent );
			Q_INVOKABLE QObject* createAudioEffect( int effectId, QObject* parent );
			Q_INVOKABLE QObject* createVolumeFaderEffect( QObject* parent );
			Q_INVOKABLE QObject* createAudioOutput( QObject* parent );
			Q_INVOKABLE QObject* createAudioDataOutput( QObject* parent );
			Q_INVOKABLE QObject* createVisualization( QObject* parent );

			Q_INVOKABLE QObject* createVideoPath( QObject* parent );
			Q_INVOKABLE QObject* createVideoEffect( int effectId, QObject* parent );
			Q_INVOKABLE QObject* createVideoDataOutput( QObject* parent );

			Q_INVOKABLE bool supportsVideo() const;
			Q_INVOKABLE bool supportsOSD() const;
			Q_INVOKABLE bool supportsFourcc( quint32 fourcc ) const;
			Q_INVOKABLE bool supportsSubtitles() const;

			Q_INVOKABLE void freeSoundcardDevices();

			Q_INVOKABLE QSet<int> objectDescriptionIndexes( ObjectDescriptionType ) const;
			Q_INVOKABLE QString objectDescriptionDescription( ObjectDescriptionType, int ) const;
			Q_INVOKABLE QString objectDescriptionName( ObjectDescriptionType, int ) const;

		public slots:
			QStringList knownMimeTypes();
			const char* uiLibrary() const;
			//const char* uiSymbol() const;

		private:
			QStringList m_supportedMimeTypes;	
	};
}} // namespace Phonon::Xine

// vim: sw=4 ts=4 noet tw=80
#endif // Phonon_XINE_BACKEND_H
