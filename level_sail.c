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
#include "level_sail.h"
#include <stdio.h>
#include <stdlib.h>

int sail_level_init (struct sail_level *c, uint8_t level_num, uint32_t tot_num_chunks, uint32_t cnk_size, struct sail_level *parent) {
  uint32_t arr_size = tot_num_chunks * cnk_size;
  c->N = (uint8_t *) calloc (arr_size, sizeof (uint8_t));
  c->P = (uint8_t *) calloc (arr_size, sizeof (uint8_t));
  c->C = (uint32_t *) calloc (arr_size, sizeof (uint32_t));
  if (!c->N || !c->P || !c->C)
    return -1;
  c->level_num = level_num;
  c->size = arr_size;
  c->cnk_size = cnk_size;
  c->parent = parent;
  if (parent != NULL)
    parent->chield = c;
  return 0;
}

int sail_level_cleanup (struct sail_level *c) {
  free(c->N);
  free(c->P);
  free(c->C);
  c->size = 0;
  c->count = 0;
  c->parent = NULL;
  c->chield = NULL;
}

int sail_level_print (struct sail_level *c) {
/*  for (long long i = 0; i < c->count; i++) {
    if (c->B[i].vec || c->B[i].leafvec)
      printf ("c->B[%lld]: vec = %llu/%d leafvec = %llu/%d \n", i, c->B[i].vec, c->B[i].base0, c->B[i].leafvec, c->B[i].base1);
  }*/
}

double mem_size (struct sail_level *c) {
  //For lookup, we need N and C array where each element is 1 and 4 bytes respectively
  return c->count * c->cnk_size * (1 + 4);
}

static int chunk_insert(struct sail_level *c, uint32_t chunk_id)
{
  register long long m;

  if (chunk_id > (c->count + 1) || chunk_id < 1) {
    puts("Invalid chunk_id");
    return -1;
  }

  //TODO: increase the array dynamically when needed.
  if (c->count >= c->size/c->cnk_size) {
    printf("Cannot insert a new chunk in level %d. Please increase the array size\n", c->level_num);
    return -1;
  }

  /*shift each element one step right to make space for the new one */      
  memmove(&c->N[chunk_id * c->cnk_size], &c->N[(chunk_id - 1) * c->cnk_size], 
          (c->count - chunk_id + 1) * c->cnk_size);
  memmove(&c->P[chunk_id * c->cnk_size], &c->P[(chunk_id - 1) * c->cnk_size], 
          (c->count - chunk_id + 1) * c->cnk_size);

  memmove(&c->C[chunk_id * c->cnk_size], &c->C[(chunk_id - 1) * c->cnk_size], 
            (c->count - chunk_id + 1) * c->cnk_size * sizeof(c->C[0]));        

  /*Reset the newly created empty chunk*/
  m = (chunk_id - 1) * c->cnk_size;
  for (; m < chunk_id * c->cnk_size; m++) {
    c->N[m] = 0;
    c->P[m] = 0;
    c->C[m] = 0;
  }

finish:
  c->count++;
  return 0;
}

/*Calculate the chunk ID*/
static uint32_t calc_ckid(struct sail_level *c, uint32_t idx)
{
  register long long i;

  //TODO: increase the array dynamically when needed.
  if (idx >= c->size) {
    printf("Array index out of bound in SAIL level %d. Please increase the array size.\n", c->level_num);
    return 0;
  }

  /*Find the first chunk ID to the left and increment that by 1*/
  for (i = (long long)idx - 1; i >= 0; i--) {
    if (c->C[i] > 0)
      return c->C[i] + 1;
  }

  /*If there is no chunk to the left, then this is the first chunk*/
  return 1;
}

/*Update C16 based on the newly inserted chunk*/
static int update_c(struct sail_level *c, uint32_t idx, uint32_t chunk_id)
{
  register long long i;

  if (idx >= c->size) {
    puts("Invalid index");
    return -1;
  }

  c->C[idx] = chunk_id;

  /* Increment chunk ID to the right */
  for (i = idx + 1; i < c->size; i++) {
    if (c->C[i] > 0)
      c->C[i]++;
  }

  return 0;
}

//Get chunk ID based on parent. This function inserts chunk if needed
uint32_t get_chunk_id_frm_parent (struct sail_level *parent, uint32_t idx) {
  register uint32_t chunk_id;
  register int err;

  assert (parent->chield != NULL);
  if (parent->C[idx] == 0) {
    /*Step 1*/
    chunk_id = calc_ckid(parent, idx);
    if (!chunk_id)
      exit(0);
    /*Step 2*/
    err = chunk_insert(parent->chield, chunk_id);
    if (err) {
      puts("Could not insert chunk to level in SAIL");
      exit(0);
    }
    /*Step 3*/
    err = update_c(parent, idx, chunk_id);
    if (err)
      exit(0);
  }

  return parent->C[idx];
}
