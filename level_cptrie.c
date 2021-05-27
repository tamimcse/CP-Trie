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
#include "level_cptrie.h"
#include <stdio.h>
#include <stdlib.h>

int cptrie_level_init (struct cptrie_level *l, uint8_t level_num, uint32_t size, struct cptrie_level *parent) {
  l->B = (struct bitmap_cptrie *) calloc (size, sizeof (struct bitmap_cptrie));
  l->C = (struct bitmap_cptrie *) calloc (size, sizeof (struct bitmap_cptrie));
  if (!l->B || !l->C)
    return -1;
  l->level_num = level_num;
  l->size = size/ELEMS_PER_STRIDE;
  l->parent = parent;
  if (parent != NULL)
    parent->chield = l;
  return 0;
}

int cptrie_level_cleanup (struct cptrie_level *l) {
  free(l->B);
  free(l->C);
  l->size = 0;
  l->count = 0;
  l->parent = NULL;
  l->chield = NULL;
}

double mem_size (struct cptrie_level *l) {
  //Each level has B and C array where each element of the array
  // is 12 byte (struct bitmap_cptrie).
  return l->count * ELEMS_PER_STRIDE * 12 * 2;
}

uint32_t count_empty_chunks (struct cptrie_level *l)
{
  long long i;
  uint32_t num = 0;

  for (i = 0; i < l->count * ELEMS_PER_STRIDE; i++) {
    if (!l->B[i].bitmap) {
      num++;
    }
  }
  return num;
}

int cptrie_level_print (struct cptrie_level *l) {
  int i;

  for (i = 0; i < l->count * ELEMS_PER_STRIDE; i++)
    printf ("B[%d] = %llu/%u \n", i, l->B[i].bitmap, l->B[i].cumu_popcnt);

  for (i = 0; i < l->count * ELEMS_PER_STRIDE; i++)
    printf ("C[%d] = %llu/%u \n", i, l->C[i].bitmap, l->C[i].cumu_popcnt);

  return 0;
}

/* Insert a new chunk to a level at chunk_id-1. Note that Chunk ID
 * starts from 1, not 0.
 * This is why, it is passed by reference
 */
static int chunk_insert(struct cptrie_level *l, uint32_t chunk_id, uint32_t elems_per_stride)
{
  register long long m;
  register uint32_t b_popcnt = 0, c_popcnt = 0; 

  if (chunk_id > (l->count + 1) || chunk_id < 1) {
    puts("Invalid chunk_id");
    return -1;
  }

  //TODO: increase the array dynamically when needed.
  if (l->count >= l->size) {
    printf("Cannot insert chunk in level %d . Please increase the level size\n", l->level_num);
    return -1;
  }

  /*shift each element one step right to make space for the new one */
  memmove(&l->B[chunk_id * elems_per_stride], 
          &l->B[(chunk_id - 1) * elems_per_stride], 
          (l->count - chunk_id + 1) * elems_per_stride * sizeof (struct bitmap_cptrie));
  memmove(&l->C[chunk_id * elems_per_stride], 
          &l->C[(chunk_id - 1) * elems_per_stride], 
          (l->count - chunk_id + 1) * elems_per_stride * sizeof (struct bitmap_cptrie));
  /*Find the popcnt of the last element to the left*/
  if (chunk_id > 1) {
    b_popcnt = l->B[(chunk_id - 1) * elems_per_stride - 1].cumu_popcnt;
    c_popcnt = l->C[(chunk_id - 1) * elems_per_stride - 1].cumu_popcnt;
  }

  /*Reset the newly created empty chunk*/
  m = (chunk_id - 1) * elems_per_stride;
  for (; m < chunk_id * elems_per_stride; m++) {
    l->B[m].bitmap = 0;
    l->B[m].cumu_popcnt = b_popcnt;
      l->C[m].bitmap = 0;
      l->C[m].cumu_popcnt = c_popcnt;
  }
  l->count++;
  return 0;
}

/*Update bitmap of the current stride and cumu_popcnt of the following strides*/
static int update_bitmap_cumu_popcnt(struct cptrie_level *l, uint32_t idx, uint32_t bit_spot)
{
  register long long i;
  register uint64_t cumu_popcnt;

  if (l->C[idx].bitmap & (MSK >> bit_spot)) {
    printf("Error: bitmap is already set");
    return -1;
  }

  /*find cumu_popcnt from current chunk*/
  if (l->C[idx].bitmap) {
    cumu_popcnt = l->C[idx].cumu_popcnt + POPCNT_LFT(l->C[idx].bitmap, bit_spot);
    goto found;
  }

  /*find cumu_popcnt from chunks to the left*/
  for (i = (long long)idx - 1; i >= 0; i--) {
    if (l->C[i].bitmap) {
      cumu_popcnt = l->C[i].cumu_popcnt + POPCNT(l->C[i].bitmap);
      goto found;
    }  
  }
  /*This is the first element of this chunk*/
  cumu_popcnt = 0;
found:
  if (!l->C[idx].bitmap)
    l->C[idx].cumu_popcnt = (uint32_t)cumu_popcnt;

  l->C[idx].bitmap |= (MSK >> bit_spot);

  /*Update offset of the chunks to the right*/
  for (i = (long long)idx + 1; i < l->count * ELEMS_PER_STRIDE; i++)
    if (l->C[i].bitmap)
      l->C[i].cumu_popcnt++;

  return 0;
}

static uint32_t calc_idx(struct bitmap_cptrie *c, uint32_t idx, uint32_t bit_spot)
{
  register long long i;
  //This chunk is already populated
  if (c[idx].bitmap) {
    return c[idx].cumu_popcnt + POPCNT_LFT(c[idx].bitmap, bit_spot);
  } else {
    //find a valid entry to the left
    for (i = (long long)idx - 1; i >= 0; i--) {
      if (c[i].bitmap) {
        //update the popcnt and return it
        c[idx].cumu_popcnt = c[i].cumu_popcnt + POPCNT(c[i].bitmap);
        return c[idx].cumu_popcnt;
      }
    }
  }
  //No chunk exist to the left
  return 0;  
}

//Calculate chunk index
uint32_t get_chunk_idx_frm_parent (struct cptrie_level *l, uint32_t idx,
                                   uint32_t bit_spot)
{
  register uint32_t chunk_id;
  register int err = 0;

  assert(l->chield != NULL);
  //The chunk does not exists already, so need to insert one
  if (!(l->C[idx].bitmap & (MSK >> bit_spot))) {
    //Calculate chunk idx based on the elements to the left  
    chunk_id = calc_idx(l->C, idx, bit_spot) + 1;
    if (!chunk_id)
      return -1;
    //Insert chunk
    err = chunk_insert(l->chield, chunk_id, ELEMS_PER_STRIDE);
    if (err) {
      puts("Could not insert chunk to level");
      return -1;
    }
    //Update bitmap of the current stride and cumu_pocnt of the following strides 
    err = update_bitmap_cumu_popcnt(l, idx, bit_spot);
    if (err)
      return -1;
  }

  return calc_idx(l->C, idx, bit_spot);
}
