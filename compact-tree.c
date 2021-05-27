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
#include "compact-tree.h"

/*Calculates the number of bits set to 1*/
#define POPCNT(X) (__builtin_popcount(X))

#define MASK 0X80000000

static uint8_t left_child (struct compact_tree *tree, uint8_t idx)
{
  return 2 * POPCNT(tree->bitmap >> (31 - idx)) - 1;
}

static uint8_t right_child (struct compact_tree *tree, uint8_t idx)
{
  return 2 * POPCNT(tree->bitmap >> (31 - idx));
}

static bool is_leaf (struct compact_tree *tree, uint8_t idx) {
//  printf ("bitmap = %u \n", tree->bitmap);
  return !(tree->bitmap & (MASK >> idx));
}

//Make the external/leaf node an internal nodce
static bool make_internal (struct compact_tree *tree, uint8_t idx) {
  tree->bitmap |= (MASK >> idx);
}

int tree_insert (struct compact_tree *tree, uint8_t prefix, uint8_t prefix_len)
{
  int i;
  uint8_t child_idx, left_child_idx;
  uint32_t mask;

  //first node of the tree
  if (!tree->bitmap) {
    tree->bitmap = 0X80000000;
    tree->bitmap_len = 3;
  }

  if (prefix_len > 8 || prefix_len < 1) {
    puts ("invalid prefix length");
    return -1;
  }
  
  prefix <<= (8 - prefix_len);
  //start from root
  child_idx = 0;
  for (i = 0; i < prefix_len; i++) {
//    printf ("prefix = %u \n", prefix);
    if (prefix & 0b10000000) {
//      puts ("to the right");
      child_idx = right_child (tree, child_idx);
    } else {
//      puts ("to the left");
      child_idx = left_child (tree, child_idx);
    }
//    printf ("child_idx = %d \n", child_idx);
    if (is_leaf (tree, child_idx)) {
      make_internal(tree, child_idx);
//      printf ("Updating index %d \n", child_idx);
      left_child_idx = left_child (tree, child_idx);
      mask = ((1U <<  left_child_idx) - 1);
      tree->bitmap = ((mask & tree->bitmap) >> 2) | (~mask & tree->bitmap);
//      printf ("bitmap = %u \n", tree->bitmap);
      tree->bitmap_len += 2;
    }
    prefix <<= 1;
  }
  printf ("bitmap = %u \n", tree->bitmap);
}

int tree_lookup (struct compact_tree *tree, uint8_t prefix)
{
  int i;
  uint8_t idx, child_idx;
  uint32_t mask;

  for (i = 0; i < 8; i++) {
    if (prefix & 0b10000000)
      break;
    prefix = prefix << 1;
  }

  //start from root
  idx = 0;
  while (1) {
    if (prefix & 0b10000000) {
      child_idx = right_child (tree, idx);
    } else {
      child_idx = left_child (tree, idx);
    }
    if (is_leaf (tree, child_idx)) {
      return idx;
    }
    idx = child_idx;
    prefix <<= 1;
  }
}
