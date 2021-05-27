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
#include "sail_u_ip6.h"
#include "level_sail.h"

/*chunk size is 2^8*/
#define CNK_8 256

//These may need to be increased to accomodate larger routing table
#define CNK16 65536/CNK_8
#define CNK24 100
#define CNK32 7000
#define CNK40 9000
#define CNK48 14000
#define CNK56 700
#define CNK64 700
#define CNK72 500
#define CNK80 500
#define CNK88 500
#define CNK96 500
#define CNK104 500
#define CNK112 500
#define CNK120 500
#define CNK128 500

#define MSK 0X8000000000000000ULL

struct sail_u {
  uint8_t def_nh;
  struct sail_level level16, level24, level32, level40, level48, level56, level64, level72, level80, level88, level96, level104, level112, level120, level128; 
};

struct sail_u sail_u;

int sail_u_init () {
  int err = 0;

  memset(&sail_u, 0, sizeof(sail_u));
  err = sail_level_init (&sail_u.level16, 16, CNK16, CNK_8, NULL);
  err = sail_level_init (&sail_u.level24, 24, CNK24, CNK_8, &sail_u.level16);
  err = sail_level_init (&sail_u.level32, 32, CNK32, CNK_8, &sail_u.level24);
  err = sail_level_init (&sail_u.level40, 40, CNK40, CNK_8, &sail_u.level32);
  err = sail_level_init (&sail_u.level48, 48, CNK48, CNK_8, &sail_u.level40);
  err = sail_level_init (&sail_u.level56, 56, CNK56, CNK_8, &sail_u.level48);
  err = sail_level_init (&sail_u.level64, 64, CNK64, CNK_8, &sail_u.level56);
  err = sail_level_init (&sail_u.level72, 72, CNK72, CNK_8, &sail_u.level64);
  err = sail_level_init (&sail_u.level80, 80, CNK80, CNK_8, &sail_u.level72);
  err = sail_level_init (&sail_u.level88, 88, CNK88, CNK_8, &sail_u.level80);
  err = sail_level_init (&sail_u.level96, 96, CNK96, CNK_8, &sail_u.level88);
  err = sail_level_init (&sail_u.level104, 104, CNK104, CNK_8, &sail_u.level96);
  err = sail_level_init (&sail_u.level112, 112, CNK112, CNK_8, &sail_u.level104);
  err = sail_level_init (&sail_u.level120, 120, CNK120, CNK_8, &sail_u.level112);
  err = sail_level_init (&sail_u.level128, 128, CNK128, CNK_8, &sail_u.level120);
  //level 16 is always populated
  sail_u.level16.count = CNK16;

  return err;
}

int sail_u_cleanup() {
  sail_level_cleanup (&sail_u.level16);
  sail_level_cleanup (&sail_u.level24);
  sail_level_cleanup (&sail_u.level32);
  sail_level_cleanup (&sail_u.level40);
  sail_level_cleanup (&sail_u.level48);
  sail_level_cleanup (&sail_u.level56);
  sail_level_cleanup (&sail_u.level64);
  sail_level_cleanup (&sail_u.level72);
  sail_level_cleanup (&sail_u.level80);
  sail_level_cleanup (&sail_u.level88);
  sail_level_cleanup (&sail_u.level96);
  sail_level_cleanup (&sail_u.level104);
  sail_level_cleanup (&sail_u.level112);
  sail_level_cleanup (&sail_u.level120);
  sail_level_cleanup (&sail_u.level128);
  memset(&sail_u, 0, sizeof(sail_u));
}

//Calculate memory in MB
double calc_sail_u_mem() {
  return (mem_size (&sail_u.level16) + mem_size (&sail_u.level24) + mem_size (&sail_u.level32) + mem_size (&sail_u.level40) + mem_size (&sail_u.level48) +
         mem_size (&sail_u.level56) + mem_size (&sail_u.level64) + mem_size (&sail_u.level72) + mem_size (&sail_u.level80) + mem_size (&sail_u.level88) +
         mem_size (&sail_u.level96) + mem_size (&sail_u.level104) + mem_size (&sail_u.level112) + mem_size (&sail_u.level120) + mem_size (&sail_u.level128)) / (1024*1024);
}

