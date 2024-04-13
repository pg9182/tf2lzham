// File: lzham_core.h
// See Copyright Notice and license at the end of lzham.h
#pragma once

#if defined(_MSC_VER)
   #pragma warning (disable: 4127) // conditional expression is constant
#endif

#if defined(_WIN64) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__)
  #define LZHAM_64BIT_POINTERS 1
  #define LZHAM_CPU_HAS_64BIT_REGISTERS 1
#else
  #define LZHAM_64BIT_POINTERS 0
  #define LZHAM_CPU_HAS_64BIT_REGISTERS 0
#endif

#define LZHAM_USE_UNALIGNED_INT_LOADS 0

#if __BIG_ENDIAN__
  #define LZHAM_BIG_ENDIAN_CPU 1
#else
  #define LZHAM_LITTLE_ENDIAN_CPU 1
#endif

#define LZHAM_RESTRICT
#define LZHAM_FORCE_INLINE inline

#define LZHAM_NOTE_UNUSED(x) (void)x

#if LZHAM_LITTLE_ENDIAN_CPU
   const bool c_lzham_little_endian_platform = true;
#else
   const bool c_lzham_little_endian_platform = false;
#endif

const bool c_lzham_big_endian_platform = !c_lzham_little_endian_platform;

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <limits.h>
#include <algorithm>
#include <errno.h>

#include "lzham.h"
#include "lzham_types.h"
#include "lzham_assert.h"
#include "lzham_platform.h"

#include "lzham_helpers.h"
#include "lzham_traits.h"
#include "lzham_mem.h"
#include "lzham_math.h"
#include "lzham_utils.h"
#include "lzham_vector.h"
