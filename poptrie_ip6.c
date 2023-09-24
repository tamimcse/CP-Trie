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
#include "poptrie_ip6.h"
#include <assert.h>

//Size of each level. They may need to be increased to accomodate larger routing table
#define DIRSIZE 65536
#define SIZE16 400
#define SIZE22 3000
#define SIZE28 16000
#define SIZE34 14000
#define SIZE40 15000
#define SIZE46 28000
#define SIZE52 2000
#define SIZE58 2000
#define SIZE64 1000
#define SIZE70 100
#define SIZE76 100
#define SIZE82 100
#define SIZE88 100
#define SIZE94 100
#define SIZE100 100
#define SIZE106 100
#define SIZE112 100
#define SIZE118 100
#define SIZE124 100
#define N_SIZE 2500000

#define MSK 0X8000000000000000ULL

/*Calculates the number of bits set to 1*/
#define POPCNT(X) (__builtin_popcountll(X))

//Forward declaration
static int _poptrie_insert(__uint128_t key, int prefix_len, int nexthop, int level);

struct poptrie poptrie;

int poptrie_init () {
  int err = 0;

  memset(&poptrie, 0, sizeof(poptrie));
  err = leaf_init (&poptrie.leafs16, DIRSIZE);
  err = dir_init (&poptrie.dir16, DIRSIZE);
  err = leaf_init (&poptrie.leafs, N_SIZE);
  err = poptrie_level_init (&poptrie.L16, 16, SIZE16, NULL);
  err = poptrie_level_init (&poptrie.L22, 22, SIZE22, &poptrie.L16);
  err = poptrie_level_init (&poptrie.L28, 28, SIZE28, &poptrie.L22);
  err = poptrie_level_init (&poptrie.L34, 34, SIZE34, &poptrie.L28);
  err = poptrie_level_init (&poptrie.L40, 40, SIZE40, &poptrie.L34);
  err = poptrie_level_init (&poptrie.L46, 46, SIZE46, &poptrie.L40);
  err = poptrie_level_init (&poptrie.L52, 52, SIZE52, &poptrie.L46);
  err = poptrie_level_init (&poptrie.L58, 58, SIZE58, &poptrie.L52);
  err = poptrie_level_init (&poptrie.L64, 64, SIZE64, &poptrie.L58);
  err = poptrie_level_init (&poptrie.L70, 70, SIZE70, &poptrie.L64);
  err = poptrie_level_init (&poptrie.L76, 76, SIZE76, &poptrie.L70);
  err = poptrie_level_init (&poptrie.L82, 82, SIZE82, &poptrie.L76);
  err = poptrie_level_init (&poptrie.L88, 88, SIZE88, &poptrie.L82);
  err = poptrie_level_init (&poptrie.L94, 94, SIZE94, &poptrie.L88);
  err = poptrie_level_init (&poptrie.L100, 100, SIZE100, &poptrie.L94);
  err = poptrie_level_init (&poptrie.L106, 106, SIZE106, &poptrie.L100);
  err = poptrie_level_init (&poptrie.L112, 112, SIZE112, &poptrie.L106);
  err = poptrie_level_init (&poptrie.L118, 118, SIZE118, &poptrie.L112);
  err = poptrie_level_init (&poptrie.L124, 124, SIZE124, &poptrie.L118);

  poptrie.leafs16.count = DIRSIZE;

  if (err)
    return -1;   
  else
    return 1;
}

int poptrie_cleanup() {
  int err = 0;

  leaf_cleanup(&poptrie.leafs16);
  dir_cleanup(&poptrie.dir16);
  leaf_cleanup(&poptrie.leafs);
  poptrie_level_cleanup(&poptrie.L16);
  poptrie_level_cleanup(&poptrie.L22);
  poptrie_level_cleanup(&poptrie.L28);
  poptrie_level_cleanup(&poptrie.L34);
  poptrie_level_cleanup(&poptrie.L40);
  poptrie_level_cleanup(&poptrie.L46);
  poptrie_level_cleanup(&poptrie.L52);
  poptrie_level_cleanup(&poptrie.L58);
  poptrie_level_cleanup(&poptrie.L64);
  poptrie_level_cleanup(&poptrie.L70);
  poptrie_level_cleanup(&poptrie.L76);
  poptrie_level_cleanup(&poptrie.L82);
  poptrie_level_cleanup(&poptrie.L88);
  poptrie_level_cleanup(&poptrie.L94);
  poptrie_level_cleanup(&poptrie.L100);
  poptrie_level_cleanup(&poptrie.L106);
  poptrie_level_cleanup(&poptrie.L112);
  poptrie_level_cleanup(&poptrie.L118);
  poptrie_level_cleanup(&poptrie.L124);
  memset(&poptrie, 0, sizeof(poptrie));
  return 0;
}

