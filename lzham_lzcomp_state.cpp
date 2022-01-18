// File: lzham_lzcomp_state.cpp
// See Copyright Notice and license at the end of lzham.h
#include "lzham_core.h"
#include "lzham_lzcomp_internal.h"

namespace lzham
{
   static uint get_huge_match_code_len(uint len)
   {
      LZHAM_ASSERT((len > CLZBase::cMaxMatchLen) && (len <= CLZBase::cMaxHugeMatchLen));
      len -= (CLZBase::cMaxMatchLen + 1);

      if (len < 256)
         return 1 + 8;
      else if (len < (256 + 1024))
         return 2 + 10;
      else if (len < (256 + 1024 + 4096))
         return 3 + 12;
      else
         return 3 + 16;
   }

   static uint get_huge_match_code_bits(uint len)
   {
      LZHAM_ASSERT((len > CLZBase::cMaxMatchLen) && (len <= CLZBase::cMaxHugeMatchLen));
      len -= (CLZBase::cMaxMatchLen + 1);

      uint c;
      if (len < 256)
         c = len;
      else if (len < (256 + 1024))
      {
         uint r = (len - 256);
         LZHAM_ASSERT(r <= 1023);
         c = r | (2 << 10);
      }
      else if (len < (256 + 1024 + 4096))
      {
         uint r = (len - (256 + 1024));
         LZHAM_ASSERT(r <= 4095);
         c = r | (6 << 12);
      }
      else
      {
         uint r = (len - (256 + 1024 + 4096));
         LZHAM_ASSERT(r <= 65535);
         c = r | (7 << 16);
      }

      return c;
   }

   uint lzcompressor::lzdecision::get_match_dist(const state& cur_state) const
   {
      if (!is_match())
         return 0;
      else if (is_rep())
      {
         int index = -m_dist - 1;
         LZHAM_ASSERT(index < CLZBase::cMatchHistSize);
         return cur_state.m_match_hist[index];
      }
      else
         return m_dist;
   }

   lzcompressor::state::state()
   {
      clear();
   }

   void lzcompressor::state::clear()
   {
      m_cur_ofs = 0;
      m_cur_state = 0;
      m_block_start_dict_ofs = 0;

      for (uint i = 0; i < 2; i++)
      {
         m_rep_len_table[i].clear();
         m_large_len_table[i].clear();
      }
      m_main_table.clear();
      m_dist_lsb_table.clear();

      for (uint i = 0; i < (1 << CLZBase::cNumLitPredBits); i++)
         m_lit_table[i].clear();

      for (uint i = 0; i < (1 << CLZBase::cNumDeltaLitPredBits); i++)
         m_delta_lit_table[i].clear();

      m_match_hist[0] = 1;
      m_match_hist[1] = 1;
      m_match_hist[2] = 1;
      m_match_hist[3] = 1;
   }

