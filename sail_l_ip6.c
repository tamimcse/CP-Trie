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
#include "sail_l_ip6.h"
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

//forward declaration
int _sail_l_insert(__uint128_t key, int prefix_len, int nexthop, int level);

struct sail_l {
  uint8_t def_nh;
  struct sail_level level16, level24, level32, level40, level48, level56, level64, level72, level80, level88, level96, level104, level112, level120, level128; 
};

struct sail_l sail_l;

int sail_l_init () {
  int err = 0;

  memset(&sail_l, 0, sizeof(sail_l));
  err = sail_level_init (&sail_l.level16, 16, CNK16, CNK_8, NULL);
  err = sail_level_init (&sail_l.level24, 24, CNK24, CNK_8, &sail_l.level16);
  err = sail_level_init (&sail_l.level32, 32, CNK32, CNK_8, &sail_l.level24);
  err = sail_level_init (&sail_l.level40, 40, CNK40, CNK_8, &sail_l.level32);
  err = sail_level_init (&sail_l.level48, 48, CNK48, CNK_8, &sail_l.level40);
  err = sail_level_init (&sail_l.level56, 56, CNK56, CNK_8, &sail_l.level48);
  err = sail_level_init (&sail_l.level64, 64, CNK64, CNK_8, &sail_l.level56);
  err = sail_level_init (&sail_l.level72, 72, CNK72, CNK_8, &sail_l.level64);
  err = sail_level_init (&sail_l.level80, 80, CNK80, CNK_8, &sail_l.level72);
  err = sail_level_init (&sail_l.level88, 88, CNK88, CNK_8, &sail_l.level80);
  err = sail_level_init (&sail_l.level96, 96, CNK96, CNK_8, &sail_l.level88);
  err = sail_level_init (&sail_l.level104, 104, CNK104, CNK_8, &sail_l.level96);
  err = sail_level_init (&sail_l.level112, 112, CNK112, CNK_8, &sail_l.level104);
  err = sail_level_init (&sail_l.level120, 120, CNK120, CNK_8, &sail_l.level112);
  err = sail_level_init (&sail_l.level128, 128, CNK128, CNK_8, &sail_l.level120);
  //level 16 is always populated
  sail_l.level16.count = CNK16;

  return err;
}

int sail_l_cleanup() {
  sail_level_cleanup (&sail_l.level16);
  sail_level_cleanup (&sail_l.level24);
  sail_level_cleanup (&sail_l.level32);
  sail_level_cleanup (&sail_l.level40);
  sail_level_cleanup (&sail_l.level48);
  sail_level_cleanup (&sail_l.level56);
  sail_level_cleanup (&sail_l.level64);
  sail_level_cleanup (&sail_l.level72);
  sail_level_cleanup (&sail_l.level80);
  sail_level_cleanup (&sail_l.level88);
  sail_level_cleanup (&sail_l.level96);
  sail_level_cleanup (&sail_l.level104);
  sail_level_cleanup (&sail_l.level112);
  sail_level_cleanup (&sail_l.level120);
  sail_level_cleanup (&sail_l.level128);
  memset(&sail_l, 0, sizeof(sail_l));
}

//Calculate memory in MB
double calc_sail_l_mem() {
  return (mem_size (&sail_l.level16) + mem_size (&sail_l.level24) + mem_size (&sail_l.level32) + mem_size (&sail_l.level40) + mem_size (&sail_l.level48) +
         mem_size (&sail_l.level56) + mem_size (&sail_l.level64) + mem_size (&sail_l.level72) + mem_size (&sail_l.level80) + mem_size (&sail_l.level88) +
         mem_size (&sail_l.level96) + mem_size (&sail_l.level104) + mem_size (&sail_l.level112) + mem_size (&sail_l.level120) + mem_size (&sail_l.level128)) / (1024*1024);
}