static int insert_leaf(struct sail_level *c, uint32_t idx, int prefix_len, int nexthop)
{
  register uint32_t num_leafs;/*Number of leafs need to be inserted for this prefix*/

  if (prefix_len > c->level_num || (c->parent != NULL && prefix_len <= c->parent->level_num )) {
    puts ("Invalid level");
    return -1;
  }

  //level pushing
  num_leafs = 1U << (c->level_num - prefix_len);
  for (int i = 0; i < num_leafs; i++) {
    /*Longer prefix exists*/
    if (c->P[idx + i] > prefix_len)
      continue;
    c->N[idx + i] = nexthop;
    c->P[idx + i] = prefix_len;
  }
  return 0;
}

int sail_u_insert(__uint128_t key, int prefix_len, int nexthop) {
  register uint32_t chunk_id = 0;
  register uint32_t idx;
  register int err = 0;
  
  //nexthop cannot be 0. We use 0 to indicate that next-hop doesn't exist.
  if (!nexthop) {
    puts ("nexthop cannot be 0. Please fix the routing table");
    exit (1);
  }

  if (prefix_len == 0) {
    sail_u.def_nh = nexthop;
    goto finish;
  }

  /*Eextract 16 bits from MSB.*/
  idx = key >> 112;
  if (prefix_len <= 16) {
    insert_leaf(&sail_u.level16, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level16, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 104) & 0XFF);
  if (prefix_len <= 24) {
    insert_leaf(&sail_u.level24, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level24, idx);        
  idx = (chunk_id - 1) * CNK_8 + ((key >> 96) & 0XFF);
  if (prefix_len <= 32) {
    insert_leaf(&sail_u.level32, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level32, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 88) & 0XFF);
  if (prefix_len <= 40) {
    insert_leaf(&sail_u.level40, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level40, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 80) & 0XFF);
  if (prefix_len <= 48) {
    insert_leaf(&sail_u.level48, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level48, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 72) & 0XFF);
  if (prefix_len <= 56) {
    insert_leaf(&sail_u.level56, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level56, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 64) & 0XFF);
  if (prefix_len <= 64) {
    insert_leaf(&sail_u.level64, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level64, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 56) & 0XFF);
  if (prefix_len <= 72) {
    insert_leaf(&sail_u.level72, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level72, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 48) & 0XFF);
  if (prefix_len <= 80) {
    insert_leaf(&sail_u.level80, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level80, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 40) & 0XFF);
  if (prefix_len <= 88) {
    insert_leaf(&sail_u.level88, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level88, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 32) & 0XFF);
  if (prefix_len <= 96) {
    insert_leaf(&sail_u.level96, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level96, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 24) & 0XFF);
  if (prefix_len <= 104) {
    insert_leaf(&sail_u.level104, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level104, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 16) & 0XFF);
  if (prefix_len <= 112) {
    insert_leaf(&sail_u.level112, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level112, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 8) & 0XFF);
  if (prefix_len <= 120) {
    insert_leaf(&sail_u.level120, idx, prefix_len, nexthop);
    goto finish;
  }
  
  chunk_id = get_chunk_id_frm_parent (&sail_u.level120, idx);
  idx = (chunk_id - 1) * CNK_8 + (key & 0XFF);
  if (prefix_len <= 128) {
    insert_leaf(&sail_u.level128, idx, prefix_len, nexthop);
    goto finish;
  }
error:
  puts("Something went wrong in route insertion");
  return -1;    
finish:
  return 0;

}

uint8_t sail_u_lookup(__uint128_t key) {
  register uint32_t idx;
  register uint8_t nh = sail_u.def_nh;

  /*extract 16 bits from MSB*/
  idx = key >> 112;

  /*Find corresponding next-hop in level 16*/
  if (sail_u.level16.N[idx] != 0)
    nh = sail_u.level16.N[idx];

  /*Check if there is a longer prefix; if yes, extract bit 17~32
   *  and calculate index to N32
   */
  if (sail_u.level16.C[idx] != 0) {
    idx = (sail_u.level16.C[idx] - 1) * CNK_8 + ((key >> 104) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level24.N[idx] != 0)
    nh = sail_u.level24.N[idx];
    
  if (sail_u.level24.C[idx] != 0) {
    idx = (sail_u.level24.C[idx] - 1) * CNK_8 + ((key >> 96) & 0XFF);
  } else {
    goto finish;
  }        

  if (sail_u.level32.N[idx] != 0)
    nh = sail_u.level32.N[idx];

  if (sail_u.level32.C[idx] != 0) {
    idx = (sail_u.level32.C[idx] - 1) * CNK_8 + ((key >> 88) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level40.N[idx] != 0)
    nh = sail_u.level40.N[idx];

  if (sail_u.level40.C[idx] != 0) {
    idx = (sail_u.level40.C[idx] - 1) * CNK_8 + ((key >> 80) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level48.N[idx] != 0)
    nh = sail_u.level48.N[idx];

  if (sail_u.level48.C[idx] != 0) {
    idx = (sail_u.level48.C[idx] - 1) * CNK_8 + ((key >> 72) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level56.N[idx] != 0)
    nh = sail_u.level56.N[idx];

  if (sail_u.level56.C[idx] != 0) {
    idx = (sail_u.level56.C[idx] - 1) * CNK_8 + ((key >> 64) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level64.N[idx] != 0)
    nh = sail_u.level64.N[idx];

  if (sail_u.level64.C[idx] != 0)
    idx = (sail_u.level64.C[idx] - 1) * CNK_8 + ((key >> 56) & 0XFF);
  else
    goto finish;

  if (sail_u.level72.N[idx] != 0)
    nh = sail_u.level72.N[idx];

  if (sail_u.level72.C[idx] != 0)
    idx = (sail_u.level72.C[idx] - 1) * CNK_8 + ((key >> 48) & 0XFF);
  else
    goto finish;

  if (sail_u.level80.N[idx] != 0)
    nh = sail_u.level80.N[idx];

  if (sail_u.level80.C[idx] != 0)
    idx = (sail_u.level80.C[idx] - 1) * CNK_8 + ((key >> 40) & 0XFF);
  else
    goto finish;

  if (sail_u.level88.N[idx] != 0)
    nh = sail_u.level88.N[idx];

  if (sail_u.level88.C[idx] != 0)
    idx = (sail_u.level88.C[idx] - 1) * CNK_8 + ((key >> 32) & 0XFF);
  else
    goto finish;

  if (sail_u.level96.N[idx] != 0)
    nh = sail_u.level96.N[idx];

  if (sail_u.level96.C[idx] != 0)
    idx = (sail_u.level96.C[idx] - 1) * CNK_8 + ((key >> 24) & 0XFF);
  else
    goto finish;

  if (sail_u.level104.N[idx] != 0)
    nh = sail_u.level104.N[idx];

  if (sail_u.level104.C[idx] != 0)
    idx = (sail_u.level104.C[idx] - 1) * CNK_8 + ((key >> 16) & 0XFF);
  else
    goto finish;

  if (sail_u.level112.N[idx] != 0)
    nh = sail_u.level112.N[idx];

  if (sail_u.level112.C[idx] != 0)
    idx = (sail_u.level112.C[idx] - 1) * CNK_8 + ((key >> 8) & 0XFF);
  else
    goto finish;

  if (sail_u.level120.N[idx] != 0)
    nh = sail_u.level120.N[idx];

  if (sail_u.level120.C[idx] != 0)
    idx = (sail_u.level120.C[idx] - 1) * CNK_8 + (key & 0XFF);
  else
    goto finish;

  if (sail_u.level128.N[idx] != 0)
    nh = sail_u.level128.N[idx];
        
finish:
  return nh;
}

//This is same as FIB lookup, except it returns matched prefix length instead
//of next-hop index
uint8_t sail_u_matched_prefix_len(__uint128_t key) {
  register uint32_t idx;
  int k = 16;

  /*extract 16 bits from MSB*/
  idx = key >> 112;

  /*Find corresponding next-hop in level 16*/
  if (sail_u.level16.N[idx] != 0) {
    k = 16;
  }

  /*Check if there is a longer prefix; if yes, extract bit 17~32
   *  and calculate index to N32
   */
  if (sail_u.level16.C[idx] != 0) {
    idx = (sail_u.level16.C[idx] - 1) * CNK_8 + ((key >> 104) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level24.N[idx] != 0) {
    k = 24;
  }
    
  if (sail_u.level24.C[idx] != 0) {
    idx = (sail_u.level24.C[idx] - 1) * CNK_8 + ((key >> 96) & 0XFF);
  } else {
    goto finish;
  }        

  if (sail_u.level32.N[idx] != 0) {
    k = 32;
  }

  if (sail_u.level32.C[idx] != 0) {
    idx = (sail_u.level32.C[idx] - 1) * CNK_8 + ((key >> 88) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level40.N[idx] != 0) {
    k = 40;
  }

  if (sail_u.level40.C[idx] != 0) {
    idx = (sail_u.level40.C[idx] - 1) * CNK_8 + ((key >> 80) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level48.N[idx] != 0) {
    k = 48;
  }
  if (sail_u.level48.C[idx] != 0) {
    idx = (sail_u.level48.C[idx] - 1) * CNK_8 + ((key >> 72) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level56.N[idx] != 0) {
    k = 56;
  }

  if (sail_u.level56.C[idx] != 0) {
    idx = (sail_u.level56.C[idx] - 1) * CNK_8 + ((key >> 64) & 0XFF);
  } else {
    goto finish;
  }

  if (sail_u.level64.N[idx] != 0) {
    k = 64;
  }

  if (sail_u.level64.C[idx] != 0)
    idx = (sail_u.level64.C[idx] - 1) * CNK_8 + ((key >> 56) & 0XFF);
  else
    goto finish;

  if (sail_u.level72.N[idx] != 0) {
    k = 72;
  }

  if (sail_u.level72.C[idx] != 0)
    idx = (sail_u.level72.C[idx] - 1) * CNK_8 + ((key >> 48) & 0XFF);
  else
    goto finish;

  if (sail_u.level80.N[idx] != 0) {
    k = 80;
  }

  if (sail_u.level80.C[idx] != 0)
    idx = (sail_u.level80.C[idx] - 1) * CNK_8 + ((key >> 40) & 0XFF);
  else
    goto finish;

  if (sail_u.level88.N[idx] != 0) {
    k = 88;
  }

  if (sail_u.level88.C[idx] != 0)
    idx = (sail_u.level88.C[idx] - 1) * CNK_8 + ((key >> 32) & 0XFF);
  else
    goto finish;

  if (sail_u.level96.N[idx] != 0) {
    k = 96;
  }

  if (sail_u.level96.C[idx] != 0)
    idx = (sail_u.level96.C[idx] - 1) * CNK_8 + ((key >> 24) & 0XFF);
  else
    goto finish;

  if (sail_u.level104.N[idx] != 0) {
    k = 104;
  }

  if (sail_u.level104.C[idx] != 0)
    idx = (sail_u.level104.C[idx] - 1) * CNK_8 + ((key >> 16) & 0XFF);
  else
    goto finish;

  if (sail_u.level112.N[idx] != 0) {
    k = 112;
  }

  if (sail_u.level112.C[idx] != 0)
    idx = (sail_u.level112.C[idx] - 1) * CNK_8 + ((key >> 8) & 0XFF);
  else
    goto finish;

  if (sail_u.level120.N[idx] != 0) {
    k = 120;
  }

  if (sail_u.level120.C[idx] != 0)
    idx = (sail_u.level120.C[idx] - 1) * CNK_8 + (key & 0XFF);
  else
    goto finish;

  if (sail_u.level128.N[idx] != 0) {
    k = 128;
  }
        
finish:
  return k;
}
