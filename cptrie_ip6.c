/*
 *   Copyright (c) 2019-2021 MD Iftakharul Islam (Tamim) <tamim@csebuet.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "cptrie_ip6.h"
#include <assert.h>

//Size of each level. They may need to be increased to accomodate larger routing table
#define SIZE16 1024
#define SIZE24 300
#define SIZE32 26000
#define SIZE40 34000
#define SIZE48 56000
#define SIZE56 4000
#define SIZE64 4000
#define SIZE72 200
#define SIZE80 200
#define SIZE88 200
#define SIZE96 200
#define SIZE104 200
#define SIZE112 200
#define SIZE120 200
#define SIZE128 200

#define N_CNT 9000000

//forward declation
static int _cptrie_insert(__uint128_t key, int prefix_len, int nexthop, int level);

struct cptrie cptrie;

int cptrie_init () {
  int err = 0;

  memset(&cptrie, 0, sizeof(cptrie));
  leaf_init (&cptrie.leaf, N_CNT);
  err = cptrie_level_init (&cptrie.level16, 16, SIZE16, NULL);
  err = cptrie_level_init (&cptrie.level24, 24, SIZE24, &cptrie.level16);
  err = cptrie_level_init (&cptrie.level32, 32, SIZE32, &cptrie.level24);
  err = cptrie_level_init (&cptrie.level40, 40, SIZE40, &cptrie.level32);
  err = cptrie_level_init (&cptrie.level48, 48, SIZE48, &cptrie.level40);
  err = cptrie_level_init (&cptrie.level56, 56, SIZE56, &cptrie.level48);
  err = cptrie_level_init (&cptrie.level64, 64, SIZE64, &cptrie.level56);
  err = cptrie_level_init (&cptrie.level72, 72, SIZE72, &cptrie.level64);
  err = cptrie_level_init (&cptrie.level80, 80, SIZE80, &cptrie.level72);
  err = cptrie_level_init (&cptrie.level88, 88, SIZE88, &cptrie.level80);
  err = cptrie_level_init (&cptrie.level96, 96, SIZE96, &cptrie.level88);
  err = cptrie_level_init (&cptrie.level104, 104, SIZE104, &cptrie.level96);
  err = cptrie_level_init (&cptrie.level112, 112, SIZE112, &cptrie.level104);
  err = cptrie_level_init (&cptrie.level120, 120, SIZE120, &cptrie.level112);
  err = cptrie_level_init (&cptrie.level128, 128, SIZE128, &cptrie.level120);
  //The count of level 16 is preset. It's not changed
  cptrie.level16.count = SIZE16/4;
}

int cptrie_cleanup() {
  leaf_cleanup(&cptrie.leaf);
  cptrie_level_cleanup(&cptrie.level16);
  cptrie_level_cleanup(&cptrie.level24);
  cptrie_level_cleanup(&cptrie.level32);
  cptrie_level_cleanup(&cptrie.level40);
  cptrie_level_cleanup(&cptrie.level48);
  cptrie_level_cleanup(&cptrie.level56);
  cptrie_level_cleanup(&cptrie.level64);
  cptrie_level_cleanup(&cptrie.level72);
  cptrie_level_cleanup(&cptrie.level80);
  cptrie_level_cleanup(&cptrie.level88);
  cptrie_level_cleanup(&cptrie.level96);
  cptrie_level_cleanup(&cptrie.level104);
  cptrie_level_cleanup(&cptrie.level112);
  cptrie_level_cleanup(&cptrie.level120);
  cptrie_level_cleanup(&cptrie.level128);
  memset(&cptrie, 0, sizeof(cptrie));
}

//Calculate memory in MB
double calc_cptrie_mem() {
  return (mem_size(&cptrie.level16) + mem_size(&cptrie.level24) + mem_size(&cptrie.level32) + mem_size(&cptrie.level40) +
        mem_size(&cptrie.level48) + mem_size(&cptrie.level56) + mem_size(&cptrie.level64) + mem_size(&cptrie.level72) +
        mem_size(&cptrie.level80) + mem_size(&cptrie.level88) + mem_size(&cptrie.level96) + mem_size(&cptrie.level104) +
        mem_size(&cptrie.level112) + mem_size(&cptrie.level120) + mem_size(&cptrie.level128) + mem_size(&cptrie.leaf))/ (1024*1024); 
}

//It calculate cumu_popcnt from previous chunk or the checks from upper level.
static uint32_t calc_cumu_popcnt(struct cptrie_level *l, uint32_t idx) 
{
  register long long i;
  struct cptrie_level *tmp_level;

  //find a valid entry to the left
  for (i = (long long)idx - 1; i >= 0; i--) {
    if (l->B[i].bitmap)
      return l->B[i].cumu_popcnt + POPCNT(l->B[i].bitmap);
  }

  //If there is no valid entry to the left, find the index from ancestor
  tmp_level = l->parent;
  while (tmp_level) {
    for (i = (long long)tmp_level->count * ELEMS_PER_STRIDE - 1; i >= 0; i--) {
      if (tmp_level->B[i].bitmap)
        return tmp_level->B[i].cumu_popcnt + POPCNT(tmp_level->B[i].bitmap);
    }
    tmp_level = tmp_level->parent;
  }

  //No chunk exist to the left or up
  return 0;
}

static uint32_t calc_n_idx(struct cptrie_level *l, uint32_t idx, uint32_t bit_spot)
{
  register long long i;
  struct cptrie_level *tmp_level;

  //If chunk is already populated, get the index based on this chunk
  if (l->B[idx].bitmap) {
      return l->B[idx].cumu_popcnt + POPCNT_LFT(l->B[idx].bitmap, bit_spot);
  }

  //Otherwise calculate the index based on chunks to the left
  for (i = (long long)idx - 1; i >= 0; i--) {
    if (l->B[i].bitmap)
      return l->B[i].cumu_popcnt + POPCNT(l->B[i].bitmap);
  }

  //Otherwise calculate the index based on chunks from the upper levels
  tmp_level = l->parent;
  while (tmp_level) {
    for (i = (long long)tmp_level->count * ELEMS_PER_STRIDE - 1; i >= 0; i--) {
      if (tmp_level->B[i].bitmap)
        return tmp_level->B[i].cumu_popcnt + POPCNT(tmp_level->B[i].bitmap);
    }
    tmp_level = tmp_level->parent;
  }

  //No chunk exist to the left or up
  return 0;
}

//There can be at most 256 leaves.
#define ARR_SIZE 256

static int insert_leaf(struct cptrie_level *l, uint32_t start_idx, uint32_t start_bit_spot, int level, struct leaf *leaf,
                __uint128_t key, int prefix_len, int nexthop) {
  register uint32_t new_prefixes = 0;
  register int i, j, k;
  register uint32_t bit_spot, idx;
  register uint32_t n_idx;
  register uint64_t tmp_bitmap = 0;
  register struct cptrie_level *tmp_level;
  __uint128_t matching_prefix1, matching_prefix2;
  //Note that, level >= prefix_len. We use level to calculate the number of
  // leaves. We don't use prefix_len for it. 
  register uint32_t num_leafs = 1U << (l->level_num - level);
  //prefixes which should be pushed higher
  __uint128_t leaf_pushing_prefixes[ARR_SIZE];
  register int leaf_pushing_prefixes_count = 0;
  //We don't insert one leaf at a time. It's too expensive. We rather store a map
  //containing index and the number of consecutive leaves. 
  //It must be set to 0. Otherwise it's initialized with garbage value
  struct uint32_Map leaf_idx[ARR_SIZE] = {0};
  register long long last_n_idx = -1;

  //TODO: increase the array dynamically when needed.
  if (leaf->count + num_leafs >= leaf->size) {
    puts ("Leaf array is full. Please increase the size.");
    exit (1);
  }

  for (i = 0; i < num_leafs; i++) {
    idx = start_idx + (start_bit_spot + i)/64;
    bit_spot = (start_bit_spot + i) % 64;
    //Longer prefix exist, so move the prefix to upper level
    if (l->C[idx].bitmap & (MSK >> bit_spot)) {
      //prefix to be pushed
      leaf_pushing_prefixes[leaf_pushing_prefixes_count++] = ((key >> (128 - l->level_num)) + i) << (128 - l->level_num);
    } else {
      n_idx = calc_n_idx(l, idx, bit_spot);
      /*A prefix already exists*/
      if (l->B[idx].bitmap & (MSK >> bit_spot)) {
        if (leaf->P[n_idx] <= prefix_len) {
          leaf->N[n_idx] = nexthop;
          leaf->P[n_idx] = prefix_len;
        }
      } else {
        //We don't insert one leaf at a time. It's too expensive. We rather
        //store the index and the number of consecutive leaves in a map for
        //the future. Later we insert them in a batch. Here we simply
        //populate the map.

        //This is the first index
        if (last_n_idx == -1) {
          k = 0;
          leaf_idx[k].start_idx = n_idx;
          leaf_idx[k].num_consecutive_leaves = 1;
          last_n_idx = n_idx;
        } else {
          //consecutive index
          if (last_n_idx == n_idx) {
            leaf_idx[k].num_consecutive_leaves++;
          } else {
            //New start index
            k++;
            leaf_idx[k].start_idx = n_idx;
            leaf_idx[k].num_consecutive_leaves = 1;
            last_n_idx = n_idx;
          }
        }
        
        tmp_bitmap |= (MSK >> bit_spot);
        new_prefixes++;
      }

      //This is the last bitmap of this chunk
      if (bit_spot == 63 || i == num_leafs - 1) {
        //Update bitmap and cumu_popcnt of the current chunk
        l->B[idx].bitmap |= tmp_bitmap;        
        l->B[idx].cumu_popcnt = calc_cumu_popcnt (l, idx);
        
        //Update cumu_popcnt of all the following chunks
        for (j = idx + 1; j < ELEMS_PER_STRIDE * l->count; j++)
          l->B[j].cumu_popcnt += new_prefixes;
        
        //Update cumu_popcnt of children
        tmp_level = l->chield;
        while (tmp_level) {
          for (j = 0; j < ELEMS_PER_STRIDE * tmp_level->count; j++) {
            if (tmp_level->B[j].bitmap) {
              tmp_level->B[j].cumu_popcnt += new_prefixes;
            }
          }
          tmp_level = tmp_level->chield;
        }
        
        //reset them for new iteration
        new_prefixes = 0;
        tmp_bitmap = 0;
      }
    }
  }

  //Insert the leaves in a batch
  leaf_insert (leaf, leaf_idx, ARR_SIZE, nexthop, prefix_len);

  //Leaf pushing
  if (l->chield) {
    for (i = 0; i < leaf_pushing_prefixes_count; i++) {
      matching_prefix1 = ((leaf_pushing_prefixes[i] >> (128 - l->chield->level_num)) + (0 << 7)) << (128 - l->chield->level_num);
      matching_prefix2 = ((leaf_pushing_prefixes[i] >> (128 - l->chield->level_num)) + (1 << 7)) << (128 - l->chield->level_num);
      _cptrie_insert (matching_prefix1, prefix_len , nexthop, l->level_num + 1);
      _cptrie_insert (matching_prefix2, prefix_len , nexthop, l->level_num + 1);
    }
  }
  return 0;
}

