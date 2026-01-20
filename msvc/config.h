/*
 * config.h - MSVC 2022+ Configuration for FIASCO
 * 
 * This file provides Windows/MSVC compatibility definitions.
 * Generated for Aurora Fractal-RAG project.
 */

#ifndef _CONFIG_H
#define _CONFIG_H

/* MSVC-specific definitions */
#ifdef _MSC_VER
#define WINDOWS 1

/* Disable specific warnings */
#pragma warning(disable: 4996)  /* deprecated functions */
#pragma warning(disable: 4244)  /* conversion, possible loss of data */
#pragma warning(disable: 4267)  /* size_t to int conversion */
#pragma warning(disable: 4018)  /* signed/unsigned mismatch */

/* Define missing POSIX functions */
/* Note: snprintf is standard in MSVC 2015+, no redefinition needed */
#define strdup _strdup
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define popen _popen
#define pclose _pclose
#define fileno _fileno
#define isatty _isatty

/* Math functions */
#define _USE_MATH_DEFINES
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

/* Boolean type for older MSVC */
#if _MSC_VER < 1800
typedef int bool;
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif

/* Standard integer types */
#include <stdint.h>

/* For getopt */
#define HAVE_GETOPT_H 0

/* Package info */
#define PACKAGE "fiasco"
#define PACKAGE_STRING "fiasco 1.0"
#define VERSION "1.0"

/* FIASCO data paths */
#ifndef FIASCO_SHARE
#define FIASCO_SHARE "."
#endif

/* Feature flags */
#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_LOG2 1      /* MSVC has log2 as intrinsic */
#define HAVE_STRDUP 1    /* MSVC has strdup as _strdup */
#define HAVE_VPRINTF 1   /* MSVC has vprintf/vfprintf */
#define HAVE_SETJMP_H 1  /* MSVC has setjmp.h */
#define HAVE_STRCASECMP 1  /* Via _stricmp */

/* Inline keyword */
#ifndef inline
#define inline __inline
#endif

/* Restrict keyword */
#ifndef restrict
#define restrict __restrict
#endif

/* Export macro for DLL */
#ifdef FIASCO_BUILD_DLL
#define FIASCO_API __declspec(dllexport)
#else
#define FIASCO_API __declspec(dllimport)
#endif

#ifdef FIASCO_STATIC
#undef FIASCO_API
#define FIASCO_API
#endif

#else /* not _MSC_VER */

#define FIASCO_API

#endif /* _MSC_VER */

#endif /* _CONFIG_H */
