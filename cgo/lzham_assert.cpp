// File: lzham_assert.cpp
// See Copyright Notice and license at the end of lzham.h
#include "lzham_core.h"

void lzham_assert(const char* pExp, const char* pFile, unsigned line)
{
   printf("%s(%u): Assertion failed: \"%s\"\n", pFile, line, pExp);
}