//Calculate memory in MB
double calc_poptrie_mem() {
  return (mem_size (&poptrie.L16) + mem_size (&poptrie.L22) + mem_size (&poptrie.L28) + mem_size (&poptrie.L34) +
         mem_size (&poptrie.L40) + mem_size (&poptrie.L46) + mem_size (&poptrie.L52) + mem_size (&poptrie.L58) +
         mem_size (&poptrie.L64) + mem_size (&poptrie.L70) + mem_size (&poptrie.L76) + mem_size (&poptrie.L82) +
         mem_size (&poptrie.L88) + mem_size (&poptrie.L94) + mem_size (&poptrie.L100) + mem_size (&poptrie.L106) +
         mem_size (&poptrie.L112) + mem_size (&poptrie.L118) + mem_size (&poptrie.L124) + mem_size(&poptrie.leafs16) +
         mem_size(&poptrie.leafs) + mem_size(&poptrie.dir16)) / (1024 * 1024);
}

//Calculates base1 from the previous chunk or checks from the upper levels.
static uint32_t calc_base1(struct poptrie_level *l, uint32_t idx) 
{
  register long long i;
  struct poptrie_level *tmp_level;

  //Calculate base1 based on the chunks to the left
  for (i = (long long)idx - 1; i >= 0; i--) {
    if (l->B[i].leafvec)
      return l->B[i].base1 + POPCNT(l->B[i].leafvec);
  }

  //Otherwise calculate based on the chunks from upper levels
  tmp_level = l->parent;
  while (tmp_level) {
    for (i = (long long)tmp_level->count - 1; i >= 0; i--) {
      if (tmp_level->B[i].leafvec)
        return tmp_level->B[i].base1 + POPCNT(tmp_level->B[i].leafvec);
    }
    tmp_level = tmp_level->parent;
  }
  //No chunk to the left or up
  return 0;
}

static uint32_t calc_n_idx(struct poptrie_level *l, uint32_t idx, uint32_t stride)
{
  register long long i;
  struct poptrie_level *tmp_level;
  
  //If chunk is already populated, get the index based on this chunk
  if (l->B[idx].leafvec) {
    if (l->B[idx].leafvec & (1ULL << stride)) {
      return l->B[idx].base1 + POPCNT(l->B[idx].leafvec & ((2ULL << stride) - 1)) - 1;
    } else {
      return l->B[idx].base1 + POPCNT(l->B[idx].leafvec & ((2ULL << stride) - 1));
    }
  }
  
  //Otherwise calculate the index based on chunks to the left
  for (i = (long long)idx - 1; i >= 0; i--)
    if (l->B[i].leafvec)
      return l->B[i].base1 + POPCNT(l->B[i].leafvec);

  //Otherwise calculate the index based on chunks from the upper levels
  tmp_level = l->parent;
  while (tmp_level) {
    for (i = (long long)tmp_level->count - 1; i >= 0; i--) {
      if (tmp_level->B[i].leafvec)
        return tmp_level->B[i].base1 + POPCNT(tmp_level->B[i].leafvec);
    }
    tmp_level = tmp_level->parent;
  }
  
  //No chunk exist to the left or up
  return 0;
}

//There can be at most 64 leaves.
#define ARR_SIZE 64

