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
#ifndef CPTRIE_IP6_H_
#define CPTRIE_IP6_H_

#include "leaf.h"
#include "level_cptrie.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

struct cptrie {
  uint8_t def_nh;
  struct cptrie_level level16, level24, level32, level40, level48, level56, level64, level72, level80, level88, level96, level104, level112, level120, level128;
  struct leaf leaf;
};

int cptrie_init();
int cptrie_cleanup();
double calc_cptrie_mem();
int cptrie_insert(__uint128_t ip, int prefix_len, int nexthop);
uint8_t cptrie_lookup(__uint128_t key);
uint8_t cptrie_matched_prefix_len(__uint128_t key);

#endif /* CPTRIE_IP6_H_ */
