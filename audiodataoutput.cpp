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

#include "audiodataoutput.h"
#include <QVector>
#include <QMap>

namespace Phonon
{
namespace Xine
{
AudioDataOutput::AudioDataOutput( QObject* parent )
	: AbstractAudioOutput( parent )
{
}

AudioDataOutput::~AudioDataOutput()
{
}

Phonon::AudioDataOutput::Format AudioDataOutput::format() const
{
	return m_format;
}

int AudioDataOutput::dataSize() const
{
	return m_dataSize;
}

int AudioDataOutput::sampleRate() const
{
	return 44100;
}

void AudioDataOutput::setFormat( Phonon::AudioDataOutput::Format format )
{
	m_format = format;
}

void AudioDataOutput::setDataSize( int size )
{
	m_dataSize = size;
}

typedef QMap<Phonon::AudioDataOutput::Channel, QVector<float> > FloatMap;
typedef QMap<Phonon::AudioDataOutput::Channel, QVector<qint16> > IntMap;

inline void AudioDataOutput::convertAndEmit( const QVector<float>& buffer )
{
	if( m_format == Phonon::AudioDataOutput::FloatFormat )
	{
		FloatMap map;
		map.insert( Phonon::AudioDataOutput::LeftChannel, buffer );
		map.insert( Phonon::AudioDataOutput::RightChannel, buffer );
		emit dataReady( map );
	}
	else
	{
		IntMap map;
		QVector<qint16> intBuffer( m_dataSize );
		for( int i = 0; i < m_dataSize; ++i )
			intBuffer[ i ] = static_cast<qint16>( buffer[ i ] * static_cast<float>( 0x7FFF ) );
		map.insert( Phonon::AudioDataOutput::LeftChannel, intBuffer );
		map.insert( Phonon::AudioDataOutput::RightChannel, intBuffer );
		emit dataReady( map );
	}
}

}} //namespace Phonon::Xine

#include "audiodataoutput.moc"
// vim: sw=4 ts=4
