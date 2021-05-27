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
#include "dir.h"
#include <stdio.h>
#include <stdlib.h>

int dir_init (struct dir *d, uint32_t size) {
  d->c = (uint16_t *) calloc (size, sizeof(uint16_t));
  d->size = size;
  d->count = 0;

  if (!d->c)
    return -1;
  else
    return 0;
}


int dir_cleanup (struct dir *d) {
  free(d->c);
  d->size = 0;
  d->count = 0;
}

int dir_print (struct dir *d) {
  for (long long i = 0; i < d->count; i++) {
    printf ("dir[%lld] = %d\n", i, d->c[i]);
  }
}

double mem_size(struct dir *d) {
  //each element is 2 bytes
  return d->size * 2;
}

/*Calculate the chunk ID*/
uint32_t calc_ckid(struct dir *d, uint32_t idx) {
  long long i;

  if (idx >= d->size) {
    puts("Index needs to be smaller than arr_size");
    return 0;
  }

  /*Find the first chunk ID to the left and increment that by 1*/
  for (i = (long long)idx - 1; i >= 0; i--) {
    if (d->c[i] > 0)
      return d->c[i] + 1;
  }

  /*If there is no chunk to the left, then this is the first chunk*/
  return 1;
}

/*Update C16 based on the newly inserted chunk*/
int update_ckid(struct dir *d, uint32_t idx, uint32_t chunk_id)
{
  long long i;

  if (idx >= d->size) {
    puts("Invalid index");
    return -1;
  }

  d->c[idx] = chunk_id;

  /* Increment chunk ID to the right */
  for (i = idx + 1; i < d->size; i++) {
    if (d->c[i] > 0)
      d->c[i]++;
  }

  return 0;
}

