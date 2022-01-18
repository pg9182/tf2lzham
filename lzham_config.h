// File: lzham_config.h
// See Copyright Notice and license at the end of lzham.h
#pragma once

#ifdef _DEBUG
   #define LZHAM_BUILD_DEBUG
   
   #ifndef DEBUG
      #define DEBUG
   #endif
#else
   #define LZHAM_BUILD_RELEASE
   
   #ifndef NDEBUG
      #define NDEBUG
   #endif
   
   #ifdef DEBUG
      #error DEBUG cannot be defined in LZHAM_BUILD_RELEASE
   #endif
#endif
#define LZHAM_BUFFERED_PRINTF 0
#define LZHAM_PERF_SECTIONS 0