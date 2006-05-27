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

#ifndef PHONON_UI_MEDIACONTROLS_H
#define PHONON_UI_MEDIACONTROLS_H

#include <QWidget>
#include <kdelibs_export.h>
#include "../phononnamespace.h"

namespace Phonon
{
class AbstractMediaProducer;
class AudioOutput;

/**
 * \short Simple widget showing buttons to control an AbstractMediaProducer
 * object.
 *
 * This widget shows the standard player controls. There's at least the
 * play/pause and stop buttons. If the media is seekable it shows a seek-slider.
 * Optional controls include a volume control and a loop control button.
 *
 * \author Matthias Kretz <kretz@kde.org>
 */
class PHONONUI_EXPORT MediaControls : public QWidget
{
	Q_OBJECT
	/**
	 * This property holds whether the slider showing the progress of the
	 * playback is visible.
	 *
	 * By default the slider is visible. It is enabled/disabled automatically
	 * depending on whether the media can be seeked or not.
	 */
	Q_PROPERTY( bool seekSliderVisible READ isSeekSliderVisible WRITE setSeekSliderVisible )

	/**
	 * This property holds whether the slider controlling the volume is visible.
	 *
	 * By default the slider is visible if an AudioOutput has been set with
	 * setAudioOutput.
	 *
	 * \see setAudioOutput
	 */
	Q_PROPERTY( bool volumeControlVisible READ isVolumeControlVisible WRITE setVolumeControlVisible )

	/**
	 * This property holds whether the button controlling loop behaviour is
	 * visible.
	 *
	 * By default the loop button is hidden.
	 */
	Q_PROPERTY( bool loopControlVisible READ isLoopControlVisible WRITE setLoopControlVisible )
	public:
		/**
		 * Constructs a media control widget with a \p parent.
		 */
		MediaControls( QWidget* parent = 0 );
		~MediaControls();

		bool isSeekSliderVisible() const;
		bool isVolumeControlVisible() const;
		bool isLoopControlVisible() const;

	public Q_SLOTS:
		void setSeekSliderVisible( bool );
		void setVolumeControlVisible( bool );
		void setLoopControlVisible( bool );

		/**
		 * Sets the media producer object to be controlled by this widget.
		 */
		void setMediaProducer( AbstractMediaProducer* );

		/**
		 * Sets the audio output object to be controlled by this widget.
		 */
		void setAudioOutput( AudioOutput* audioOutput );

	private Q_SLOTS:
		void stateChanged( Phonon::State, Phonon::State );
		void mediaDestroyed();

	private:
		class Private;
		Private* d;
};

} // namespace Phonon

// vim: sw=4 ts=4 tw=80
#endif // PHONON_UI_MEDIACONTROLS_H
