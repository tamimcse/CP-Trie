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
#include "level_poptrie.h"
#include "leaf.h"
#include <stdio.h>
#include <stdlib.h>

/*Calculates the number of bits set to 1*/
#define POPCNT(X) (__builtin_popcountll(X))

int poptrie_level_init (struct poptrie_level *l, uint8_t level_num, uint32_t size, struct poptrie_level *parent) {
  l->B = (struct poptrie_node *) calloc (size, sizeof (struct poptrie_node));
  if (!l->B)
    return -1;
  l->level_num = level_num;
  l->size = size;
  l->parent = parent;
  if (parent != NULL)
    parent->chield = l;
  return 0;
}

int poptrie_level_cleanup (struct poptrie_level *l) {
  free(l->B);
  l->size = 0;
  l->count = 0;
  l->parent = NULL;
  l->chield = NULL;
}

int poptrie_level_print (struct poptrie_level *l) {
  for (long long i = 0; i < l->count; i++) {
    if (l->B[i].vec || l->B[i].leafvec)
      printf ("l->B[%lld]: vec = %llu/%d leafvec = %llu/%d \n", i, l->B[i].vec, l->B[i].base0, l->B[i].leafvec, l->B[i].base1);
  }
}

double mem_size (struct poptrie_level *l) {
  //each element is 24 bytes
  return l->count * 24;
}

static uint32_t calc_idx(struct poptrie_node *c, uint32_t idx, uint32_t stride)
{
  register long long i;
  //This chunk is already populated
  if (c[idx].vec) {
    if (c[idx].vec & (1ULL << stride))    
      return c[idx].base0 + POPCNT(c[idx].vec & ((2ULL << stride) - 1)) - 1;
    else
      return c[idx].base0 + POPCNT(c[idx].vec & ((2ULL << stride) - 1));
  } else {
    puts ("Something went wrong");
    exit (0); 
  }
  //No chunk exist to the left
  return 0;  
}

/* Insert a new chunk to a level at chunk_id-1. Note that Chunk ID
 * starts from 1, not 0.
 * This is why, it is passed by reference
 */
int node_insert(struct poptrie_level *L, uint32_t chunk_id)
{
  register long long m;
  register uint32_t base0 = 0, base1 = 0; 

  if (chunk_id > (L->count + 1) || chunk_id < 1) {
    puts("Invalid chunk_id");
    return -1;
  }

  //TODO: increase the array dynamically when needed.
  if (L->count >= L->size) {
    printf("Cannot insert chunk in level %d . Please increase the level size\n", L->level_num);
    return -1;
  }

  /*shift each element one step right to make space for the new one */
  memmove(&L->B[chunk_id], &L->B[chunk_id - 1], 
          (L->count - chunk_id + 1) * sizeof (struct poptrie_node));

  /*Find the popcnt of the last element to the left*/
  if (chunk_id > 1) {
    base0 = L->B[chunk_id - 2].base0;
    base1 = L->B[chunk_id - 2].base1;
  }

  /*Reset the newly created empty chunk*/
  L->B[chunk_id - 1].vec = 0;
  L->B[chunk_id - 1].leafvec = 0;
  L->B[chunk_id - 1].base0 = base0;
  L->B[chunk_id - 1].base1 = base1;

  L->count++;
  return 0;
}

/*Calculate the chunk ID from parent*/
static uint32_t calc_cid_frm_parent(struct poptrie_level *l, uint32_t idx, uint64_t stride)
{
  register long long i;
  register long long index = 0;
        
  /*find the index to C24 where previous chunk ID would be found*/
  if (l->B[idx].vec) {
    index = l->B[idx].base0 + POPCNT(l->B[idx].vec & ((2ULL << stride) - 1)) - 1;
    if (index >= 0)
      return index + 2;
  }

  for (i = (long long)idx - 1; i >= 0; i--) {
    if (l->B[i].vec) {
      index = l->B[i].base0 + POPCNT(l->B[i].vec) - 1;
      return index + 2;
    }
  }

  /*If there is no chunk to the left, then this is the first chunk*/
  return 1;
err:
  puts("Invalid index");
  return -1;
}

/*Update CK and C based on the newly inserted chunk*/
static int update_parent(struct poptrie_level *l, uint32_t idx, uint32_t stride)
{
  register long long i;
  register uint64_t index = 0;

  if (l->B[idx].vec & (1ULL << stride)) {
    puts("Error: bitmap is already set");
    return -1;
  }

  /*find the index where Chunk ID should be copied to*/
  if (l->B[idx].vec) {
    index = l->B[idx].base0 + POPCNT(l->B[idx].vec & ((2ULL << stride) - 1));
    goto index_found;
  }

  /*Find a chunk to the left which is not empty*/
  for (i = (long long)idx - 1; i >= 0; i--) {
    if (l->B[i].vec) {
      index = l->B[i].base0 + POPCNT(l->B[i].vec);
      goto index_found;
    }  
  }
index_found:
  /*This is the first element of this chunk*/
  if (!l->B[idx].vec)
    l->B[idx].base0 = (uint32_t)index;

  l->B[idx].vec |= (1ULL << stride);

  /*Update offset of the chunks to the right*/
  for (i = (long long)idx + 1; i < l->size; i++)
    if (l->B[i].vec)
      l->B[i].base0++;

  return 0;
err:
  puts("Invalid index");
  return -1;
}

//Get chunk ID based on parent. This function inserts chunk to the child on the way if needed
uint32_t get_idx_to_next_level (struct poptrie_level *parent, uint32_t idx, uint32_t stride) {
  register uint32_t chunk_id;
  register int err;

  assert (parent->chield != NULL);
  if (!(parent->B[idx].vec & (1ULL << stride))) {
    chunk_id = calc_cid_frm_parent(parent, idx, stride);
    if (!chunk_id)
      return -1;
    err = node_insert(parent->chield, chunk_id);
    if (err) {
      puts("Could not insert chunk to level in Poptrie");
      return -1;
    }
    err = update_parent(parent, idx, stride);
    if (err)
      return -1;
  }
  return calc_idx(parent->B, idx, stride);
}