static int insert_leaf(struct sail_level *c, uint32_t idx, int level, __uint128_t key, int prefix_len, int nexthop)
{
  //Level pushing prefixes
  __uint128_t lp_prefixes[256];
  register int lp_count = 0;
  /*Number of leafs need to be inserted for this prefix*/
  register uint32_t num_leafs;
  register __uint128_t matching_key;
  int i, j;

  //level pushing
  num_leafs = 1U << (c->level_num - level);
  for (i = 0; i < num_leafs; i++) {
    //The prefix should be pushed to upper level
    if (c->C[idx + i]) {
      //Prefix that needs to be pushed to next level
      lp_prefixes[lp_count++] = ((key >> (128 - c->level_num)) + i) << (128 - c->level_num);
    } else {
      /*Longer prefix exists*/
      if (c->P[idx + i] > prefix_len)
        continue;
      c->N[idx + i] = nexthop;
      c->P[idx + i] = prefix_len;
    }
  }

  //Leaf pushing
  if (c->chield) {
    for (i = 0; i < lp_count; i++) {
      matching_key = ((lp_prefixes[i] >> (128 - c->chield->level_num)) + (0 << 7)) << (128 - c->chield->level_num);
      _sail_l_insert (matching_key, prefix_len , nexthop, c->level_num + 1);
      matching_key = ((lp_prefixes[i] >> (128 - c->chield->level_num)) + (1 << 7)) << (128 - c->chield->level_num);
      _sail_l_insert (matching_key, prefix_len , nexthop, c->level_num + 1);
    }
  }
  return 0;
}

//Checks if there a leaf in the level; if yes, push it to the next level.
static int leaf_pushing(struct sail_level *c, uint32_t idx, int level, __uint128_t key) {
  register uint8_t next_hop, prefix_len;
  register int i;
  register __uint128_t matching_key;

  //Matching leaf found, so need to push it to the next level
  if (c->N[idx] && c->chield) {
    next_hop = c->N[idx];
    prefix_len = c->P[idx];
    //Set the entry to 0
    c->N[idx] = 0;
    c->P[idx] = 0;
    //Key to which the match was found and add 8 bits to the right
    matching_key = (key >> (128 - c->level_num)) << 8;
    _sail_l_insert ((matching_key + (0 << 7)) << (120 - c->level_num), prefix_len, next_hop, c->level_num + 1);
    _sail_l_insert ((matching_key + (1 << 7)) << (120 - c->level_num), prefix_len, next_hop, c->level_num + 1);
  }
  return 0;
}

int sail_l_insert(__uint128_t key, int prefix_len, int nexthop) {
  //nexthop cannot be 0. We use 0 to indicate that next-hop doesn't exist.
  if (!nexthop) {
    puts ("nexthop cannot be 0. Please fix the routing table");
    exit (1);
  }
  //level is same as prefix len
  return _sail_l_insert(key, prefix_len, nexthop, prefix_len);
}