static int insert_leaf(struct poptrie_level *l, int level, uint32_t idx, uint32_t stride,
                 __uint128_t key, int prefix_len,
                 int nexthop, struct leaf *leaf) {
  __uint128_t matching_prefix;
  register uint32_t new_prefixes = 0;
  register int i, j, k;
  register uint32_t bit_spot;
  register struct poptrie_level *tmp_level;
  register uint32_t n_idx;
  //level to which the leaf belong to currently.
  register int curr_level = l->level_num + 6;
  //level to which the leaf may will be pushed.
  register int dst_level = curr_level + 6;
  //Note that, level >= prefix_len. We use level to calculate the number of
  // leaves. We don't use prefix_len for it. 
  register uint32_t num_leafs = 1U << (curr_level - level);
  register uint64_t tmp_bitmap = 0;
  __uint128_t leaf_pushing_prefixes[ARR_SIZE];
  register int leaf_pushing_prefixes_count = 0;
  //We don't insert one leaf at a time. It's too expensive. We rather store a map
  //containing index and the number of consecutive leaves. 
  //It must be set to 0. Otherwise it's initialized with garbage value
  struct uint32_Map leaf_idx[ARR_SIZE] = {0};
  register long long last_n_idx = -1;

  if (leaf->count + num_leafs >= leaf->size) {
      puts ("Leaf array is full. Please increase the size.");
      exit (1);
  }

  for (i = 0; i < num_leafs; i++) {
    bit_spot = stride + i;
    //Longer prefix exist, so perform leaf pushing. Here we just store the
    //indices and perform leaf pushing later
    if (l->B[idx].vec & (1ULL << bit_spot)) {
      //Prefix that needs to be pushed to next level
      leaf_pushing_prefixes[leaf_pushing_prefixes_count++] = ((key >> (128 - curr_level)) + i) << (128 - curr_level);
    } else {//Leaf pushing is not needed
      n_idx = calc_n_idx(l, idx, bit_spot);
      if (l->B[idx].leafvec & (1ULL << bit_spot)) {
        /*Longer prefix exists*/
        if (leaf->P[n_idx] > prefix_len) {
          continue;
        }
        leaf->N[n_idx] = nexthop;
        leaf->P[n_idx] = prefix_len;
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
        //Create a tmp_bitmap to update leafvec
        tmp_bitmap |= (1ULL << bit_spot);
        new_prefixes++;
      }
    }
  }

  //Update the leafvec bitmap and base1 of the current chunk
  l->B[idx].leafvec |= tmp_bitmap;
  l->B[idx].base1 = calc_base1 (l, idx);

  //Update the base1 index of all the following chunks
  for (i = idx + 1; i < l->count; i++) {
    l->B[i].base1 += new_prefixes;
  }

  //Update base1 of children
  tmp_level = l->chield;
  while (tmp_level) {
    for (i = 0; i < tmp_level->count; i++) {
      if (tmp_level->B[i].leafvec) {
        tmp_level->B[i].base1 += new_prefixes;
      }
    }
    tmp_level = tmp_level->chield;
  }

  //Insert the leaves in a batch
  leaf_insert (leaf, leaf_idx, ARR_SIZE, nexthop, prefix_len);

  //Leaf pushing
  if (l->chield) {
    for (i = 0; i < leaf_pushing_prefixes_count; i++) {
      if (curr_level == 124) {
        //Pushing from level 124 to level 130
        matching_prefix = leaf_pushing_prefixes[i] + (0 << 3);
        _poptrie_insert (matching_prefix, prefix_len, nexthop, curr_level + 1);
        matching_prefix = leaf_pushing_prefixes[i] + (1 << 3);
        _poptrie_insert (matching_prefix, prefix_len, nexthop, curr_level + 1);
      } else {
        matching_prefix =  ((leaf_pushing_prefixes[i] >> (128 - dst_level)) + (0 << 5)) << (128 - dst_level);
        _poptrie_insert (matching_prefix, prefix_len , nexthop, curr_level + 1);
        matching_prefix =  ((leaf_pushing_prefixes[i] >> (128 - dst_level)) + (1 << 5)) << (128 - dst_level);
        _poptrie_insert (matching_prefix, prefix_len , nexthop, curr_level + 1);
      }
    }
  }
  return 0;
}