//Checks if there a leaf in the level; if yes, it then move the leafs to the next level
static int leaf_pushing(struct cptrie_level *l, uint32_t idx, uint32_t bit_spot, struct leaf *leafs,
                __uint128_t key) {
  register long long i;
  register uint32_t n_idx;
  register uint8_t next_hop, prefix_len;
  register struct cptrie_level *runner;
  register __uint128_t matching_key;

  //Matching leaf found, so need to push it to the next level
  if (l->B[idx].bitmap & (MSK >> bit_spot)) {
    n_idx = calc_n_idx(l, idx, bit_spot);
    next_hop = leafs->N[n_idx];
    prefix_len = leafs->P[n_idx];
    //shift each element one step left
    memmove(&leafs->N[n_idx], &leafs->N[n_idx + 1],
            (leafs->count - n_idx - 1) * sizeof(leafs->N[0]));
    memmove(&leafs->P[n_idx], &leafs->P[n_idx + 1],
            (leafs->count - n_idx - 1) * sizeof(leafs->P[0]));
    //reset the last element
    leafs->N[leafs->count - 1] = 0;
    leafs->P[leafs->count - 1] = 0;
    leafs->count--;

    //turn off the bit
    l->B[idx].bitmap &= ~(MSK >> bit_spot);

    //Update cumulative popcnt of following chunks
    for (i = (long long)idx + 1; i < ELEMS_PER_STRIDE * l->count; i++) {
      if (l->B[i].cumu_popcnt > 0)
        l->B[i].cumu_popcnt--;
    }

    //Update cumulative popcnt of children
    runner = l->chield;
    while (runner) {
      for (i = 0; i < ELEMS_PER_STRIDE * runner->count; i++) {
        if (runner->B[i].cumu_popcnt > 0)
          runner->B[i].cumu_popcnt--;
      }
      runner = runner->chield;
    }

    //Key to which the match was found and add 8 bits to the right
    matching_key = (key >> (128 - l->level_num)) << 8;
    //Previously inserted prefix is being pushed to a higher level.
    _cptrie_insert ((matching_key + (0 << 7)) << (120 - l->level_num), prefix_len, next_hop, l->level_num + 1);
    _cptrie_insert ((matching_key + (1 << 7)) << (120 - l->level_num), prefix_len, next_hop, l->level_num + 1);
  }
  return 0;
}

