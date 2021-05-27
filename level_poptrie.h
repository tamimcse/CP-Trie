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
#ifndef LEVEL_POPTRIE_H_
#define LEVEL_POPTRIE_H_

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>


struct poptrie_node {
    uint64_t vec;
    uint64_t leafvec;
    /*
     *These are u16 in orginal implementation. Here we made them u32 to make
     *them future-proof.
     */ 
    uint32_t base0;
    uint32_t base1;
};

struct poptrie_level {
  struct poptrie_node *B;
  uint8_t level_num;
  uint32_t count;
  uint32_t size;
  struct poptrie_level *parent, *chield;
};

int poptrie_level_init (struct poptrie_level *l, uint8_t poptrie_level_num, uint32_t size, struct poptrie_level *parent);
int poptrie_level_cleanup (struct poptrie_level *l);
int poptrie_level_print (struct poptrie_level *l);
double mem_size (struct poptrie_level *l);
int node_insert(struct poptrie_level *L, uint32_t chunk_id);
uint32_t get_idx_to_next_level (struct poptrie_level *parent, uint32_t idx, uint32_t stride);

#endif /* LEVEL_POPTRIE_H_ */
