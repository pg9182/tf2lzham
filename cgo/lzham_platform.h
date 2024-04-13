// File: lzham_platform.h
// See Copyright Notice and license at the end of lzham.h
#pragma once

// actually in lzham_assert.cpp
void lzham_assert(const char* pExp, const char* pFile, unsigned line);
void lzham_fail(const char* pExp, const char* pFile, unsigned line);

#define LZHAM_BREAKPOINT
#define LZHAM_BUILTIN_EXPECT(c, v) c

#if defined(__GNUC__) && LZHAM_PLATFORM_PC
extern __inline__ __attribute__((__always_inline__,__gnu_inline__)) void lzham_yield_processor()
{
   __asm__ __volatile__("pause");
}
#else
LZHAM_FORCE_INLINE void lzham_yield_processor()
{
}
#endif

#ifdef __APPLE__
#include <malloc/malloc.h>
#define malloc_usable_size malloc_size
#else
#include <malloc.h>
#endif
#ifdef __MINGW32__
#define malloc_usable_size _msize
#endif

// Note: It's very important that LZHAM_READ_BIG_ENDIAN_UINT32() is fast on the target platform.
// This is used to read every DWORD from the input stream.

#if LZHAM_USE_UNALIGNED_INT_LOADS
   #if LZHAM_BIG_ENDIAN_CPU
      #define LZHAM_READ_BIG_ENDIAN_UINT32(p) *reinterpret_cast<const uint32*>(p)
   #else
      #if defined(LZHAM_USE_MSVC_INTRINSICS)
         #define LZHAM_READ_BIG_ENDIAN_UINT32(p) _byteswap_ulong(*reinterpret_cast<const uint32*>(p))
      #elif defined(__GNUC__)
         #define LZHAM_READ_BIG_ENDIAN_UINT32(p) __builtin_bswap32(*reinterpret_cast<const uint32*>(p))
      #else
         #define LZHAM_READ_BIG_ENDIAN_UINT32(p) utils::swap32(*reinterpret_cast<const uint32*>(p))
      #endif
   #endif
#else
   #define LZHAM_READ_BIG_ENDIAN_UINT32(p) ((reinterpret_cast<const uint8*>(p)[0] << 24) | (reinterpret_cast<const uint8*>(p)[1] << 16) | (reinterpret_cast<const uint8*>(p)[2] << 8) | (reinterpret_cast<const uint8*>(p)[3]))
#endif

namespace lzham
{
   #define LZHAM_NO_ATOMICS 1

   // Atomic ops not supported - but try to do something reasonable. Assumes no threading at all.
   typedef long atomic32_t;
   typedef long long atomic64_t;

   inline atomic32_t atomic_compare_exchange32(atomic32_t volatile *pDest, atomic32_t exchange, atomic32_t comparand)
   {
      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(pDest) & 3) == 0);
      atomic32_t cur = *pDest;
      if (cur == comparand)
         *pDest = exchange;
      return cur;
   }

   inline atomic64_t atomic_compare_exchange64(atomic64_t volatile *pDest, atomic64_t exchange, atomic64_t comparand)
   {
      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(pDest) & 7) == 0);
      atomic64_t cur = *pDest;
      if (cur == comparand)
         *pDest = exchange;
      return cur;
   }

   inline atomic32_t atomic_increment32(atomic32_t volatile *pDest)
   {
      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(pDest) & 3) == 0);
      return (*pDest += 1);
   }

   inline atomic32_t atomic_decrement32(atomic32_t volatile *pDest)
   {
      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(pDest) & 3) == 0);
      return (*pDest -= 1);
   }

   inline atomic32_t atomic_exchange32(atomic32_t volatile *pDest, atomic32_t val)
   {
      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(pDest) & 3) == 0);
      atomic32_t cur = *pDest;
      *pDest = val;
      return cur;
   }

   inline atomic32_t atomic_add32(atomic32_t volatile *pDest, atomic32_t val)
   {
      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(pDest) & 3) == 0);
      return (*pDest += val);
   }

   inline atomic32_t atomic_exchange_add(atomic32_t volatile *pDest, atomic32_t val)
   {
      LZHAM_ASSERT((reinterpret_cast<ptr_bits_t>(pDest) & 3) == 0);
      atomic32_t cur = *pDest;
      *pDest += val;
      return cur;
   }

} // namespace lzham