int cptrie_insert(__uint128_t key, int prefix_len, int nexthop) {
  //nexthop cannot be 0. We use 0 to indicate that next-hop doesn't exist.
  if (!nexthop) {
    puts ("nexthop cannot be 0. Please fix the routing table");
    exit (1);
  }
  //Level is same as prefix length
  return _cptrie_insert(key, prefix_len, nexthop, prefix_len);
}

//This function will be called by cptrie_insert() and by itself recursively for
//leaf pushing. When called by cptrie_insert(), level and prefix length should
//be same. When called recursively, level will be higher than the prefix length.
static int _cptrie_insert(__uint128_t key, int prefix_len, int nexthop, int level) {
  register uint32_t bit_spot;
  //Index to array at each level
  register uint32_t idx;
  register uint32_t stride;

  if (prefix_len == 0) {
    cptrie.def_nh = nexthop;
    goto finish;
  }

  /*Eextract 16 bits from MSB.*/
  stride = key >> 112;
  idx = stride / 64;
  bit_spot = stride % 64;
  if (level <= 16) {
    insert_leaf(&cptrie.level16, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level16, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 104) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level16, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 24) {
    insert_leaf(&cptrie.level24, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level24, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 96) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level24, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 32) {
    insert_leaf(&cptrie.level32, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level32, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 88) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level32, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 40) {
    insert_leaf(&cptrie.level40, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level40, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 80) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level40, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 48) {
    insert_leaf(&cptrie.level48, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level48, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 72) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level48, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 56) {
    insert_leaf(&cptrie.level56, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level56, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 64) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level56, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 64) {
    insert_leaf(&cptrie.level64, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level64, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 56) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level64, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 72) {
    insert_leaf(&cptrie.level72, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level72, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 48) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level72, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 80) {
    insert_leaf(&cptrie.level80, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level80, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 40) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level80, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 88) {
    insert_leaf(&cptrie.level88, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level88, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 32) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level88, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 96) {
    insert_leaf(&cptrie.level96, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level96, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 24) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level96, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 104) {
    insert_leaf(&cptrie.level104, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level104, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 16) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level104, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 112) {
    insert_leaf(&cptrie.level112, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level112, idx, bit_spot, &cptrie.leaf, key);

  stride = ((key >> 8) & 0XFF);
  idx = get_chunk_idx_frm_parent (&cptrie.level112, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 120) {
    insert_leaf(&cptrie.level120, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing (&cptrie.level120, idx, bit_spot, &cptrie.leaf, key);

  stride = key & 0XFF;
  idx = get_chunk_idx_frm_parent (&cptrie.level120, idx, bit_spot) * ELEMS_PER_STRIDE + stride / 64;
  bit_spot = stride % 64;
  if (level <= 128) {
    insert_leaf(&cptrie.level128, idx, bit_spot, level, &cptrie.leaf, key, prefix_len, nexthop);
    goto finish;
  }
error:
  puts("Something went wrong in route insertion");
  return -1;
finish:
//  leaf_print(&cptrie.leaf);
//  cptrie_level_print(&cptrie.level32);
  return 0;
}

/*Calculating index to the next level*/
#define IDX_NXT(C, IDX, BITSPOT , STRIDE) (((C[IDX].cumu_popcnt + \
            POPCNT_LFT(C[IDX].bitmap, BITSPOT)) * ELEMS_PER_STRIDE) + \
            STRIDE / 64)