//Checks if there a leaf in the level; if yes, it then move the leafs to the next level
static int leaf_pushing (struct poptrie_level *l, uint32_t idx, uint32_t stride, struct leaf *leafs, __uint128_t key) {
  register uint32_t n_idx;
  register __uint128_t matching_key;
  register long long i;
  register uint8_t next_hop, prefix_len;
  register struct poptrie_level *runner;
  //Current level
  register int curr_level = l->level_num + 6;
  
  //Matching leaf found, so need to push it to the next level
  if (l->B[idx].leafvec & (1ULL << stride)) {
    n_idx = calc_n_idx(l, idx, stride);
    next_hop = leafs->N[n_idx];
    prefix_len = leafs->P[n_idx];

    //shift each element one step left
    memmove(&leafs->N[n_idx], &leafs->N[n_idx + 1],
            (leafs->count - n_idx - 1) * sizeof(leafs->N[0]));
    memmove(&leafs->P[n_idx], &leafs->P[n_idx + 1],
            (leafs->count - n_idx - 1) * sizeof(leafs->P[0]));

    //reset the last elementment
    leafs->N[leafs->count - 1] = 0;
    leafs->P[leafs->count - 1] = 0;
    leafs->count--;

    //turn off the bit
    l->B[idx].leafvec &= ~(1ULL << stride);

    //Update base1 of following chunks
    for (i = idx + 1; i < l->count; i++) {
      if (l->B[i].leafvec && l->B[i].base1 > 0)
        l->B[i].base1--;
    }

    //Update base1 of children
    runner = l->chield;
    while (runner) {
      for (i = 0; i < runner->count; i++) {
        if (runner->B[i].leafvec && runner->B[i].base1 > 0)
          runner->B[i].base1--;
      }
      runner = runner->chield;
    }

    if (curr_level != 124) {
      //Key to which the match was found and add 6 bits to the right
      matching_key = (key >> (128 - curr_level)) << 6;
      _poptrie_insert ((matching_key + (0 << 5)) << (122 - curr_level), prefix_len, next_hop, curr_level + 1);
      _poptrie_insert ((matching_key + (1 << 5)) << (122 - curr_level), prefix_len, next_hop, curr_level + 1);
    } else {
      //Pushing from level 124 to level 130
      matching_key = (key >> 4) << 4;
      _poptrie_insert (matching_key + (0 << 3), prefix_len, next_hop, curr_level + 1);
      _poptrie_insert (matching_key + (1 << 3), prefix_len, next_hop, curr_level + 1);
    }
  }
  return 0;
}

int poptrie_insert(__uint128_t key, int prefix_len, int nexthop) {
  //nexthop cannot be 0. We use 0 to indicate that next-hop doesn't exist.
  if (!nexthop) {
    puts ("nexthop cannot be 0. Please fix the routing table");
    exit (1);
  }
  //level is same as prefix len
  return _poptrie_insert(key, prefix_len, nexthop, prefix_len);
}