   void lzcompressor::state::reset()
   {
      m_cur_ofs = 0;
      m_cur_state = 0;
      m_block_start_dict_ofs = 0;

      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_is_match_model); i++) 
         m_is_match_model[i].clear();
      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_is_rep_model); i++) 
         m_is_rep_model[i].clear();
      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_is_rep0_model); i++) 
         m_is_rep0_model[i].clear();
      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_is_rep0_single_byte_model); i++) 
         m_is_rep0_single_byte_model[i].clear();
      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_is_rep1_model); i++) 
         m_is_rep1_model[i].clear();
      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_is_rep2_model); i++) 
         m_is_rep2_model[i].clear();
      
      for (uint i = 0; i < 2; i++)
      {
         m_rep_len_table[i].reset();
         m_large_len_table[i].reset();
      }
      m_main_table.reset();
      m_dist_lsb_table.reset();

      // Only reset the first table in the array, then just clone it to the others because they're all the same when reset. (This is ~9x faster than resetting each one.)
      m_lit_table[0].reset();
      for (uint i = 1; i < LZHAM_ARRAY_SIZE(m_lit_table); i++)
         m_lit_table[i] = m_lit_table[0];

      m_delta_lit_table[0].reset();
      for (uint i = 1; i < LZHAM_ARRAY_SIZE(m_delta_lit_table); i++)
         m_delta_lit_table[i] = m_delta_lit_table[0];

      m_match_hist[0] = 1;
      m_match_hist[1] = 1;
      m_match_hist[2] = 1;
      m_match_hist[3] = 1;
   }

   bool lzcompressor::state::init(CLZBase& lzbase, bool fast_adaptive_huffman_updating, bool use_polar_codes)
   {
      m_cur_ofs = 0;
      m_cur_state = 0;

      if (!m_rep_len_table[0].init(true, CLZBase::cNumHugeMatchCodes + (CLZBase::cMaxMatchLen - CLZBase::cMinMatchLen + 1), fast_adaptive_huffman_updating, use_polar_codes)) 
         return false;
      if (!m_rep_len_table[1].assign(m_rep_len_table[0])) 
         return false;
      
      if (!m_large_len_table[0].init(true, CLZBase::cNumHugeMatchCodes + CLZBase::cLZXNumSecondaryLengths, fast_adaptive_huffman_updating, use_polar_codes)) 
         return false;
      if (!m_large_len_table[1].assign(m_large_len_table[0])) 
         return false;

      if (!m_main_table.init(true, CLZBase::cLZXNumSpecialLengths + (lzbase.m_num_lzx_slots - CLZBase::cLZXLowestUsableMatchSlot) * 8, fast_adaptive_huffman_updating, use_polar_codes)) 
         return false;
      if (!m_dist_lsb_table.init(true, 16, fast_adaptive_huffman_updating, use_polar_codes)) 
         return false;

      if (!m_lit_table[0].init(true, 256, fast_adaptive_huffman_updating, use_polar_codes)) 
         return false;
      for (uint i = 1; i < (1 << CLZBase::cNumLitPredBits); i++)
         if (!m_lit_table[i].assign(m_lit_table[0]))
            return false;

      if (!m_delta_lit_table[0].init(true, 256, fast_adaptive_huffman_updating, use_polar_codes)) 
         return false;
      for (uint i = 1; i < (1 << CLZBase::cNumDeltaLitPredBits); i++)
         if (!m_delta_lit_table[i].assign(m_delta_lit_table[0]))
            return false;

      m_match_hist[0] = 1;
      m_match_hist[1] = 1;
      m_match_hist[2] = 1;
      m_match_hist[3] = 1;

      return true;
   }

   void lzcompressor::state_base::partial_advance(const lzdecision& lzdec)
   {
      if (lzdec.m_len == 0)
      {
         if (m_cur_state < 4) m_cur_state = 0; else if (m_cur_state < 10) m_cur_state -= 3; else m_cur_state -= 6;
      }
      else
      {
         if (lzdec.m_dist < 0)
         {
            int match_hist_index = -lzdec.m_dist - 1;

            if (!match_hist_index)
            {
               if (lzdec.m_len == 1)
               {
                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 9 : 11;
               }
               else
               {
                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
               }
            }
            else
            {
               if (match_hist_index == 1)
               {
                  std::swap(m_match_hist[0], m_match_hist[1]);
               }
               else if (match_hist_index == 2)
               {
                  int dist = m_match_hist[2];
                  m_match_hist[2] = m_match_hist[1];
                  m_match_hist[1] = m_match_hist[0];
                  m_match_hist[0] = dist;
               }
               else
               {
                  LZHAM_ASSERT(match_hist_index == 3);

                  int dist = m_match_hist[3];
                  m_match_hist[3] = m_match_hist[2];
                  m_match_hist[2] = m_match_hist[1];
                  m_match_hist[1] = m_match_hist[0];
                  m_match_hist[0] = dist;
               }

               m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
            }
         }
         else
         {
            // full
            LZHAM_ASSUME(CLZBase::cMatchHistSize == 4);
            m_match_hist[3] = m_match_hist[2];
            m_match_hist[2] = m_match_hist[1];
            m_match_hist[1] = m_match_hist[0];
            m_match_hist[0] = lzdec.m_dist;

            m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? CLZBase::cNumLitStates : CLZBase::cNumLitStates + 3;
         }
      }

      m_cur_ofs = lzdec.m_pos + lzdec.get_len();
   }

   uint lzcompressor::state::get_pred_char(const search_accelerator& dict, int pos, int backward_ofs) const
   {
      LZHAM_ASSERT(pos >= (int)m_block_start_dict_ofs);
      int limit = pos - m_block_start_dict_ofs;
      if (backward_ofs > limit)
         return 0;
      return dict[pos - backward_ofs];
   }

   bit_cost_t lzcompressor::state::get_cost(CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec) const
   {
      const uint lit_pred0 = get_pred_char(dict, lzdec.m_pos, 1);

      uint is_match_model_index = LZHAM_IS_MATCH_MODEL_INDEX(lit_pred0, m_cur_state);
      LZHAM_ASSERT(is_match_model_index < LZHAM_ARRAY_SIZE(m_is_match_model));
      bit_cost_t cost = m_is_match_model[is_match_model_index].get_cost(lzdec.is_match());

      if (!lzdec.is_match())
      {
         const uint lit = dict[lzdec.m_pos];

         if (m_cur_state < CLZBase::cNumLitStates)
         {
            const uint lit_pred1 = get_pred_char(dict, lzdec.m_pos, 2);

            uint lit_pred = (lit_pred0 >> (8 - CLZBase::cNumLitPredBits/2)) |
               (((lit_pred1 >> (8 - CLZBase::cNumLitPredBits/2)) << CLZBase::cNumLitPredBits/2));

            // literal
            cost += m_lit_table[lit_pred].get_cost(lit);
         }
         else
         {
            // delta literal
            const uint rep_lit0 = dict[(lzdec.m_pos - m_match_hist[0]) & dict.m_max_dict_size_mask];
            const uint rep_lit1 = dict[(lzdec.m_pos - m_match_hist[0] - 1) & dict.m_max_dict_size_mask];

            uint delta_lit = rep_lit0 ^ lit;

            uint lit_pred = (rep_lit0 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) |
               ((rep_lit1 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) << CLZBase::cNumDeltaLitPredBits/2);

            cost += m_delta_lit_table[lit_pred].get_cost(delta_lit);
         }
      }
      else
      {
         // match
         if (lzdec.m_dist < 0)
         {
            // rep match
            cost += m_is_rep_model[m_cur_state].get_cost(1);

            int match_hist_index = -lzdec.m_dist - 1;

            if (!match_hist_index)
            {
               // rep0 match
               cost += m_is_rep0_model[m_cur_state].get_cost(1);

               if (lzdec.m_len == 1)
               {
                  // single byte rep0
                  cost += m_is_rep0_single_byte_model[m_cur_state].get_cost(1);
               }
               else
               {
                  // normal rep0
                  cost += m_is_rep0_single_byte_model[m_cur_state].get_cost(0);

                  if (lzdec.m_len > CLZBase::cMaxMatchLen)
                  {
                     cost += get_huge_match_code_len(lzdec.m_len) + m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost((CLZBase::cMaxMatchLen + 1) - CLZBase::cMinMatchLen);
                  }
                  else
                  {
                     cost += m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost(lzdec.m_len - CLZBase::cMinMatchLen);
                  }
               }
            }
            else
            {
               if (lzdec.m_len > CLZBase::cMaxMatchLen)
               {
                  cost += get_huge_match_code_len(lzdec.m_len) + m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost((CLZBase::cMaxMatchLen + 1) - CLZBase::cMinMatchLen);
               }
               else
               {
                  cost += m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost(lzdec.m_len - CLZBase::cMinMatchLen);
               }

               // rep1-rep3 match
               cost += m_is_rep0_model[m_cur_state].get_cost(0);

               if (match_hist_index == 1)
               {
                  // rep1
                  cost += m_is_rep1_model[m_cur_state].get_cost(1);
               }
               else
               {
                  cost += m_is_rep1_model[m_cur_state].get_cost(0);

                  if (match_hist_index == 2)
                  {
                     // rep2
                     cost += m_is_rep2_model[m_cur_state].get_cost(1);
                  }
                  else
                  {
                     LZHAM_ASSERT(match_hist_index == 3);
                     // rep3
                     cost += m_is_rep2_model[m_cur_state].get_cost(0);
                  }
               }
            }
         }
         else
         {
            cost += m_is_rep_model[m_cur_state].get_cost(0);

            LZHAM_ASSERT(lzdec.m_len >= CLZBase::cMinMatchLen);

            // full match
            uint match_slot, match_extra;
            lzbase.compute_lzx_position_slot(lzdec.m_dist, match_slot, match_extra);

            uint match_low_sym = 0;
            if (lzdec.m_len >= 9)
            {
               match_low_sym = 7;
               if (lzdec.m_len > CLZBase::cMaxMatchLen)
               {
                  cost += get_huge_match_code_len(lzdec.m_len) + m_large_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost((CLZBase::cMaxMatchLen + 1) - 9);
               }
               else
               {
                  cost += m_large_len_table[m_cur_state >= CLZBase::cNumLitStates].get_cost(lzdec.m_len - 9);
               }
            }
            else
               match_low_sym = lzdec.m_len - 2;

            uint match_high_sym = 0;

            LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
            match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

            uint main_sym = match_low_sym | (match_high_sym << 3);

            cost += m_main_table.get_cost(CLZBase::cLZXNumSpecialLengths + main_sym);

            uint num_extra_bits = lzbase.m_lzx_position_extra_bits[match_slot];
            if (num_extra_bits < 3)
               cost += convert_to_scaled_bitcost(num_extra_bits);
            else
            {
               if (num_extra_bits > 4)
                  cost += convert_to_scaled_bitcost(num_extra_bits - 4);

               cost += m_dist_lsb_table.get_cost(match_extra & 15);
            }
         }
      }

      return cost;
   }

   bit_cost_t lzcompressor::state::get_len2_match_cost(CLZBase& lzbase, uint dict_pos, uint len2_match_dist, uint is_match_model_index)
   {
      LZHAM_NOTE_UNUSED(dict_pos);

      bit_cost_t cost = m_is_match_model[is_match_model_index].get_cost(1);

      cost += m_is_rep_model[m_cur_state].get_cost(0);

      // full match
      uint match_slot, match_extra;
      lzbase.compute_lzx_position_slot(len2_match_dist, match_slot, match_extra);

      const uint match_len = 2;
      uint match_low_sym = match_len - 2;

      uint match_high_sym = 0;

      LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
      match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

      uint main_sym = match_low_sym | (match_high_sym << 3);

      cost += m_main_table.get_cost(CLZBase::cLZXNumSpecialLengths + main_sym);

      uint num_extra_bits = lzbase.m_lzx_position_extra_bits[match_slot];
      if (num_extra_bits < 3)
         cost += convert_to_scaled_bitcost(num_extra_bits);
      else
      {
         if (num_extra_bits > 4)
            cost += convert_to_scaled_bitcost(num_extra_bits - 4);

         cost += m_dist_lsb_table.get_cost(match_extra & 15);
      }

      return cost;
   }

   bit_cost_t lzcompressor::state::get_lit_cost(const search_accelerator& dict, uint dict_pos, uint lit_pred0, uint is_match_model_index) const
   {
      bit_cost_t cost = m_is_match_model[is_match_model_index].get_cost(0);

      const uint lit = dict[dict_pos];

      if (m_cur_state < CLZBase::cNumLitStates)
      {
         // literal
         const uint lit_pred1 = get_pred_char(dict, dict_pos, 2);

         uint lit_pred = (lit_pred0 >> (8 - CLZBase::cNumLitPredBits/2)) |
            (((lit_pred1 >> (8 - CLZBase::cNumLitPredBits/2)) << CLZBase::cNumLitPredBits/2));

         cost += m_lit_table[lit_pred].get_cost(lit);
      }
      else
      {
         // delta literal
         const uint rep_lit0 = dict[(dict_pos - m_match_hist[0]) & dict.m_max_dict_size_mask];
         const uint rep_lit1 = dict[(dict_pos - m_match_hist[0] - 1) & dict.m_max_dict_size_mask];

         uint delta_lit = rep_lit0 ^ lit;

         uint lit_pred = (rep_lit0 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) |
            ((rep_lit1 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) << CLZBase::cNumDeltaLitPredBits/2);

         cost += m_delta_lit_table[lit_pred].get_cost(delta_lit);
      }

      return cost;
   }

   void lzcompressor::state::get_rep_match_costs(uint dict_pos, bit_cost_t *pBitcosts, uint match_hist_index, int min_len, int max_len, uint is_match_model_index) const
   {
      LZHAM_NOTE_UNUSED(dict_pos);
      // match
      const sym_data_model &rep_len_table = m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates];

      bit_cost_t base_cost = m_is_match_model[is_match_model_index].get_cost(1);

      base_cost += m_is_rep_model[m_cur_state].get_cost(1);

      if (!match_hist_index)
      {
         // rep0 match
         base_cost += m_is_rep0_model[m_cur_state].get_cost(1);
      }
      else
      {
         // rep1-rep3 matches
         base_cost += m_is_rep0_model[m_cur_state].get_cost(0);

         if (match_hist_index == 1)
         {
            // rep1
            base_cost += m_is_rep1_model[m_cur_state].get_cost(1);
         }
         else
         {
            base_cost += m_is_rep1_model[m_cur_state].get_cost(0);

            if (match_hist_index == 2)
            {
               // rep2
               base_cost += m_is_rep2_model[m_cur_state].get_cost(1);
            }
            else
            {
               // rep3
               base_cost += m_is_rep2_model[m_cur_state].get_cost(0);
            }
         }
      }

      // rep match
      if (!match_hist_index)
      {
         if (min_len == 1)
         {
            // single byte rep0
            pBitcosts[1] = base_cost + m_is_rep0_single_byte_model[m_cur_state].get_cost(1);
            min_len++;
         }

         bit_cost_t rep0_match_base_cost = base_cost + m_is_rep0_single_byte_model[m_cur_state].get_cost(0);
         for (int match_len = min_len; match_len <= max_len; match_len++)
         {
            // normal rep0
            if (match_len > CLZBase::cMaxMatchLen)
            {
               pBitcosts[match_len] = get_huge_match_code_len(match_len) + rep0_match_base_cost + rep_len_table.get_cost((CLZBase::cMaxMatchLen + 1) - CLZBase::cMinMatchLen);
            }
            else
            {
               pBitcosts[match_len] = rep0_match_base_cost + rep_len_table.get_cost(match_len - CLZBase::cMinMatchLen);
            }
         }
      }
      else
      {
         for (int match_len = min_len; match_len <= max_len; match_len++)
         {
            if (match_len > CLZBase::cMaxMatchLen)
            {
               pBitcosts[match_len] = get_huge_match_code_len(match_len) + base_cost + rep_len_table.get_cost((CLZBase::cMaxMatchLen + 1) - CLZBase::cMinMatchLen);
            }
            else
            {
               pBitcosts[match_len] = base_cost + rep_len_table.get_cost(match_len - CLZBase::cMinMatchLen);
            }
         }
      }
   }

   void lzcompressor::state::get_full_match_costs(CLZBase& lzbase, uint dict_pos, bit_cost_t *pBitcosts, uint match_dist, int min_len, int max_len, uint is_match_model_index) const
   {
      LZHAM_NOTE_UNUSED(dict_pos);
      LZHAM_ASSERT(min_len >= CLZBase::cMinMatchLen);

      bit_cost_t cost = m_is_match_model[is_match_model_index].get_cost(1);

      cost += m_is_rep_model[m_cur_state].get_cost(0);

      uint match_slot, match_extra;
      lzbase.compute_lzx_position_slot(match_dist, match_slot, match_extra);
      LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));

      uint num_extra_bits = lzbase.m_lzx_position_extra_bits[match_slot];

      if (num_extra_bits < 3)
         cost += convert_to_scaled_bitcost(num_extra_bits);
      else
      {
         if (num_extra_bits > 4)
            cost += convert_to_scaled_bitcost(num_extra_bits - 4);

         cost += m_dist_lsb_table.get_cost(match_extra & 15);
      }

      uint match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

      const sym_data_model &large_len_table = m_large_len_table[m_cur_state >= CLZBase::cNumLitStates];

      for (int match_len = min_len; match_len <= max_len; match_len++)
      {
         bit_cost_t len_cost = cost;

         uint match_low_sym = 0;
         if (match_len >= 9)
         {
            match_low_sym = 7;
            if (match_len > CLZBase::cMaxMatchLen)
            {
               len_cost += get_huge_match_code_len(match_len) + large_len_table.get_cost((CLZBase::cMaxMatchLen + 1) - 9);
            }
            else
            {
               len_cost += large_len_table.get_cost(match_len - 9);
            }
         }
         else
            match_low_sym = match_len - 2;

         uint main_sym = match_low_sym | (match_high_sym << 3);

         pBitcosts[match_len] = len_cost + m_main_table.get_cost(CLZBase::cLZXNumSpecialLengths + main_sym);
      }
   }

   bool lzcompressor::state::advance(CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec)
   {
      const uint lit_pred0 = get_pred_char(dict, lzdec.m_pos, 1);

      uint is_match_model_index = LZHAM_IS_MATCH_MODEL_INDEX(lit_pred0, m_cur_state);
      m_is_match_model[is_match_model_index].update(lzdec.is_match());

      if (!lzdec.is_match())
      {
         const uint lit = dict[lzdec.m_pos];

         if (m_cur_state < CLZBase::cNumLitStates)
         {
            const uint lit_pred1 = get_pred_char(dict, lzdec.m_pos, 2);

            uint lit_pred = (lit_pred0 >> (8 - CLZBase::cNumLitPredBits/2)) |
               (((lit_pred1 >> (8 - CLZBase::cNumLitPredBits/2)) << CLZBase::cNumLitPredBits/2));

            // literal
            if (!m_lit_table[lit_pred].update(lit)) return false;
         }
         else
         {
            // delta literal
            const uint rep_lit0 = dict[(lzdec.m_pos - m_match_hist[0]) & dict.m_max_dict_size_mask];
            const uint rep_lit1 = dict[(lzdec.m_pos - m_match_hist[0] - 1) & dict.m_max_dict_size_mask];

            uint delta_lit = rep_lit0 ^ lit;

            uint lit_pred = (rep_lit0 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) |
               ((rep_lit1 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) << CLZBase::cNumDeltaLitPredBits/2);

            if (!m_delta_lit_table[lit_pred].update(delta_lit)) return false;
         }

         if (m_cur_state < 4) m_cur_state = 0; else if (m_cur_state < 10) m_cur_state -= 3; else m_cur_state -= 6;
      }
      else
      {
         // match
         if (lzdec.m_dist < 0)
         {
            // rep match
            m_is_rep_model[m_cur_state].update(1);

            int match_hist_index = -lzdec.m_dist - 1;

            if (!match_hist_index)
            {
               // rep0 match
               m_is_rep0_model[m_cur_state].update(1);

               if (lzdec.m_len == 1)
               {
                  // single byte rep0
                  m_is_rep0_single_byte_model[m_cur_state].update(1);

                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 9 : 11;
               }
               else
               {
                  // normal rep0
                  m_is_rep0_single_byte_model[m_cur_state].update(0);

                  if (lzdec.m_len > CLZBase::cMaxMatchLen)
                  {
                     if (!m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].update((CLZBase::cMaxMatchLen + 1) - CLZBase::cMinMatchLen)) return false;
                  }
                  else
                  {
                     if (!m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].update(lzdec.m_len - CLZBase::cMinMatchLen)) return false;
                  }

                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
               }
            }
            else
            {
               // rep1-rep3 match
               m_is_rep0_model[m_cur_state].update(0);

               if (lzdec.m_len > CLZBase::cMaxMatchLen)
               {
                  if (!m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].update((CLZBase::cMaxMatchLen + 1) - CLZBase::cMinMatchLen)) return false;
               }
               else
               {
                  if (!m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates].update(lzdec.m_len - CLZBase::cMinMatchLen)) return false;
               }

               if (match_hist_index == 1)
               {
                  // rep1
                  m_is_rep1_model[m_cur_state].update(1);

                  std::swap(m_match_hist[0], m_match_hist[1]);
               }
               else
               {
                  m_is_rep1_model[m_cur_state].update(0);

                  if (match_hist_index == 2)
                  {
                     // rep2
                     m_is_rep2_model[m_cur_state].update(1);

                     int dist = m_match_hist[2];
                     m_match_hist[2] = m_match_hist[1];
                     m_match_hist[1] = m_match_hist[0];
                     m_match_hist[0] = dist;
                  }
                  else
                  {
                     // rep3
                     m_is_rep2_model[m_cur_state].update(0);

                     int dist = m_match_hist[3];
                     m_match_hist[3] = m_match_hist[2];
                     m_match_hist[2] = m_match_hist[1];
                     m_match_hist[1] = m_match_hist[0];
                     m_match_hist[0] = dist;
                  }
               }

               m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
            }
         }
         else
         {
            m_is_rep_model[m_cur_state].update(0);

            LZHAM_ASSERT(lzdec.m_len >= CLZBase::cMinMatchLen);

            // full match
            uint match_slot, match_extra;
            lzbase.compute_lzx_position_slot(lzdec.m_dist, match_slot, match_extra);

            uint match_low_sym = 0;
            int large_len_sym = -1;
            if (lzdec.m_len >= 9)
            {
               match_low_sym = 7;

               large_len_sym = lzdec.m_len - 9;
            }
            else
               match_low_sym = lzdec.m_len - 2;

            uint match_high_sym = 0;

            LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
            match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

            uint main_sym = match_low_sym | (match_high_sym << 3);

            if (!m_main_table.update(CLZBase::cLZXNumSpecialLengths + main_sym)) return false;

            if (large_len_sym >= 0)
            {
               if (lzdec.m_len > CLZBase::cMaxMatchLen)
               {
                  if (!m_large_len_table[m_cur_state >= CLZBase::cNumLitStates].update((CLZBase::cMaxMatchLen + 1) - 9)) return false;
               }
               else
               {
                  if (!m_large_len_table[m_cur_state >= CLZBase::cNumLitStates].update(large_len_sym)) return false;
               }
            }

            uint num_extra_bits = lzbase.m_lzx_position_extra_bits[match_slot];
            if (num_extra_bits >= 3)
            {
               if (!m_dist_lsb_table.update(match_extra & 15)) return false;
            }

            update_match_hist(lzdec.m_dist);

            m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? CLZBase::cNumLitStates : CLZBase::cNumLitStates + 3;
         }
      }

      m_cur_ofs = lzdec.m_pos + lzdec.get_len();
      return true;
   }

   bool lzcompressor::state::encode(symbol_codec& codec, CLZBase& lzbase, const search_accelerator& dict, const lzdecision& lzdec)
   {
      const uint lit_pred0 = get_pred_char(dict, lzdec.m_pos, 1);

      uint is_match_model_index = LZHAM_IS_MATCH_MODEL_INDEX(lit_pred0, m_cur_state);
      if (!codec.encode(lzdec.is_match(), m_is_match_model[is_match_model_index])) return false;

      if (!lzdec.is_match())
      {
         const uint lit = dict[lzdec.m_pos];

         if (m_cur_state < CLZBase::cNumLitStates)
         {
            const uint lit_pred1 = get_pred_char(dict, lzdec.m_pos, 2);

            uint lit_pred = (lit_pred0 >> (8 - CLZBase::cNumLitPredBits/2)) |
               (((lit_pred1 >> (8 - CLZBase::cNumLitPredBits/2)) << CLZBase::cNumLitPredBits/2));

            // literal
            if (!codec.encode(lit, m_lit_table[lit_pred])) return false;
         }
         else
         {
            // delta literal
            const uint rep_lit0 = dict[(lzdec.m_pos - m_match_hist[0]) & dict.m_max_dict_size_mask];
            const uint rep_lit1 = dict[(lzdec.m_pos - m_match_hist[0] - 1) & dict.m_max_dict_size_mask];

            uint delta_lit = rep_lit0 ^ lit;

            uint lit_pred = (rep_lit0 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) |
               ((rep_lit1 >> (8 - CLZBase::cNumDeltaLitPredBits/2)) << CLZBase::cNumDeltaLitPredBits/2);

            if (!codec.encode(delta_lit, m_delta_lit_table[lit_pred])) return false;
         }

         if (m_cur_state < 4) m_cur_state = 0; else if (m_cur_state < 10) m_cur_state -= 3; else m_cur_state -= 6;
      }
      else
      {
         // match
         if (lzdec.m_dist < 0)
         {
            // rep match
            if (!codec.encode(1, m_is_rep_model[m_cur_state])) return false;

            int match_hist_index = -lzdec.m_dist - 1;

            if (!match_hist_index)
            {
               // rep0 match
               if (!codec.encode(1, m_is_rep0_model[m_cur_state])) return false;

               if (lzdec.m_len == 1)
               {
                  // single byte rep0
                  if (!codec.encode(1, m_is_rep0_single_byte_model[m_cur_state])) return false;

                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 9 : 11;
               }
               else
               {
                  // normal rep0
                  if (!codec.encode(0, m_is_rep0_single_byte_model[m_cur_state])) return false;

                  if (lzdec.m_len > CLZBase::cMaxMatchLen)
                  {
                     if (!codec.encode((CLZBase::cMaxMatchLen + 1) - CLZBase::cMinMatchLen, m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
                     if (!codec.encode_bits(get_huge_match_code_bits(lzdec.m_len), get_huge_match_code_len(lzdec.m_len))) return false;
                  }
                  else
                  {
                     if (!codec.encode(lzdec.m_len - CLZBase::cMinMatchLen, m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
                  }

                  m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
               }
            }
            else
            {
               // rep1-rep3 match
               if (!codec.encode(0, m_is_rep0_model[m_cur_state])) return false;

               if (lzdec.m_len > CLZBase::cMaxMatchLen)
               {
                  if (!codec.encode((CLZBase::cMaxMatchLen + 1) - CLZBase::cMinMatchLen, m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
                  if (!codec.encode_bits(get_huge_match_code_bits(lzdec.m_len), get_huge_match_code_len(lzdec.m_len))) return false;
               }
               else
               {
                  if (!codec.encode(lzdec.m_len - CLZBase::cMinMatchLen, m_rep_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
               }

               if (match_hist_index == 1)
               {
                  // rep1
                  if (!codec.encode(1, m_is_rep1_model[m_cur_state])) return false;

                  std::swap(m_match_hist[0], m_match_hist[1]);
               }
               else
               {
                  if (!codec.encode(0, m_is_rep1_model[m_cur_state])) return false;

                  if (match_hist_index == 2)
                  {
                     // rep2
                     if (!codec.encode(1, m_is_rep2_model[m_cur_state])) return false;

                     int dist = m_match_hist[2];
                     m_match_hist[2] = m_match_hist[1];
                     m_match_hist[1] = m_match_hist[0];
                     m_match_hist[0] = dist;
                  }
                  else
                  {
                     // rep3
                     if (!codec.encode(0, m_is_rep2_model[m_cur_state])) return false;

                     int dist = m_match_hist[3];
                     m_match_hist[3] = m_match_hist[2];
                     m_match_hist[2] = m_match_hist[1];
                     m_match_hist[1] = m_match_hist[0];
                     m_match_hist[0] = dist;
                  }
               }

               m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? 8 : 11;
            }
         }
         else
         {
            if (!codec.encode(0, m_is_rep_model[m_cur_state])) return false;

            LZHAM_ASSERT(lzdec.m_len >= CLZBase::cMinMatchLen);

            // full match
            uint match_slot, match_extra;
            lzbase.compute_lzx_position_slot(lzdec.m_dist, match_slot, match_extra);

            uint match_low_sym = 0;
            int large_len_sym = -1;
            if (lzdec.m_len >= 9)
            {
               match_low_sym = 7;

               large_len_sym = lzdec.m_len - 9;
            }
            else
               match_low_sym = lzdec.m_len - 2;

            uint match_high_sym = 0;

            LZHAM_ASSERT(match_slot >= CLZBase::cLZXLowestUsableMatchSlot && (match_slot < lzbase.m_num_lzx_slots));
            match_high_sym = match_slot - CLZBase::cLZXLowestUsableMatchSlot;

            uint main_sym = match_low_sym | (match_high_sym << 3);

            if (!codec.encode(CLZBase::cLZXNumSpecialLengths + main_sym, m_main_table)) return false;

            if (large_len_sym >= 0)
            {
               if (lzdec.m_len > CLZBase::cMaxMatchLen)
               {
                  if (!codec.encode((CLZBase::cMaxMatchLen + 1) - 9, m_large_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
                  if (!codec.encode_bits(get_huge_match_code_bits(lzdec.m_len), get_huge_match_code_len(lzdec.m_len))) return false;
               }
               else
               {
                  if (!codec.encode(large_len_sym, m_large_len_table[m_cur_state >= CLZBase::cNumLitStates])) return false;
               }
            }

            uint num_extra_bits = lzbase.m_lzx_position_extra_bits[match_slot];
            if (num_extra_bits < 3)
            {
               if (!codec.encode_bits(match_extra, num_extra_bits)) return false;
            }
            else
            {
               if (num_extra_bits > 4)
               {
                  if (!codec.encode_bits((match_extra >> 4), num_extra_bits - 4)) return false;
               }

               if (!codec.encode(match_extra & 15, m_dist_lsb_table)) return false;
            }

            update_match_hist(lzdec.m_dist);

            m_cur_state = (m_cur_state < CLZBase::cNumLitStates) ? CLZBase::cNumLitStates : CLZBase::cNumLitStates + 3;
         }
      }

      m_cur_ofs = lzdec.m_pos + lzdec.get_len();
      return true;
   }

   bool lzcompressor::state::encode_eob(symbol_codec& codec, const search_accelerator& dict, uint dict_pos)
   {
      const uint match_pred = get_pred_char(dict, dict_pos, 1);
      uint is_match_model_index = LZHAM_IS_MATCH_MODEL_INDEX(match_pred, m_cur_state);
      if (!codec.encode(1, m_is_match_model[is_match_model_index])) return false;

      // full match
      if (!codec.encode(0, m_is_rep_model[m_cur_state])) return false;

      return codec.encode(CLZBase::cLZXSpecialCodeEndOfBlockCode, m_main_table);
   }

   bool lzcompressor::state::encode_reset_state_partial(symbol_codec& codec, const search_accelerator& dict, uint dict_pos)
   {
      const uint match_pred = get_pred_char(dict, dict_pos, 1);
      uint is_match_model_index = LZHAM_IS_MATCH_MODEL_INDEX(match_pred, m_cur_state);
      if (!codec.encode(1, m_is_match_model[is_match_model_index])) return false;

      // full match
      if (!codec.encode(0, m_is_rep_model[m_cur_state])) return false;

      if (!codec.encode(CLZBase::cLZXSpecialCodePartialStateReset, m_main_table))
         return false;

      reset_state_partial();
      return true;
   }

   void lzcompressor::state::update_match_hist(uint match_dist)
   {
      LZHAM_ASSUME(CLZBase::cMatchHistSize == 4);
      m_match_hist[3] = m_match_hist[2];
      m_match_hist[2] = m_match_hist[1];
      m_match_hist[1] = m_match_hist[0];
      m_match_hist[0] = match_dist;
   }

   int lzcompressor::state::find_match_dist(uint match_dist) const
   {
      for (uint match_hist_index = 0; match_hist_index < CLZBase::cMatchHistSize; match_hist_index++)
         if (match_dist == m_match_hist[match_hist_index])
            return match_hist_index;

      return -1;
   }

   void lzcompressor::state::reset_state_partial()
   {
      LZHAM_ASSUME(CLZBase::cMatchHistSize == 4);
      m_match_hist[0] = 1;
      m_match_hist[1] = 1;
      m_match_hist[2] = 1;
      m_match_hist[3] = 1;
      m_cur_state = 0;
   }

   void lzcompressor::state::start_of_block(const search_accelerator& dict, uint cur_ofs, uint block_index)
   {
      LZHAM_NOTE_UNUSED(dict), LZHAM_NOTE_UNUSED(block_index);

      reset_state_partial();

      m_cur_ofs = cur_ofs;
      m_block_start_dict_ofs = cur_ofs;
   }

   void lzcompressor::state::reset_update_rate()
   {
      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_lit_table); i++)
         m_lit_table[i].reset_update_rate();

      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_delta_lit_table); i++)
         m_delta_lit_table[i].reset_update_rate();

      m_main_table.reset_update_rate();

      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_rep_len_table); i++)
         m_rep_len_table[i].reset_update_rate();

      for (uint i = 0; i < LZHAM_ARRAY_SIZE(m_large_len_table); i++)
         m_large_len_table[i].reset_update_rate();

      m_dist_lsb_table.reset_update_rate();
   }

   void lzcompressor::coding_stats::clear()
   {
      m_total_bytes = 0;
      m_total_contexts = 0;
      m_total_match_bits_cost = 0;
      m_worst_match_bits_cost = 0;
      m_total_is_match0_bits_cost = 0;
      m_total_is_match1_bits_cost = 0;
      m_context_stats.clear();

      m_total_nonmatches = 0;
      m_total_matches = 0;
      m_total_cost = 0.0f;

      m_lit_stats.clear();
      m_delta_lit_stats.clear();

      m_rep0_len1_stats.clear();
      for (uint i = 0; i < CLZBase::cMatchHistSize; i++)
         m_rep_stats[i].clear();
      m_rep0_len1_stats.clear();
      m_rep0_len2_plus_stats.clear();

      for (uint i = 0; i <= CLZBase::cMaxMatchLen; i++)
         m_full_match_stats[i].clear();

      m_total_far_len2_matches = 0;
      m_total_near_len2_matches = 0;

      m_total_truncated_matches = 0;
      utils::zero_object(m_match_truncation_len_hist);
      utils::zero_object(m_match_truncation_hist);
      utils::zero_object(m_match_type_truncation_hist);
      utils::zero_object(m_match_type_was_not_truncated_hist);

      m_total_update_rate_resets = 0;

      m_max_len2_dist = 0;
   }

   void lzcompressor::coding_stats::update(const lzdecision& lzdec, const state& cur_state, const search_accelerator& dict, bit_cost_t cost)
   {
      m_total_bytes += lzdec.get_len();
      m_total_contexts++;

      float cost_in_bits = cost / (float)cBitCostScale;
      LZHAM_ASSERT(cost_in_bits > 0.0f);
      m_total_cost += cost_in_bits;

      m_context_stats.update(cost_in_bits);

      uint match_pred = cur_state.get_pred_char(dict, lzdec.m_pos, 1);
      uint is_match_model_index = LZHAM_IS_MATCH_MODEL_INDEX(match_pred, cur_state.m_cur_state);

      if (lzdec.m_len == 0)
      {
         float match_bit_cost = cur_state.m_is_match_model[is_match_model_index].get_cost(0) / (float)cBitCostScale;

         m_total_is_match0_bits_cost += match_bit_cost;
         m_total_match_bits_cost += match_bit_cost;
         m_worst_match_bits_cost = math::maximum<double>(m_worst_match_bits_cost, static_cast<double>(match_bit_cost));
         m_total_nonmatches++;

         if (cur_state.m_cur_state < CLZBase::cNumLitStates)
         {
            m_lit_stats.update(cost_in_bits);
         }
         else
         {
            m_delta_lit_stats.update(cost_in_bits);
         }
      }
      else if (lzdec.m_len <= CLZBase::cMaxMatchLen)
      {
         const uint match_len = lzdec.get_len();

         {
            uint match_dist = lzdec.get_match_dist(cur_state);

            uint cur_lookahead_size = dict.get_lookahead_size();

            uint actual_match_len = dict.get_match_len(0, match_dist, LZHAM_MIN(cur_lookahead_size, static_cast<uint>(CLZBase::cMaxMatchLen)));
            LZHAM_VERIFY(match_len <= actual_match_len);

            m_total_truncated_matches += match_len < actual_match_len;
            m_match_truncation_len_hist[math::maximum<int>(0, actual_match_len - match_len)]++;

            uint type_index = 4;
            if (!lzdec.is_full_match())
            {
               LZHAM_ASSUME(CLZBase::cMatchHistSize == 4);
               type_index = -lzdec.m_dist - 1;
            }

            if (actual_match_len > match_len)
            {
               m_match_truncation_hist[match_len]++;

               m_match_type_truncation_hist[cur_state.m_cur_state][type_index]++;
            }
            else
            {
               m_match_type_was_not_truncated_hist[cur_state.m_cur_state][type_index]++;
            }
         }

         float match_bit_cost = cur_state.m_is_match_model[is_match_model_index].get_cost(1) / (float)cBitCostScale;
         m_total_is_match1_bits_cost += match_bit_cost;
         m_total_match_bits_cost += match_bit_cost;
         m_worst_match_bits_cost = math::maximum<double>(m_worst_match_bits_cost, static_cast<double>(match_bit_cost));
         m_total_matches++;

         if (lzdec.m_dist < 0)
         {
            // rep match
            int match_hist_index = -lzdec.m_dist - 1;
            LZHAM_ASSERT(match_hist_index < CLZBase::cMatchHistSize);

            m_rep_stats[match_hist_index].update(cost_in_bits);

            if (!match_hist_index)
            {
               // rep0 match
               if (lzdec.m_len == 1)
               {
                  m_rep0_len1_stats.update(cost_in_bits);
               }
               else
               {
                  m_rep0_len2_plus_stats.update(cost_in_bits);
               }
            }
         }
         else
         {
            m_full_match_stats[math::minimum<int>(cMaxMatchLen, match_len)].update(cost_in_bits);

            if (match_len == 2)
            {
               if (lzdec.m_dist <= 512)
                  m_total_near_len2_matches++;
               else
                  m_total_far_len2_matches++;

               m_max_len2_dist = LZHAM_MAX((int)m_max_len2_dist, lzdec.m_dist);
            }
         }
      }
      else
      {
         // TODO: Handle huge matches.
      }
   }
} // namespace lzham