#define N_IDX(B, IDX, BITSPOT) (B[IDX].cumu_popcnt + \
               POPCNT_LFT(B[IDX].bitmap, BITSPOT))

uint8_t cptrie_lookup(__uint128_t key) {
  //Making them register improves the lookup performance
  register uint32_t bit_spot;
  register uint32_t idx, stride;
  register uint64_t mask;
  register uint64_t n_idx = N_CNT;

  stride = key >> 112;
  //Changing arithmetic operators to bitwise operators doesn't increase
  //throughput. Probably compiler is changing it anyway. So we are using
  //arithmetic operators as it is
  idx = stride / 64;
  bit_spot = stride % 64;
  mask = MSK >> bit_spot;
  if (cptrie.level16.C[idx].bitmap & mask) {
    stride = (key >> 104) & 0XFF;
    idx = IDX_NXT (cptrie.level16.C, idx, bit_spot, stride);
    bit_spot = stride % 64;
    mask = MSK >> bit_spot;
    if (cptrie.level24.C[idx].bitmap & mask) {
      stride = (key >> 96) & 0XFF;
      idx = IDX_NXT (cptrie.level24.C, idx, bit_spot, stride);
      bit_spot = stride % 64;
      mask = MSK >> bit_spot;
      if (cptrie.level32.C[idx].bitmap & mask) {
        stride = (key >> 88) & 0XFF;
        idx = IDX_NXT (cptrie.level32.C, idx, bit_spot, stride);
        bit_spot = stride % 64;
        mask = MSK >> bit_spot;
        if (cptrie.level40.C[idx].bitmap & mask) {
          stride = (key >> 80) & 0XFF;
          idx = IDX_NXT (cptrie.level40.C, idx, bit_spot, stride);
          bit_spot = stride % 64;
          mask = MSK >> bit_spot;
          if (cptrie.level48.C[idx].bitmap & mask) {
            stride = (key >> 72) & 0XFF;
            idx = IDX_NXT (cptrie.level48.C, idx, bit_spot, stride);
            bit_spot = stride % 64;
            mask = MSK >> bit_spot;
            if (cptrie.level56.C[idx].bitmap & mask) {
              stride = (key >> 64) & 0XFF;
              idx = IDX_NXT (cptrie.level56.C, idx, bit_spot, stride);
              bit_spot = stride % 64;
              mask = MSK >> bit_spot;
              if (cptrie.level64.C[idx].bitmap & mask) {
                stride = (key >> 56) & 0XFF;
                idx = IDX_NXT (cptrie.level64.C, idx, bit_spot, stride);
                bit_spot = stride % 64;
                mask = MSK >> bit_spot;
                if (cptrie.level72.C[idx].bitmap & mask) {
                  stride = (key >> 48) & 0XFF;
                  idx = IDX_NXT (cptrie.level72.C, idx, bit_spot, stride);
                  bit_spot = stride % 64;
                  mask = MSK >> bit_spot;
                  if (cptrie.level80.C[idx].bitmap & mask) {
                    stride = (key >> 40) & 0XFF;
                    idx = IDX_NXT (cptrie.level80.C, idx, bit_spot, stride);
                    bit_spot = stride % 64;
                    mask = MSK >> bit_spot;
                    if (cptrie.level88.C[idx].bitmap & mask) {
                      stride = (key >> 32) & 0XFF;
                      idx = IDX_NXT (cptrie.level88.C, idx, bit_spot, stride);
                      bit_spot = stride % 64;
                      mask = MSK >> bit_spot;
                      if (cptrie.level96.C[idx].bitmap & mask) {
                        stride = (key >> 24) & 0XFF;
                        idx = IDX_NXT (cptrie.level96.C, idx, bit_spot, stride);
                        bit_spot = stride % 64;
                        mask = MSK >> bit_spot;
                        if (cptrie.level104.C[idx].bitmap & mask) {
                          stride = (key >> 16) & 0XFF;
                          idx = IDX_NXT (cptrie.level104.C, idx, bit_spot, stride);
                          bit_spot = stride % 64;
                          mask = MSK >> bit_spot;
                          if (cptrie.level112.C[idx].bitmap & mask) {
                            stride = (key >> 8) & 0XFF;
                            idx = IDX_NXT (cptrie.level112.C, idx, bit_spot, stride);
                            bit_spot = stride % 64;
                            mask = MSK >> bit_spot;
                            if (cptrie.level120.C[idx].bitmap & mask) {
                              stride = key & 0XFF;
                              idx = IDX_NXT (cptrie.level120.C, idx, bit_spot, stride);
                              bit_spot = stride % 64;
                              mask = MSK >> bit_spot;
                              if (cptrie.level128.B[idx].bitmap & mask)
                                n_idx = N_IDX(cptrie.level128.B, idx, bit_spot);
                            } else if (cptrie.level120.B[idx].bitmap & mask)
                              n_idx = N_IDX(cptrie.level120.B, idx, bit_spot);
                          } else if (cptrie.level112.B[idx].bitmap & mask)
                            n_idx = N_IDX(cptrie.level112.B, idx, bit_spot);
                        } else if (cptrie.level104.B[idx].bitmap & mask)
                          n_idx = N_IDX(cptrie.level104.B, idx, bit_spot);
                      } else if (cptrie.level96.B[idx].bitmap & mask)
                        n_idx = N_IDX(cptrie.level96.B, idx, bit_spot);
                    } else if (cptrie.level88.B[idx].bitmap & mask)
                      n_idx = N_IDX(cptrie.level88.B, idx, bit_spot);
                  } else if (cptrie.level80.B[idx].bitmap & mask)
                    n_idx = N_IDX(cptrie.level80.B, idx, bit_spot);
                } else if (cptrie.level72.B[idx].bitmap & mask)
                  n_idx = N_IDX(cptrie.level72.B, idx, bit_spot);
              } else if (cptrie.level64.B[idx].bitmap & mask)
                n_idx = N_IDX(cptrie.level64.B, idx, bit_spot);
            } else if (cptrie.level56.B[idx].bitmap & mask)
              n_idx = N_IDX(cptrie.level56.B, idx, bit_spot);
          } else if (cptrie.level48.B[idx].bitmap & mask)
            n_idx = N_IDX(cptrie.level48.B, idx, bit_spot);
        } else if (cptrie.level40.B[idx].bitmap & mask)
          n_idx = N_IDX(cptrie.level40.B, idx, bit_spot);
      } else if (cptrie.level32.B[idx].bitmap & mask)
        n_idx = N_IDX(cptrie.level32.B, idx, bit_spot);
    } else if (cptrie.level24.B[idx].bitmap & mask)
      n_idx = N_IDX(cptrie.level24.B, idx, bit_spot);
  } else if (cptrie.level16.B[idx].bitmap & mask)
    n_idx = N_IDX(cptrie.level16.B, idx, bit_spot);
  return n_idx == N_CNT ?  cptrie.def_nh : cptrie.leaf.N[n_idx];
}

