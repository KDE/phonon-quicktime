/*  This file is part of the KDE project
    Copyright (C) 2006 Matthias Kretz <kretz@kde.org>

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

#ifndef AUDIODATAOUTPUT_P_H
#define AUDIODATAOUTPUT_P_H

#include "audiodataoutput.h"
#include "ifaces/audiodataoutput.h"
#include "abstractaudiooutput_p.h"

namespace Phonon
{
class AudioDataOutputPrivate : public AbstractAudioOutputPrivate
{
	Q_DECLARE_PUBLIC( AudioDataOutput )
	PHONON_PRIVATECLASS( AudioDataOutput, AbstractAudioOutput )
	protected:
		int availableSamples;
};
} //namespace Phonon

#endif // AUDIODATAOUTPUT_P_H
// vim: sw=4 ts=4 tw=80
