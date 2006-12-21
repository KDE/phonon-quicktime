/*  This file is part of the KDE project
    Copyright (C) 2006 David Faure <faure@kde.org>

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
#ifndef PHONON_XINEENGINE_EXPORT_H
#define PHONON_XINEENGINE_EXPORT_H

/* needed for KDE_EXPORT macros */
#include <kdemacros.h>

#if defined _WIN32 || defined _WIN64

#ifndef PHONON_XINEENGINE_EXPORT
# if defined(MAKE_PHONONXINEENGINEENGINE_LIB)
#  define PHONON_XINEENGINE_EXPORT KDE_EXPORT
# else
#  define PHONON_XINEENGINE_EXPORT KDE_IMPORT
# endif
#endif


#else /* UNIX */

#define PHONON_XINEENGINE_EXPORT KDE_EXPORT

#endif

#endif
