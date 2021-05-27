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
#include "prefix_distribution.h"
#include <stdio.h>
#include <stdlib.h>

#define NUM_POSSIBLE_PREFIX_LEN 129

//prefix length distribution across all the FIBs
uint64_t prefix_length_dist [NUM_POSSIBLE_PREFIX_LEN];
//Total number of prefixes across all the FIBs
uint64_t total_prefixes = 0;

void record_prefix_len (int len) {
  prefix_length_dist[len]++;
  total_prefixes++;
}

void dump_prefix_distribution (FILE *output) {
  double sum;
  double prefix_length_precentage [NUM_POSSIBLE_PREFIX_LEN];

  //Calculate percentage
  for(int i = 0; i < NUM_POSSIBLE_PREFIX_LEN; i++)
    prefix_length_precentage[i] = ((double)prefix_length_dist[i] * 100)/total_prefixes;

  fprintf(output, "PrefixLength	Percenrage \n");
  for(int i = 0; i < NUM_POSSIBLE_PREFIX_LEN; i++) {
    if (prefix_length_precentage[i])
      fprintf(output, "%d	%3f \n", i, prefix_length_precentage[i]);
  }

  fprintf(output, "\n\n");

  fprintf(output, "PrefixLength	Percenrage (Aggregated) \n");
  sum = 0;
  for(int i = 0; i < 32; i++)
    sum += prefix_length_precentage[i];
  fprintf(output, "0-31	%3f \n", sum);
  fprintf(output, "32	%3f \n", prefix_length_precentage[32]);
  sum = 0;
  for(int i = 33; i < 40; i++)
    sum += prefix_length_precentage[i];
  fprintf(output, "33-39	%3f \n", sum);
  fprintf(output, "40	%3f \n", prefix_length_precentage[40]);
  sum = 0;
  for(int i = 41; i < 48; i++)
    sum += prefix_length_precentage[i];
  fprintf(output, "41-47	%3f \n", sum);
  fprintf(output, "48	%3f \n", prefix_length_precentage[48]);
  sum = 0;
  for(int i = 49; i <= 64; i++)
    sum += prefix_length_precentage[i];
  fprintf(output, "49-64	%3f \n", sum);
  sum = 0;
  for(int i = 65; i < NUM_POSSIBLE_PREFIX_LEN; i++)
    sum += prefix_length_precentage[i];
  fprintf(output, "65-128	%3f \n", sum);
}
