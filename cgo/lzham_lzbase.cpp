// File: lzham_lzbase.cpp
// See Copyright Notice and license at the end of lzham.h
#include "lzham_core.h"
#include "lzham_lzbase.h"

namespace lzham
{
   void CLZBase::init_slot_tabs()
   {
      for (uint i = 0; i < m_num_lzx_slots; i++)
      {
         uint lo = m_lzx_position_base[i];
         uint hi = lo + m_lzx_position_extra_mask[i];

         uint8* pTab;
         uint shift;
         uint n; LZHAM_NOTE_UNUSED(n);

         if (hi < 0x1000)
         {
            pTab = m_slot_tab0;
            shift = 0;
            n = sizeof(m_slot_tab0);
         }
         else if (hi < 0x100000)
         {
            pTab = m_slot_tab1;
            shift = 11;
            n = sizeof(m_slot_tab1);
         }
         else if (hi < 0x1000000)
         {
            pTab = m_slot_tab2;
            shift = 16;
            n = sizeof(m_slot_tab2);
         }
         else
            break;

         lo >>= shift;
         hi >>= shift;

         LZHAM_ASSERT(hi < n);
         memset(pTab + lo, (uint8)i, hi - lo + 1);
      }
   }
} //namespace lzham

