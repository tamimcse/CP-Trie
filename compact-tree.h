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
#ifndef COMPACT_TREE_H_
#define COMPACT_TREE_H_

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

//The height of the tree is at most 8
struct compact_tree {
  uint32_t bitmap;
  uint8_t bitmap_len;
};

int tree_insert (struct compact_tree *tree, uint8_t prefix, uint8_t prefix_len);
int tree_lookup (struct compact_tree *tree, uint8_t prefix);

#endif /* COMPACT_TREE_H_ */
