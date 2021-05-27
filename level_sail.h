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
#ifndef LEVEL_SAIL_H_
#define LEVEL_SAIL_H_

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>


struct sail_level {
  uint8_t *N, *P;
  uint32_t *C;
  uint8_t level_num;
  //chunk count
  uint32_t count;
  //size = total number of chunks * cnk_size 
  uint32_t size;
  //Number of elements in each chunk. We made it so that each level can have
  //chunk of differenet size (unlike the originbal SAIL)
  uint32_t cnk_size;
  struct sail_level *parent, *chield;
};

int sail_level_init (struct sail_level *c, uint8_t level_num, uint32_t size, uint32_t cnk_size, struct sail_level *parent);
int sail_level_cleanup (struct sail_level *c);
int sail_level_print (struct sail_level *c);
double mem_size (struct sail_level *c);
bool isNULL (struct sail_level *c);
uint32_t get_chunk_id_frm_parent (struct sail_level *parent, uint32_t idx);

#endif /* LEVEL_SAIL_H_ */
