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

#ifndef PHONON_XINE_XINESTREAM_H
#define PHONON_XINE_XINESTREAM_H

#include <QThread>
#include <QMutex>
#include <QMultiMap>
#include <phonon/phononnamespace.h>
#include <xine.h>
#include <QWaitCondition>

namespace Phonon
{
namespace Xine
{

/**
 * \brief xine_stream_t wrapper that runs in its own thread.
 *
 * The xine_stream_t object is created as late as possible so that it doesn't have to be recreated
 * when an audio- or video_port is added.
 *
 * \author Matthias Kretz <kretz@kde.org>
 */
class XineStream : public QThread
{
    Q_OBJECT
    public:
        XineStream(QObject *parent = 0);

        Phonon::State state() const { return m_state; }

        int totalTime() const;
        int remainingTime() const;
        int currentTime() const;
        bool hasVideo() const;
        bool isSeekable() const;
        void setVolume(int vol);
        void setAudioPort(xine_audio_port_t *port);
        void setVideoPort(xine_video_port_t *port);

        void setParam(int param, int value) { xine_set_param(m_stream, param, value); }

    public slots:
        void setUrl(const KUrl &url);
        void setMrl(const QByteArray &mrl);
        void play();
        void pause();
        void stop();
        void seek(qint64 time);

    Q_SIGNALS:
        /**
         * emitted from the xine thread
         */
        void stateChanged(Phonon::State newstate, Phonon::State oldstate);
        /**
         * emitted from the xine thread
         */
        void metaDataChanged(const QMultiMap<QString, QString>&);
        /**
         * emitted from the xine thread
         */
        void length(qint64);
        /**
         * emitted from the xine thread
         */
        void seekDone();

    protected:
        bool event(QEvent *ev);
        void run();

    private slots:
        void getStartTime();

    private:
        void xineOpen();
        void updateMetaData();
        void recreateStream();
        bool createStream();
        void changeState(Phonon::State newstate);

        xine_stream_t *m_stream;
        xine_event_queue_t *m_event_queue;

        xine_audio_port_t *m_audioPort;
        xine_video_port_t *m_videoPort;

        Phonon::State m_state;

        mutable QMutex m_mutex;
        mutable QWaitCondition m_waitingForXineOpen;
        mutable QWaitCondition m_waitingForStreamInfo;
        QMultiMap<QString, QString> m_metaDataMap;
        QByteArray m_mrl;

        int m_volume;
        int m_startTime;
        bool m_streamInfoReady : 1;
        bool m_hasVideo : 1;
        bool m_isSeekable : 1;
        bool m_recreateEventSent : 1;
};

} // namespace Xine
} // namespace Phonon

#endif // PHONON_XINE_XINESTREAM_H