//This function will be called by sail_l_insert() and by itself recursively for
//leaf pushing. When called by sail_l_insert(), level and prefix length should
//be same. When called recursively, level will be higher than the prefix length.
int _sail_l_insert(__uint128_t key, int prefix_len, int nexthop, int level) {
  register uint32_t chunk_id = 0;
  //Index to N and C array at each level
  register uint32_t idx;
  register int err = 0;

  if (prefix_len == 0) {
    sail_l.def_nh = nexthop;
    goto finish;
  }

  /*Eextract 16 bits from MSB.*/
  idx = key >> 112;
  if (level <= 16) {
    insert_leaf(&sail_l.level16, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level16, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level16, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 104) & 0XFF);
  if (level <= 24) {
    insert_leaf(&sail_l.level24, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level24, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level24, idx);        
  idx = (chunk_id - 1) * CNK_8 + ((key >> 96) & 0XFF);
  if (level <= 32) {
    insert_leaf(&sail_l.level32, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level32, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level32, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 88) & 0XFF);
  if (level <= 40) {
    insert_leaf(&sail_l.level40, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level40, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level40, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 80) & 0XFF);
  if (level <= 48) {
    insert_leaf(&sail_l.level48, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level48, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level48, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 72) & 0XFF);
  if (level <= 56) {
    insert_leaf(&sail_l.level56, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level56, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level56, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 64) & 0XFF);
  if (level <= 64) {
    insert_leaf(&sail_l.level64, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level64, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level64, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 56) & 0XFF);
  if (level <= 72) {
    insert_leaf(&sail_l.level72, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level72, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level72, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 48) & 0XFF);
  if (level <= 80) {
    insert_leaf(&sail_l.level80, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level80, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level80, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 40) & 0XFF);
  if (level <= 88) {
    insert_leaf(&sail_l.level88, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level88, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level88, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 32) & 0XFF);
  if (level <= 96) {
    insert_leaf(&sail_l.level96, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level96, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level96, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 24) & 0XFF);
  if (level <= 104) {
    insert_leaf(&sail_l.level104, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level104, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level104, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 16) & 0XFF);
  if (level <= 112) {
    insert_leaf(&sail_l.level112, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level112, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level112, idx);
  idx = (chunk_id - 1) * CNK_8 + ((key >> 8) & 0XFF);
  if (level <= 120) {
    insert_leaf(&sail_l.level120, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
  leaf_pushing(&sail_l.level120, idx, level, key);

  chunk_id = get_chunk_id_frm_parent (&sail_l.level120, idx);
  idx = (chunk_id - 1) * CNK_8 + (key & 0XFF);
  if (level <= 128) {
    insert_leaf(&sail_l.level128, idx, level, key, prefix_len, nexthop);
    goto finish;
  }
error:
  puts("Something went wrong in route insertion");
  return -1;    
finish:
  return 0;

}

uint8_t sail_l_lookup(__uint128_t key) {
  register uint32_t idx;
  register uint8_t nh = sail_l.def_nh;

  /*extract 16 bits from MSB*/
  idx = key >> 112;

  if (sail_l.level16.C[idx] != 0) {
    idx = (sail_l.level16.C[idx] - 1) * CNK_8 + ((key >> 104) & 0XFF);
    if (sail_l.level24.C[idx] != 0) {
      idx = (sail_l.level24.C[idx] - 1) * CNK_8 + ((key >> 96) & 0XFF);
      if (sail_l.level32.C[idx] != 0) {
        idx = (sail_l.level32.C[idx] - 1) * CNK_8 + ((key >> 88) & 0XFF);
        if (sail_l.level40.C[idx] != 0) {
          idx = (sail_l.level40.C[idx] - 1) * CNK_8 + ((key >> 80) & 0XFF);
          if (sail_l.level48.C[idx] != 0) {
            idx = (sail_l.level48.C[idx] - 1) * CNK_8 + ((key >> 72) & 0XFF);
            if (sail_l.level56.C[idx] != 0) {
              idx = (sail_l.level56.C[idx] - 1) * CNK_8 + ((key >> 64) & 0XFF);
              if (sail_l.level64.C[idx] != 0) {
                idx = (sail_l.level64.C[idx] - 1) * CNK_8 + ((key >> 56) & 0XFF);
                if (sail_l.level72.C[idx] != 0) {
                  idx = (sail_l.level72.C[idx] - 1) * CNK_8 + ((key >> 48) & 0XFF);
                  if (sail_l.level80.C[idx] != 0) {
                    idx = (sail_l.level80.C[idx] - 1) * CNK_8 + ((key >> 40) & 0XFF);
                    if (sail_l.level88.C[idx] != 0) {
                      idx = (sail_l.level88.C[idx] - 1) * CNK_8 + ((key >> 32) & 0XFF);
                      if (sail_l.level96.C[idx] != 0) {
                        idx = (sail_l.level96.C[idx] - 1) * CNK_8 + ((key >> 24) & 0XFF);
                        if (sail_l.level104.C[idx] != 0) {
                          idx = (sail_l.level104.C[idx] - 1) * CNK_8 + ((key >> 16) & 0XFF);
                          if (sail_l.level112.C[idx] != 0) {
                            idx = (sail_l.level112.C[idx] - 1) * CNK_8 + ((key >> 8) & 0XFF);
                            if (sail_l.level120.C[idx] != 0) {
                              idx = (sail_l.level120.C[idx] - 1) * CNK_8 + (key & 0XFF);
                              if (sail_l.level128.N[idx] != 0)
                                return sail_l.level128.N[idx];
                            } else {
                              if (sail_l.level120.N[idx] != 0)
                                return sail_l.level120.N[idx];
                            }

                          } else {
                            if (sail_l.level112.N[idx] != 0)
                              return sail_l.level112.N[idx];
                          }
                        } else {
                          if (sail_l.level104.N[idx] != 0)
                            return sail_l.level104.N[idx];
                        }
                      } else {
                        if (sail_l.level96.N[idx] != 0)
                          return sail_l.level96.N[idx];
                      }
                    } else {
                      if (sail_l.level88.N[idx] != 0)
                        return sail_l.level88.N[idx];
                    }
                  } else {
                    if (sail_l.level80.N[idx] != 0)
                      return sail_l.level80.N[idx];
                  }
                } else {
                  if (sail_l.level72.N[idx] != 0)
                    return sail_l.level72.N[idx];
                }
              } else {
                if (sail_l.level64.N[idx] != 0)
                  return sail_l.level64.N[idx];
              }
            } else {
              if (sail_l.level56.N[idx] != 0)
                return sail_l.level56.N[idx];
            }
          } else {
            if (sail_l.level48.N[idx] != 0)
              return sail_l.level48.N[idx];
          }
        } else {
          if (sail_l.level40.N[idx] != 0)
            return sail_l.level40.N[idx];
        }
      } else {
        if (sail_l.level32.N[idx] != 0)
          return sail_l.level32.N[idx];
      }
    } else {
      if (sail_l.level24.N[idx] != 0)
        return sail_l.level24.N[idx];
    }
  } else {
    if (sail_l.level16.N[idx] != 0)
      return sail_l.level16.N[idx];
  }
  return nh;
}

//This is same as FIB lookup, except it returns matched prefix length instead
//of next-hop index
uint8_t sail_l_matched_prefix_len(__uint128_t key) {
  register uint32_t idx;
  register uint8_t nh = sail_l.def_nh;

  /*extract 16 bits from MSB*/
  idx = key >> 112;
  if (sail_l.level16.C[idx] != 0) {
    idx = (sail_l.level16.C[idx] - 1) * CNK_8 + ((key >> 104) & 0XFF);
    if (sail_l.level24.C[idx] != 0) {
      idx = (sail_l.level24.C[idx] - 1) * CNK_8 + ((key >> 96) & 0XFF);
      if (sail_l.level32.C[idx] != 0) {
        idx = (sail_l.level32.C[idx] - 1) * CNK_8 + ((key >> 88) & 0XFF);
        if (sail_l.level40.C[idx] != 0) {
          idx = (sail_l.level40.C[idx] - 1) * CNK_8 + ((key >> 80) & 0XFF);
          if (sail_l.level48.C[idx] != 0) {
            idx = (sail_l.level48.C[idx] - 1) * CNK_8 + ((key >> 72) & 0XFF);
            if (sail_l.level56.C[idx] != 0) {
              idx = (sail_l.level56.C[idx] - 1) * CNK_8 + ((key >> 64) & 0XFF);
              if (sail_l.level64.C[idx] != 0) {
                idx = (sail_l.level64.C[idx] - 1) * CNK_8 + ((key >> 56) & 0XFF);
                if (sail_l.level72.C[idx] != 0) {
                  idx = (sail_l.level72.C[idx] - 1) * CNK_8 + ((key >> 48) & 0XFF);
                  if (sail_l.level80.C[idx] != 0) {
                    idx = (sail_l.level80.C[idx] - 1) * CNK_8 + ((key >> 40) & 0XFF);
                    if (sail_l.level88.C[idx] != 0) {
                      idx = (sail_l.level88.C[idx] - 1) * CNK_8 + ((key >> 32) & 0XFF);
                      if (sail_l.level96.C[idx] != 0) {
                        idx = (sail_l.level96.C[idx] - 1) * CNK_8 + ((key >> 24) & 0XFF);
                        if (sail_l.level104.C[idx] != 0) {
                          idx = (sail_l.level104.C[idx] - 1) * CNK_8 + ((key >> 16) & 0XFF);
                          if (sail_l.level112.C[idx] != 0) {
                            idx = (sail_l.level112.C[idx] - 1) * CNK_8 + ((key >> 8) & 0XFF);
                            if (sail_l.level120.C[idx] != 0) {
                              idx = (sail_l.level120.C[idx] - 1) * CNK_8 + (key & 0XFF);                              
                              if (sail_l.level128.N[idx] != 0) {
                                return 128;
                              }
                            } else {
                              if (sail_l.level120.N[idx] != 0) {
                                return 120;
                              }
                            }

                          } else {
                            if (sail_l.level112.N[idx] != 0) {
                              return 112;
                            }
                          }
                        } else {
                          if (sail_l.level104.N[idx] != 0) {
                            return 104;
                          }
                        }
                      } else {
                        if (sail_l.level96.N[idx] != 0) {
                          return 96;
                        }
                      }
                    } else {
                      if (sail_l.level88.N[idx] != 0) {
                        return 88;
                      }
                    }
                  } else {
                    if (sail_l.level80.N[idx] != 0) {
                      return 80;
                    }
                  }
                } else {
                  if (sail_l.level72.N[idx] != 0) {
                    return 72;
                  }
                }
              } else {
                if (sail_l.level64.N[idx] != 0) {
                  return 64;
                }
              }
            } else {
              if (sail_l.level56.N[idx] != 0) {
                return 56;
              }
            }
          } else {
            if (sail_l.level48.N[idx] != 0) {
              return 48;
            }
          }
        } else {
          if (sail_l.level40.N[idx] != 0) {
            return 40;
          }
        }
      } else {
        if (sail_l.level32.N[idx] != 0) {
          return 32;
        }
      }
    } else {
      if (sail_l.level24.N[idx] != 0) {
        return 24;
      }
    }
  } else {
    if (sail_l.level16.N[idx] != 0) {
      return 16;
    }
  }
  return 16;
}
