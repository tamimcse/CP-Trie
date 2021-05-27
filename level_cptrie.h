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
#ifndef LEVEL_CPTRIE_H_
#define LEVEL_CPTRIE_H_

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>

/*Calculates the number of bits set to 1*/
#define POPCNT(X) (__builtin_popcountll(X))

/*POPCNT of right-most N bits of X*/
#define POPCNT_RGT(X, N) (__builtin_popcountll(((1ULL << (N)) - 1) & (X)))

/*POPCNT of left-most N bits of X*/
#define POPCNT_LFT(X, N) (__builtin_popcountll(((X) >> (63 - (N))) >> 1))

/* Each stride consists of 4 (256/64) elements */
#define ELEMS_PER_STRIDE 4

#define MSK 0X8000000000000000ULL

struct bitmap_cptrie {
    uint64_t bitmap;
    uint32_t cumu_popcnt;
};

struct cptrie_level {
  struct bitmap_cptrie *B, *C;
  uint8_t level_num;
  uint32_t count;
  uint32_t size;
  struct cptrie_level *parent, *chield;
};

int cptrie_level_init (struct cptrie_level *l, uint8_t level_num, uint32_t size, struct cptrie_level *parent);
int cptrie_level_cleanup (struct cptrie_level *l);
double mem_size (struct cptrie_level *l);
int cptrie_level_print (struct cptrie_level *l);
uint32_t get_chunk_idx_frm_parent (struct cptrie_level *l, uint32_t idx, uint32_t bit_spot);
uint32_t count_empty_chunks (struct cptrie_level *l);
#endif /* LEVEL_CPTRIE_H_ */