//This function will be called by poptrie_insert() and by itself recursively for
//leaf pushing. When called by poptrie_insert(), level and prefix length should
//be same. When called recursively, level will be higher than the prefix length.
static int _poptrie_insert(__uint128_t key, int prefix_len, int nexthop, int level) {
  register int i, j;
  register uint32_t stride;
  //Index to arrays at each level
  register uint32_t idx;
  register int err = 0;
  register uint32_t chunk_id = 0;
  register uint32_t num_leafs;/*Number of leafs need to be inserted for this prefix*/
  register uint8_t tmp_next_hop, tmp_prefix_len;

  if (prefix_len == 0) {
    poptrie.def_nh = nexthop;
    goto finish;
  }

  /*Eextract 16 bits from MSB.*/
  idx = key >> 112;
  if (level <= 16) {
    /*All the leafs in level 1~16 will be stored in level 16.*/
    num_leafs = 1U << (16 - level);
    for (i = 0; i < num_leafs; i++) {
      //Longer prefix exist, so move the prefix to upper level
      if (poptrie.dir16.c[idx + i] != 0) {
        //The pushing is performed by two insert call
        _poptrie_insert (key, prefix_len , nexthop, 16 + 1);
        _poptrie_insert (key | ((__uint128_t)1 <<(128 - 16 - 1)), prefix_len , nexthop, 16 + 1);
      } else {
        /*Longer prefix exists*/
        if (poptrie.leafs16.P[idx + i] > prefix_len)
          continue;
        poptrie.leafs16.N[idx + i] = nexthop;
        poptrie.leafs16.P[idx + i] = prefix_len;
      }
    }
    goto finish;
  }

  //There is a matching prefix in level 16, leaf pushing to next level
  if (poptrie.leafs16.N[idx]) {
    tmp_next_hop = poptrie.leafs16.N[idx];
    tmp_prefix_len = poptrie.leafs16.P[idx];
    //set this to zero before making recursive call. Otherwise the call will come here again
    poptrie.leafs16.N[idx] = 0;
    poptrie.leafs16.P[idx] = 0;
    _poptrie_insert((key >> 112) << 112, tmp_prefix_len, tmp_next_hop, 16 + 1);
    _poptrie_insert(((key >> 112) << 112) | ((__uint128_t)1 <<(128 - 16 - 1)), tmp_prefix_len, tmp_next_hop, 16 + 1);
  }

  //The prefix length is longer than 16, so get index to level 16 from DIR array
  if (poptrie.dir16.c[idx] == 0) {
    //Calculate chunk ID from dir
    chunk_id = calc_ckid(&poptrie.dir16, idx);
    if (!chunk_id)
      goto error;
    //Insert into level 16
    err = node_insert(&poptrie.L16, chunk_id);
    if (err)
      goto error;
    //Update dir
    err = update_ckid(&poptrie.dir16, idx, chunk_id);
    if (err)
      goto error;
  }
  idx = poptrie.dir16.c[idx] - 1;

  //Visiting level 16. Note that in Poptrie, leaf will always be in the
  //last (leaf) level. So level 16 will contain pointer to these leaves with
  //prefix length 17-22
  stride = (key >> 106) & 63;
  if (level <= 22) {
    err = insert_leaf(&poptrie.L16, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L16, idx, stride, &poptrie.leafs, key);

  //Visiting level 22. It will contain pointers to the prefixes with length 23-28
  idx = get_idx_to_next_level (&poptrie.L16, idx, stride);
  stride = (key >> 100) & 63;
  if (level <= 28) {
    err = insert_leaf(&poptrie.L22, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L22, idx, stride, &poptrie.leafs, key);

  //Visiting level 28. It will contain pointers to the prefixes with length 29-34
  idx = get_idx_to_next_level (&poptrie.L22, idx, stride);
  stride = (key >> 94) & 63;
  if (level <= 34) {
    err = insert_leaf(&poptrie.L28, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L28, idx, stride, &poptrie.leafs, key);

  //Visiting level 34
  idx = get_idx_to_next_level (&poptrie.L28, idx, stride);
  stride = (key >> 88) & 63;
  if (level <= 40) {
    err = insert_leaf(&poptrie.L34, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L34, idx, stride, &poptrie.leafs, key);

  //Visiting level 40
  idx = get_idx_to_next_level (&poptrie.L34, idx, stride);
  stride = (key >> 82) & 63;
  if (level <= 46) {
    err = insert_leaf(&poptrie.L40, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L40, idx, stride, &poptrie.leafs, key);

  //Visiting level 46
  idx = get_idx_to_next_level (&poptrie.L40, idx, stride);
  stride = (key >> 76) & 63;
  if (level <= 52) {
    err = insert_leaf(&poptrie.L46, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L46, idx, stride, &poptrie.leafs, key);

  //Visiting level 52
  idx = get_idx_to_next_level (&poptrie.L46, idx, stride);
  stride = (key >> 70) & 63;
  if (level <= 58) {
    err = insert_leaf(&poptrie.L52, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L52, idx, stride, &poptrie.leafs, key);

  //Visiting level 58
  idx = get_idx_to_next_level (&poptrie.L52, idx, stride);
  stride = (key >> 64) & 63;
  if (level <= 64) {
    err = insert_leaf(&poptrie.L58, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L58, idx, stride, &poptrie.leafs, key);

  //Visiting level 64
  idx = get_idx_to_next_level (&poptrie.L58, idx, stride);
  stride = (key >> 58) & 63;
  if (level <= 70) {
    err = insert_leaf(&poptrie.L64, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L64, idx, stride, &poptrie.leafs, key);

  //Visiting level 70
  idx = get_idx_to_next_level (&poptrie.L64, idx, stride);
  stride = (key >> 52) & 63;
  if (level <= 76) {
    err = insert_leaf(&poptrie.L70, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L70, idx, stride, &poptrie.leafs, key);

  //Visiting level 76
  idx = get_idx_to_next_level (&poptrie.L70, idx, stride);
  stride = (key >> 46) & 63;
  if (level <= 82) {
    err = insert_leaf(&poptrie.L76, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L76, idx, stride, &poptrie.leafs, key);

  //Visiting level 82
  idx = get_idx_to_next_level (&poptrie.L76, idx, stride);
  stride = (key >> 40) & 63;
  if (level <= 88) {
    err = insert_leaf(&poptrie.L82, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L82, idx, stride, &poptrie.leafs, key);

  //Visiting level 88
  idx = get_idx_to_next_level (&poptrie.L82, idx, stride);
  stride = (key >> 34) & 63;
  if (level <= 94) {
    err = insert_leaf(&poptrie.L88, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L88, idx, stride, &poptrie.leafs, key);

  //Visiting level 94
  idx = get_idx_to_next_level (&poptrie.L88, idx, stride);
  stride = (key >> 28) & 63;
  if (level <= 100) {
    err = insert_leaf(&poptrie.L94, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L94, idx, stride, &poptrie.leafs, key);

  //Visiting level 100
  idx = get_idx_to_next_level (&poptrie.L94, idx, stride);
  stride = (key >> 22) & 63;
  if (level <= 106) {
    err = insert_leaf(&poptrie.L100, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L100, idx, stride, &poptrie.leafs, key);

  //Visiting level 106
  idx = get_idx_to_next_level (&poptrie.L100, idx, stride);
  stride = (key >> 16) & 63;
  if (level <= 112) {
    err = insert_leaf(&poptrie.L106, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L106, idx, stride, &poptrie.leafs, key);

  //Visiting level 112
  idx = get_idx_to_next_level (&poptrie.L106, idx, stride);
  stride = (key >> 10) & 63;
  if (level <= 118) {
    err = insert_leaf(&poptrie.L112, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L112, idx, stride, &poptrie.leafs, key);

  //Visiting level 118
  idx = get_idx_to_next_level (&poptrie.L112, idx, stride);
  stride = (key >> 4) & 63;
  if (level <= 124) {
    err = insert_leaf(&poptrie.L118, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  leaf_pushing (&poptrie.L118, idx, stride, &poptrie.leafs, key);

  //Visiting level 124
  idx = get_idx_to_next_level (&poptrie.L118, idx, stride);
  stride = (key & 15) << 2;
  if (level <= 130) {
    err = insert_leaf(&poptrie.L124, level, idx, stride, key, prefix_len, nexthop, &poptrie.leafs);
    if (err) goto error;
    goto finish;
  }
  return 0;

error:
  puts("Something went wrong in route insertion");
  return -1;
finish:
//  leaf_print (&poptrie.leafs);
//  poptrie_level_print (&poptrie.L40);
  return 0;
}

/*Calculating index to the next level*/
#define IDX_NXT(NODE, STRIDE) (NODE->base0 + POPCNT(NODE->vec & \
                              ((2ULL << STRIDE) - 1)) - 1)

uint8_t poptrie_lookup(__uint128_t key) {
  register uint32_t n_idx;
  register uint32_t stride;
  register uint32_t idx;
  register struct poptrie_node *node;
  register uint8_t nh = poptrie.def_nh;

  idx = key >> 112;
  if (poptrie.leafs16.N[idx]) {
    return poptrie.leafs16.N[idx];
  }

  idx = poptrie.dir16.c[idx];
  if (!idx)
    return nh;
  node = &poptrie.L16.B[idx - 1];
  stride = (key >> 106) & 63;
  if (node->vec & (1ULL << stride)) {
    idx = IDX_NXT(node, stride);
    node = &poptrie.L22.B[idx];
    stride = (key >> 100) & 63;
    if (node->vec & (1ULL << stride)) {
      idx = idx = IDX_NXT(node, stride);
      node = &poptrie.L28.B[idx];
      stride = ((key >> 94) & 63);
      if (node->vec & (1ULL << stride)) {
        idx = idx = IDX_NXT(node, stride);
        node = &poptrie.L34.B[idx];
        stride = ((key >> 88) & 63);
        if (node->vec & (1ULL << stride)) {
          idx = idx = IDX_NXT(node, stride);
          node = &poptrie.L40.B[idx];
          stride = ((key >> 82) & 63);
          if (node->vec & (1ULL << stride)) {
            idx = IDX_NXT(node, stride);
            node = &poptrie.L46.B[idx];
            stride = ((key >> 76) & 63);
            if (node->vec & (1ULL << stride)) {
              idx = IDX_NXT(node, stride);
              node = &poptrie.L52.B[idx];
              stride = ((key >> 70) & 63);
              if (node->vec & (1ULL << stride)) {
                idx = IDX_NXT(node, stride);
                node = &poptrie.L58.B[idx];
                stride = ((key >> 64) & 63);
                if (node->vec & (1ULL << stride)) {
                  idx = IDX_NXT(node, stride);
                  node = &poptrie.L64.B[idx];
                  stride = ((key >> 58) & 63);
                  if (node->vec & (1ULL << stride)) {
                    idx = IDX_NXT(node, stride);
                    node = &poptrie.L70.B[idx];
                    stride = ((key >> 52) & 63);
                    if (node->vec & (1ULL << stride)) {
                      idx = IDX_NXT(node, stride);
                      node = &poptrie.L76.B[idx];
                      stride = ((key >> 46) & 63);
                      if (node->vec & (1ULL << stride)) {
                        idx = IDX_NXT(node, stride);
                        node = &poptrie.L82.B[idx];
                        stride = ((key >> 40) & 63);
                        if (node->vec & (1ULL << stride)) {
                          idx = IDX_NXT(node, stride);
                          node = &poptrie.L88.B[idx];
                          stride = ((key >> 34) & 63);
                          if (node->vec & (1ULL << stride)) {
                            idx = IDX_NXT(node, stride);
                            node = &poptrie.L94.B[idx];
                            stride = ((key >> 28) & 63);
                            if (node->vec & (1ULL << stride)) {
                              idx = IDX_NXT(node, stride);
                              node = &poptrie.L100.B[idx];
                              stride = ((key >> 22) & 63);
                              if (node->vec & (1ULL << stride)) {
                                idx = IDX_NXT(node, stride);
                                node = &poptrie.L106.B[idx];
                                stride = ((key >> 16) & 63);
                                if (node->vec & (1ULL << stride)) {
                                  idx = IDX_NXT(node, stride);
                                  node = &poptrie.L112.B[idx];
                                  stride = ((key >> 10) & 63);
                                  if (node->vec & (1ULL << stride)) {
                                    idx = IDX_NXT(node, stride);
                                    node = &poptrie.L118.B[idx];
                                    stride = ((key >> 4) & 63);
                                    if (node->vec & (1ULL << stride)) {
                                      idx = IDX_NXT(node, stride);
                                      node = &poptrie.L124.B[idx];
                                      stride = (key & 15) << 2;
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  if (node->leafvec & (1ULL << stride)) {
    n_idx = node->base1 + POPCNT(node->leafvec & ((2ULL << stride) - 1)) - 1;
    nh = poptrie.leafs.N[n_idx];
  }

  return nh;
}

//This is same as FIB lookup, except it returns matched prefix length instead
//of next-hop index
uint8_t poptrie_matched_prefix_len(__uint128_t key) {
  register uint32_t n_idx;
  register uint32_t stride;
  register uint32_t idx;
  register struct poptrie_node *node;
  register uint8_t nh = poptrie.def_nh;
  int k;

  idx = key >> 112;
  if (poptrie.leafs16.N[idx]) {
    return 16;
  }

  idx = poptrie.dir16.c[idx];
  if (!idx) {
    return 16;
  }
  node = &poptrie.L16.B[idx - 1];
  stride = (key >> 106) & 63;
  k = 22;
  if (node->vec & (1ULL << stride)) {
    k = 28;
    idx = IDX_NXT(node, stride);
    node = &poptrie.L22.B[idx];
    stride = (key >> 100) & 63;
    if (node->vec & (1ULL << stride)) {
      k = 34;
      idx = idx = IDX_NXT(node, stride);
      node = &poptrie.L28.B[idx];
      stride = ((key >> 94) & 63);
      if (node->vec & (1ULL << stride)) {
        k = 40;
        idx = idx = IDX_NXT(node, stride);
        node = &poptrie.L34.B[idx];
        stride = ((key >> 88) & 63);
        if (node->vec & (1ULL << stride)) {
          k = 46;
          idx = idx = IDX_NXT(node, stride);
          node = &poptrie.L40.B[idx];
          stride = ((key >> 82) & 63);
          if (node->vec & (1ULL << stride)) {
            k = 52;
            idx = IDX_NXT(node, stride);
            node = &poptrie.L46.B[idx];
            stride = ((key >> 76) & 63);
            if (node->vec & (1ULL << stride)) {
              k = 58;
              idx = IDX_NXT(node, stride);
              node = &poptrie.L52.B[idx];
              stride = ((key >> 70) & 63);
              if (node->vec & (1ULL << stride)) {
                k = 64;
                idx = IDX_NXT(node, stride);
                node = &poptrie.L58.B[idx];
                stride = ((key >> 64) & 63);
                if (node->vec & (1ULL << stride)) {
                  k = 70;
                  idx = IDX_NXT(node, stride);
                  node = &poptrie.L64.B[idx];
                  stride = ((key >> 58) & 63);
                  if (node->vec & (1ULL << stride)) {
                    k = 76;
                    idx = IDX_NXT(node, stride);
                    node = &poptrie.L70.B[idx];
                    stride = ((key >> 52) & 63);
                    if (node->vec & (1ULL << stride)) {
                      k = 82;
                      idx = IDX_NXT(node, stride);
                      node = &poptrie.L76.B[idx];
                      stride = ((key >> 46) & 63);
                      if (node->vec & (1ULL << stride)) {
                        k = 88;
                        idx = IDX_NXT(node, stride);
                        node = &poptrie.L82.B[idx];
                        stride = ((key >> 40) & 63);
                        if (node->vec & (1ULL << stride)) {
                          k = 94;
                          idx = IDX_NXT(node, stride);
                          node = &poptrie.L88.B[idx];
                          stride = ((key >> 34) & 63);
                          if (node->vec & (1ULL << stride)) {
                            k = 100;
                            idx = IDX_NXT(node, stride);
                            node = &poptrie.L94.B[idx];
                            stride = ((key >> 28) & 63);
                            if (node->vec & (1ULL << stride)) {
                              k = 106;
                              idx = IDX_NXT(node, stride);
                              node = &poptrie.L100.B[idx];
                              stride = ((key >> 22) & 63);
                              if (node->vec & (1ULL << stride)) {
                                k = 112;
                                idx = IDX_NXT(node, stride);
                                node = &poptrie.L106.B[idx];
                                stride = ((key >> 16) & 63);
                                if (node->vec & (1ULL << stride)) {
                                  k = 118;
                                  idx = IDX_NXT(node, stride);
                                  node = &poptrie.L112.B[idx];
                                  stride = ((key >> 10) & 63);
                                  if (node->vec & (1ULL << stride)) {
                                    k = 124;
                                    idx = IDX_NXT(node, stride);
                                    node = &poptrie.L118.B[idx];
                                    stride = ((key >> 4) & 63);
                                    if (node->vec & (1ULL << stride)) {
                                      k = 130;
                                      idx = IDX_NXT(node, stride);
                                      node = &poptrie.L124.B[idx];
                                      stride = (key & 15) << 2;
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  if (node->leafvec & (1ULL << stride)) {
    return k;
  }
  return 16;
}
