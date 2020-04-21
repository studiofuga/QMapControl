#pragma once

// Qt includes.
#include <QtCore/QtGlobal>

/*!
 * @author Kai Winter <kaiwinter@gmx.de>
 * @author Chris Stylianou <chris5287@gmail.com>
 */

/*
#ifdef QMAPCONTROL_LIBRARY
    /// Defines that the specified function, variable or class should be exported.
    #define QMAPCONTROL_EXPORT Q_DECL_EXPORT
#else
    /// Defines that the specified function, variable or class should be imported.
    #define QMAPCONTROL_EXPORT Q_DECL_IMPORT
#endif
*/

//#define QMAP_DEBUG 1

// MSVC does not define M_PI in <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define QMAPCONTROL_EXPORT

constexpr int kDefaultMaxZoom = 20;
