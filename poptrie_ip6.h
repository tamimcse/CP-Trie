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
#ifndef POPTRIE_IP6_H_
#define POPTRIE_IP6_H_

#include "level_poptrie.h"
#include "leaf.h"
#include "dir.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

struct poptrie {
  uint8_t def_nh;
  struct leaf leafs16;
  //direct pointer
  struct dir dir16;
  //Rest of the leafs
  struct leaf leafs;
  //Non-leaf nodes
  struct poptrie_level L16, L22, L28, L34, L40, L46, L52, L58, L64, L70, L76, L82, L88, L94, L100, L106, L112, L118, L124;
};

int poptrie_init();
int poptrie_cleanup();
double calc_poptrie_mem();
int poptrie_insert(__uint128_t ip, int prefix_len, int nexthop);
uint8_t poptrie_lookup(__uint128_t key);
uint8_t poptrie_matched_prefix_len(__uint128_t key);


#endif /* POPTRIE_IP6_H_ */
