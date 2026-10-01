/*
 * LAME build configuration for Audionaut.
 *
 * LAME ships an autotools-generated config.h (and configMS.h for MSVC);
 * Audionaut compiles the encoder sources directly from its own build
 * systems (Projucer, CMake), so this hand-written header stands in for
 * both. It is force-included into every LAME source (-include / /FI)
 * rather than found as <config.h> via HAVE_CONFIG_H, so no other
 * config.h on the app's include paths can be picked up by mistake.
 * It describes a plain C99 encoder build on macOS, Linux and Windows:
 * no decoder (mpglib/mpg123), no SSE or NASM code paths.
 */

#ifndef AUDIONAUT_LAME_CONFIG_H
#define AUDIONAUT_LAME_CONFIG_H

#define STDC_HEADERS 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCHR 1
#define HAVE_MEMCPY 1

#define PROTOTYPES 1
#define PACKAGE "lame"
#define LAME_LIBRARY_BUILD 1

/* faster log implementation with less but enough precision (as upstream) */
#define USE_FAST_LOG 1

/* no frame analysis hooks (only the mp3x frontend uses them) */
#define NOANALYSIS 1

/* the IEEE float types LAME expects; C99 float/double are exactly these */
typedef float  ieee754_float32_t;
typedef double ieee754_float64_t;
typedef long double ieee854_float80_t;

#endif
