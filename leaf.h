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
#ifndef LEAF_H_
#define LEAF_H_

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

struct leaf {
  uint8_t *N;
  uint8_t *P;
  uint64_t size;
  uint64_t count;
};

struct uint32_Map {
    uint32_t start_idx;
    uint32_t num_consecutive_leaves;
};

int leaf_init (struct leaf *l, uint32_t size);
int leaf_cleanup (struct leaf *l);
double mem_size (struct leaf *l);
int leaf_print (struct leaf *l);
int leaf_insert (struct leaf *l, uint32_t idx, uint8_t next_hop, uint8_t prefix_len);
int leaf_insert (struct leaf *l, uint32_t idx, uint32_t num_leaves, uint8_t next_hop, uint8_t prefix_len);
int leaf_insert (struct leaf *l, struct uint32_Map *idx_map, int map_size, uint8_t next_hop, uint8_t prefix_len);

#endif /* LEAF_H_ */
