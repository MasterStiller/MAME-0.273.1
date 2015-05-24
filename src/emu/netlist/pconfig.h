// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * pconfig.h
 *
 */

#ifndef PCONFIG_H_
#define PCONFIG_H_

#ifndef PSTANDALONE
	#define PSTANDALONE (0)
#endif

#if defined(__GNUC__) && (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3))
#if !defined(__ppc__) && !defined (__PPC__) && !defined(__ppc64__) && !defined(__PPC64__)
#define ATTR_ALIGN __attribute__ ((aligned(64)))
#else
#define ATTR_ALIGN
#endif
#else
#define ATTR_ALIGN
#endif

//============================================================
//  Compiling standalone
//============================================================

// Compiling without mame ?

#if !(PSTANDALONE)
#include "osdcore.h"
#else
#define ATTR_HOT
#define ATTR_COLD

#define RESTRICT
#define EXPECTED(x)		(x)
#define UNEXPECTED(x)	(x)
#define ATTR_PRINTF(n1,n2)
#define ATTR_UNUSED             __attribute__((__unused__))

/* 8-bit values */
typedef unsigned char                       UINT8;
typedef signed char                         INT8;

/* 16-bit values */
typedef unsigned short                      UINT16;
typedef signed short                        INT16;

/* 32-bit values */
#ifndef _WINDOWS_H
typedef unsigned int                        UINT32;
typedef signed int                          INT32;
#endif

/* 64-bit values */
#ifndef _WINDOWS_H
#ifdef _MSC_VER
typedef signed __int64                      INT64;
typedef unsigned __int64                    UINT64;
#else
__extension__ typedef unsigned long long    UINT64;
__extension__ typedef signed long long      INT64;
#endif
#endif

/* U64 and S64 are used to wrap long integer constants. */
#if defined(__GNUC__) || defined(_MSC_VER)
#define U64(val) val##ULL
#define S64(val) val##LL
#else
#define U64(val) val
#define S64(val) val
#endif

#endif

#endif /* PCONFIG_H_ */
