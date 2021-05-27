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
#ifndef DIR_H_
#define DIR_H_

#include <stdint.h>

struct dir {
  uint16_t *c;
  uint64_t size;
  uint64_t count;
};

int dir_init (struct dir *l, uint32_t size);
int dir_cleanup (struct dir *l);
int dir_print (struct dir *l);
double mem_size(struct dir *l);
uint32_t calc_ckid(struct dir *d, uint32_t idx);
int update_ckid(struct dir *d, uint32_t idx, uint32_t chunk_id);

#endif /* DIR_H_ */