//This is same as FIB lookup, except it returns matched prefix length instead
//of next-hop index
uint8_t cptrie_matched_prefix_len(__uint128_t key) {
  //Making them register improves the lookup performance
  register uint32_t bit_spot;
  register uint32_t idx, stride;
  register uint64_t mask;

  stride = key >> 112;
  //Changing arithmetic operators to bitwise operators doesn't increase
  //throughput. Probably compiler is changing it anyway. So we are using
  //arithmetic operators as it is
  idx = stride / 64;
  bit_spot = stride % 64;
  mask = MSK >> bit_spot;
  if (cptrie.level16.C[idx].bitmap & mask) {
    stride = (key >> 104) & 0XFF;
    idx = IDX_NXT (cptrie.level16.C, idx, bit_spot, stride);
    bit_spot = stride % 64;
    mask = MSK >> bit_spot;
    if (cptrie.level24.C[idx].bitmap & mask) {
      stride = (key >> 96) & 0XFF;
      idx = IDX_NXT (cptrie.level24.C, idx, bit_spot, stride);
      bit_spot = stride % 64;
      mask = MSK >> bit_spot;
      if (cptrie.level32.C[idx].bitmap & mask) {
        stride = (key >> 88) & 0XFF;
        idx = IDX_NXT (cptrie.level32.C, idx, bit_spot, stride);
        bit_spot = stride % 64;
        mask = MSK >> bit_spot;
        if (cptrie.level40.C[idx].bitmap & mask) {
          stride = (key >> 80) & 0XFF;
          idx = IDX_NXT (cptrie.level40.C, idx, bit_spot, stride);
          bit_spot = stride % 64;
          mask = MSK >> bit_spot;
          if (cptrie.level48.C[idx].bitmap & mask) {
            stride = (key >> 72) & 0XFF;
            idx = IDX_NXT (cptrie.level48.C, idx, bit_spot, stride);
            bit_spot = stride % 64;
            mask = MSK >> bit_spot;
            if (cptrie.level56.C[idx].bitmap & mask) {
              stride = (key >> 64) & 0XFF;
              idx = IDX_NXT (cptrie.level56.C, idx, bit_spot, stride);
              bit_spot = stride % 64;
              mask = MSK >> bit_spot;
              if (cptrie.level64.C[idx].bitmap & mask) {
                stride = (key >> 56) & 0XFF;
                idx = IDX_NXT (cptrie.level64.C, idx, bit_spot, stride);
                bit_spot = stride % 64;
                mask = MSK >> bit_spot;
                if (cptrie.level72.C[idx].bitmap & mask) {
                  stride = (key >> 48) & 0XFF;
                  idx = IDX_NXT (cptrie.level72.C, idx, bit_spot, stride);
                  bit_spot = stride % 64;
                  mask = MSK >> bit_spot;
                  if (cptrie.level80.C[idx].bitmap & mask) {
                    stride = (key >> 40) & 0XFF;
                    idx = IDX_NXT (cptrie.level80.C, idx, bit_spot, stride);
                    bit_spot = stride % 64;
                    mask = MSK >> bit_spot;
                    if (cptrie.level88.C[idx].bitmap & mask) {
                      stride = (key >> 32) & 0XFF;
                      idx = IDX_NXT (cptrie.level88.C, idx, bit_spot, stride);
                      bit_spot = stride % 64;
                      mask = MSK >> bit_spot;
                      if (cptrie.level96.C[idx].bitmap & mask) {
                        stride = (key >> 24) & 0XFF;
                        idx = IDX_NXT (cptrie.level96.C, idx, bit_spot, stride);
                        bit_spot = stride % 64;
                        mask = MSK >> bit_spot;
                        if (cptrie.level104.C[idx].bitmap & mask) {
                          stride = (key >> 16) & 0XFF;
                          idx = IDX_NXT (cptrie.level104.C, idx, bit_spot, stride);
                          bit_spot = stride % 64;
                          mask = MSK >> bit_spot;
                          if (cptrie.level112.C[idx].bitmap & mask) {
                            stride = (key >> 8) & 0XFF;
                            idx = IDX_NXT (cptrie.level112.C, idx, bit_spot, stride);
                            bit_spot = stride % 64;
                            mask = MSK >> bit_spot;
                            if (cptrie.level120.C[idx].bitmap & mask) {
                              stride = key & 0XFF;
                              idx = IDX_NXT (cptrie.level120.C, idx, bit_spot, stride);
                              bit_spot = stride % 64;
                              mask = MSK >> bit_spot;
                              if (cptrie.level128.B[idx].bitmap & mask) {
                                return 128;
                              }
                            } else if (cptrie.level120.B[idx].bitmap & mask) {
                              return 120;
                            }
                          } else if (cptrie.level112.B[idx].bitmap & mask) {
                            return 112;
                          }
                        } else if (cptrie.level104.B[idx].bitmap & mask) {
                          return 104;
                        }
                      } else if (cptrie.level96.B[idx].bitmap & mask) {
                        return 96;
                      }
                    } else if (cptrie.level88.B[idx].bitmap & mask) {
                      return 88;
                    }
                  } else if (cptrie.level80.B[idx].bitmap & mask) {
                    return 80;
                  }
                } else if (cptrie.level72.B[idx].bitmap & mask) {
                  return 72;
                }
              } else if (cptrie.level64.B[idx].bitmap & mask) {
                return 64;
              }
            } else if (cptrie.level56.B[idx].bitmap & mask) {
              return 56;
            }
          } else if (cptrie.level48.B[idx].bitmap & mask) {
            return 48;
          }
        } else if (cptrie.level40.B[idx].bitmap & mask) {
          return 40;
        }
      } else if (cptrie.level32.B[idx].bitmap & mask) {
        return 32;
      }
    } else if (cptrie.level24.B[idx].bitmap & mask) {
      return 24;
    }
  } else if (cptrie.level16.B[idx].bitmap & mask) {
    return 16;
  }

  return 16;
}
