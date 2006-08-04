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
#ifndef Phonon_XINE_BYTESTREAM_H
#define Phonon_XINE_BYTESTREAM_H

#include "abstractmediaproducer.h"

#include "xine_engine.h"
#include <phonon/bytestreaminterface.h>

class QTimer;

namespace Phonon
{
namespace Xine
{
	class ByteStream : public AbstractMediaProducer, public ByteStreamInterface
	{
		Q_OBJECT
		Q_INTERFACES( Phonon::ByteStreamInterface )
		public:
			ByteStream( QObject* parent, XineEngine* xe );
			~ByteStream();

			qint64 currentTime() const;
			bool isSeekable() const;

			void writeData( const QByteArray& data );

			void play();
			void pause();
			void stop();
			void seek( qint64 time );

		public Q_SLOTS:
			void endOfData();
			void setStreamSeekable( bool );
			void setStreamSize( qint64 );
			qint64 streamSize() const;
			qint64 totalTime() const;
			bool streamSeekable() const;
			qint32 aboutToFinishTime() const;
			void setAboutToFinishTime( qint32 );

		Q_SIGNALS:
			void finished();
			void aboutToFinish( qint32 );
			void length( qint64 );
			void needData();
			void enoughData();
			void seekStream( qint64 );

		private Q_SLOTS:
			void consumeStream();

		private:
			XineEngine* m_xine_engine;
			qint64 m_aboutToFinishBytes;
			qint64 m_streamSize;
			qint64 m_bufferSize;
			qint64 m_streamPosition;
			bool m_streamSeekable;
			bool m_eof;
			bool m_aboutToFinishEmitted;
			QTimer* m_streamConsumeTimer;
	};
}} //namespace Phonon::Xine

// vim: sw=4 ts=4 tw=80 noet
#endif // Phonon_XINE_BYTESTREAM_H
