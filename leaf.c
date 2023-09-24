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
#include "leaf.h"

int leaf_init (struct leaf *l, uint32_t size) {
  l->N = (uint8_t *) calloc (size, sizeof(uint8_t));
  l->P = (uint8_t *) calloc (size, sizeof(uint8_t));
  l->size = size;
  l->count = 0;

  if (!l->N || !l->P)
    return -1;
  else
    return 0;
}

int leaf_cleanup (struct leaf *l) {
  int err = 0;

  free(l->N);
  free(l->P);
  l->size = 0;
  l->count = 0;
  return err;
}

double mem_size (struct leaf *l) {
  //Each entry is 1 byte
  return l->count;
}

int leaf_print (struct leaf *l) {
  for (long long i = 0; i < l->count; i++) {
    printf ("N[%lld] = %d\n", i, l->N[i]);
  }
  return 0;
}


//Insert one next-hop/prefix-len at idx by shifting each element one step right
int leaf_insert (struct leaf *l, uint32_t idx, uint8_t next_hop, uint8_t prefix_len)
{
  return leaf_insert (l, idx, 1, next_hop, prefix_len);
}

int leaf_insert (struct leaf *l, struct uint32_Map *idx_map, int map_size, uint8_t next_hop, uint8_t prefix_len)
{
  int i;
  int ret;

  for (i = 0; i < map_size; i++) {
    if (idx_map[i].num_consecutive_leaves == 0)
      break;
    ret = leaf_insert (l, idx_map[i].start_idx, idx_map[i].num_consecutive_leaves, next_hop, prefix_len);
    if (ret < 0)
     return ret;
  }
  return 0;
}

//Insert next-hop/prefix-len at idx by shifting each element one step right
int leaf_insert (struct leaf *l, uint32_t idx, uint32_t num_leaves, uint8_t next_hop, uint8_t prefix_len)
{
  uint32_t i;

  if (l->count + num_leaves > l->size) {
    puts ("Leaf is full");
    return -1;
  }

  if (idx + num_leaves > l->size) {
    puts ("Invalid index in leaf insert");
    return -1;
  }

  //Shift each element to the right to make room for the new num_leaves elements
  memmove(&l->N[idx + num_leaves], &l->N[idx], (l->count - idx) * sizeof (l->N[0]));
  memmove(&l->P[idx + num_leaves], &l->P[idx], (l->count - idx) * sizeof (l->P[0]));

  //Insert the leaves
  for (i = idx; i < idx + num_leaves; i++) {
    l->N[i] = next_hop;
    l->P[i] = prefix_len;
  }
  l->count += num_leaves;
  return 0;
}
