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
#ifndef SAIL_U_IP6_H_
#define SAIL_U_IP6_H_

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

int sail_u_init();
int sail_u_cleanup();
double calc_sail_u_mem();
int sail_u_insert(__uint128_t ip, int prefix_len, int nexthop);
uint8_t sail_u_lookup(__uint128_t key);
uint8_t sail_u_matched_prefix_len(__uint128_t key) ;


#endif /* SAIL_U_IP6_H_ */
