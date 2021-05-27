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
#include "sail_l_ip6.h"
#include "cptrie_ip6.h"
#include "poptrie_ip6.h"
#include "prefix_distribution.h"
#include "stopwatch.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <inttypes.h>//Needed for "PRIx64" format specifier

//This option checks if our FIB insertion and FIB lookup are working properly.
//In order to verify FIB lookup, it stores results from SAIL-U and matches
//that with results from other lookup algorithms.
//This option should be disabled for actual performance measurement.
//#define TEST

//This option calculates average matched prefix length of each 
//algorithm for each traffic
//This option should be disabled for actual performance measurement.
//#define CALC_MATCHED_PREFIX_LEN

//This option writes traffics to files.
//#define RECORD_TRAFFIC

//Maximum number of prefixes in a FIB.
#define PRE_CNT 110000

//IPs and next-hop results in sequential traffic. Please don't change this, 
//because currently we use only 8 bit long sequential pattern 
#define SEQ_CNT (1ULL << 8)
__uint128_t seq_ips[SEQ_CNT];
#ifdef TEST
  uint8_t seq_res[SEQ_CNT];
#endif

//IPs and next-hop results in random traffic
#define RND_CNT (1ULL << 24)
__uint128_t rnd_ips[RND_CNT];
#ifdef TEST
  uint8_t rnd_res[RND_CNT];
#endif

//IPs in real traffic
#define REAL_CNT  (1ULL << 24)
__uint128_t real_ips[REAL_CNT];
//Actual number of IPs in real traffic
uint64_t real_ip_cnt = 0;
#ifdef TEST
  //Next-hop results for real traffic
  uint8_t real_res[REAL_CNT];
#endif

//# of times a lookup is repeated
#define REPEAT 10
//IPs in Repeated traffic
#define REP_CNT  (1ULL << 24)
__uint128_t rep_ips[REP_CNT];
#ifdef TEST
  //Next-hop results for repeated traffic
  uint8_t rep_res[REP_CNT];
#endif

struct result {
  //Number of prefixes with length 49-64
  uint64_t prefixes_49_64;
  //Number of prefixes with length 65-128
  uint64_t prefixes_65_128;
  //Total number of prefixes
  uint64_t total_prefixes;
  //Results for SAIL_U
  double sail_u_insert_time;
  double sail_u_lookup_time;
  double sail_u_lookup_throughput_real_traffic;
  double sail_u_lookup_throughput_rnd_traffic;
  double sail_u_lookup_throughput_seq_traffic;
  double sail_u_lookup_throughput_pre_traffic;
  double sail_u_lookup_throughput_rep_traffic;
  double sail_u_mem_consumption;
  double sail_u_lookup_cpucycle;
  //Results for SAIL_L
  double sail_l_insert_time;
  double sail_l_lookup_time;
  double sail_l_lookup_throughput_real_traffic;
  double sail_l_lookup_throughput_rnd_traffic;
  double sail_l_lookup_throughput_seq_traffic;
  double sail_l_lookup_throughput_pre_traffic;
  double sail_l_lookup_throughput_rep_traffic;
  double sail_l_mem_consumption;
  double sail_l_lookup_cpucycle;
  //Results for Poptrie
  double poptrie_insert_time;
  double poptrie_lookup_time;
  double poptrie_lookup_throughput_real_traffic;
  double poptrie_lookup_throughput_rnd_traffic;
  double poptrie_lookup_throughput_seq_traffic;
  double poptrie_lookup_throughput_pre_traffic;
  double poptrie_lookup_throughput_rep_traffic;
  double poptrie_mem_consumption;
  double poptrie_lookup_cpucycle;
  //Results for CP-Trie
  double cptrie_insert_time;
  double cptrie_lookup_time;
  double cptrie_lookup_throughput_real_traffic;
  double cptrie_lookup_throughput_rnd_traffic;
  double cptrie_lookup_throughput_seq_traffic;
  double cptrie_lookup_throughput_pre_traffic;
  double cptrie_lookup_throughput_rep_traffic;
  double cptrie_mem_consumption;
  double cptrie_lookup_cpucycle;
};

struct xorshift32_state {
  uint32_t a;
};

/* The state word must be initialized to non-zero */
uint32_t xorshift32(struct xorshift32_state *state)
{
  /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
  uint32_t x = state->a;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return state->a = x;
}

//Creates a 128-bit number from 4 32-bit numbers
#define U32_TO_U128(a,b,c,d) (((__uint128_t)a << 96) | ((__uint128_t)b << 64) |\
                          ((__uint128_t)c << 32) | (__uint128_t)d)

struct xorshift128_state {
  uint32_t a, b, c, d;
};

/* The state array must be initialized to not be all zero */
uint32_t xorshift128(struct xorshift128_state *state)
{
  /* Algorithm "xor128" from p. 5 of Marsaglia, "Xorshift RNGs" */
  uint32_t t = state->d;

  uint32_t const s = state->a;
  state->d = state->c;
  state->c = state->b;
  state->b = s;

  t ^= t << 11;
  t ^= t >> 8;
  return state->a = t ^ s ^ (s >> 19);
}

__uint128_t xorshift_to_ipv6 (struct xorshift128_state *s) {
    return U32_TO_U128(s->a, s->b, s->c, s->d);
}

//Converts 128-bit IPv6 to a string of hex characters
static char* ipv6_to_str (__uint128_t ip) {
  //An IPv6 address needs 32 character + 1 for NULL
  char *str = new char[33];
  //We print __uint128_t by breaking into two uint64_t variables. We 
  //use %016"PRIx64" format specifier to print a uint64_t.
  sprintf(str, "%016"PRIx64"%016"PRIx64"", (uint64_t)(ip >> 64), (uint64_t)ip);
  return str;
}

static __inline__ __uint128_t in6_addr_to_uint128(struct in6_addr *in6)
{
  __uint128_t a;
  int i;

  a = 0;
  for ( i = 0; i < 16; i++ ) {
    a <<= 8;
    a |= in6->s6_addr[i];
  }
  return a;
}

int test(char *file, struct result *res){
  //As this variable is used during FIB lookup, make it register
  //It improves lookup performance siginificantly
  register long long i = 0, j = 0;
  register uint8_t nh;
  //Number of IPs in PRE traffic
  register uint64_t prefix_cnt = 0;
  double delay = 0, cpu_cycles = 0;
  //Prefixes in the FIB
  __uint128_t prefixes[PRE_CNT];
  //Prefix lengths
   uint8_t pre_lens[PRE_CNT];
  //Next-hops for the prefixes
   uint8_t pre_nhs[PRE_CNT];
#ifdef TEST
  //Next-hop results for prefix traffic
  uint8_t pre_res[PRE_CNT];
#endif
  FILE *fp;
  //Must have set to 0. Otherwise it's preallocated with garbage value
  char v6str[256] = {0};
  char buff[4096];
  int prefixlen;
  int nexthop;
  int ret;
  char prefixStr[256];
  struct in6_addr v6addr;

  if ((fp = fopen(file, "r")) == NULL) {
    puts("File not exists");
    return -1;
  }

  printf("Reading FIB from file %s ....... \n", file);
  
  while ( !feof(fp) ) {
    if ( !fgets(buff, sizeof(buff), fp) )
      continue;
    prefixlen = 0;
    nexthop = 0;
    ret = sscanf(buff, "%255[^'/']/%d\t%d", v6str, &prefixlen, &nexthop);
    if ( ret < 0 ) {
      puts ("The input file is not formatted properly");
      return -1;
    }
//    printf ("%s/%d %d\n", v6str, prefixlen, nexthop);

    ret = inet_pton(AF_INET6, v6str, &v6addr);
    if ( ret != 1 ) {
      puts ("Invalid IPv6 prefix");
      return -1;
    }

    //TODO: Make it dynamic
    if (prefix_cnt >= PRE_CNT) {
      puts ("The PRE traffic array is full. Please increase the array size");
      return -1;
    }

    //Store prefix info
    prefixes[prefix_cnt] = in6_addr_to_uint128(&v6addr);
    pre_lens[prefix_cnt] = prefixlen;
    pre_nhs[prefix_cnt] = nexthop;
    prefix_cnt++;

   //Record the prefix counts in results
    res->total_prefixes++;
    if (prefixlen >= 49 && prefixlen <= 64)
      res->prefixes_49_64++;
    else if (prefixlen >= 65 && prefixlen <= 128)
      res->prefixes_65_128++;
    
    //record prefix distribution across all the FIBs
    record_prefix_len (prefixlen);
  }
  
  printf("-----------------Generating repeated traffic-------------- \n");
  struct xorshift32_state rnd_idx = {1};
  xorshift128_state rnd_ip = {1, 1, 1, 1};
  
  for (i = 0; i < REP_CNT; i++) {
    //generate a 32-bit random number
    xorshift32(&rnd_idx);
    //generate a 128-bit random number
    xorshift128(&rnd_ip);
    //select an IP from prefix traffic
    uint32_t ix = rnd_idx.a % prefix_cnt;
    //Add the new random number to the right of the prefix to generate the IP
    rep_ips[i] = prefixes[ix] | (xorshift_to_ipv6(&rnd_ip) >>  pre_lens[ix]);

//    printf("%s\n%s\n%d\n\n", ipv6_to_str(prefixes[ix]), ipv6_to_str(rep_ips[i]), pre_lens[ix]);
  }
  
  printf("-----------------Generating sequential traffic-------------- \n");
  //Find the longest prefix
  __uint128_t max_prefix = 0;
  uint8_t max_prefix_len = 0;
  for (i = 0; i < prefix_cnt; i++) {
    if (pre_lens[i] > max_prefix_len) {
      max_prefix_len = pre_lens[i];
      max_prefix = prefixes[i];
    } 
  }
  
  uint8_t prexlen_seq_traffic =  (ceil(max_prefix_len/8.0) - 1) * 8;
  printf ("generating sequential traffic withing %s/%d\n", ipv6_to_str(max_prefix), prexlen_seq_traffic);
  for (i = 0; i < SEQ_CNT; i++) {
    seq_ips[i] = (((max_prefix >> (128 - prexlen_seq_traffic)) << 8) | ((__uint128_t)i)) << (128 - (prexlen_seq_traffic + 8));
//    printf ("sequential IP = %s \n", ipv6_to_str(seq_ips[i]));
  }

  printf("---------------------Checking SAIL-U-------------------------- \n");

  ret = sail_u_init();
  if (ret < 0) {
    puts("Failed to initialize SAIL-U");
    sail_u_cleanup();
    return -1;
  }

  //Insrting into SAIL-U
  stopwatch_start();
  for (i = 0; i < prefix_cnt; i++) {
    ret = sail_u_insert(prefixes[i], pre_lens[i], pre_nhs[i]);
//We don't want to include this while measuring performance
#ifdef TEST
    if (ret) {
      sprintf(prefixStr, "%s/%d %d", ipv6_to_str(prefixes[i]), pre_lens[i], pre_nhs[i]);
      printf("Failed to insert %s into SAIL-U \n", prefixStr);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  //Calculate avg. insertion time in microsec
  res->sail_u_insert_time = delay / (1000 * prefix_cnt);
  printf ("SAIL-U insertion time per prefix = %f microsec \n", res->sail_u_insert_time);

  //Calculate memory consumption in MB
  res->sail_u_mem_consumption = calc_sail_u_mem();
  printf ("SAIL-U memory consumption = %f MB \n", res->sail_u_mem_consumption);

  //Lookup for real traffic
  stopwatch_start();
  for (i = 0; i < real_ip_cnt; i++) {
    nh = sail_u_lookup(real_ips[i]);
#ifdef TEST
    real_res[i] = nh;
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_u_lookup_throughput_real_traffic = (real_ip_cnt * 1000) / delay;
  printf ("SAIL-U lookup throughput for real traffic = %f Mlps \n", res->sail_u_lookup_throughput_real_traffic);

  //Lookup for random traffic
  stopwatch_start();
  for (i = 0; i < RND_CNT; i++) {
    nh = sail_u_lookup(rnd_ips[i]);
#ifdef TEST
    rnd_res[i] = nh;
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_u_lookup_throughput_rnd_traffic = (RND_CNT * 1000) / delay;
  printf ("SAIL-U lookup throughput for random traffic = %f Mlps \n", res->sail_u_lookup_throughput_rnd_traffic);

  //Lookup for sequential traffic
  stopwatch_start();
  for (i = 0; i < SEQ_CNT; i++) {
    nh = sail_u_lookup(seq_ips[i]);
#ifdef TEST
    seq_res[i] = nh;
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_u_lookup_throughput_seq_traffic = (SEQ_CNT * 1000) / delay;
  printf ("SAIL-U lookup throughput for sequential traffic = %f Mlps \n", res->sail_u_lookup_throughput_seq_traffic);

  //Lookup for prefix traffic
  stopwatch_start();
  for (i = 0; i < prefix_cnt; i++) {
    nh = sail_u_lookup(prefixes[i]);
#ifdef TEST
    pre_res[i] = nh;
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_u_lookup_time = delay/prefix_cnt;
  res->sail_u_lookup_throughput_pre_traffic = (prefix_cnt * 1000) / delay;
  res->sail_u_lookup_cpucycle = cpu_cycles/prefix_cnt;
  printf ("SAIL-U lookup throughput for prefix traffic = %f Mlps \n", res->sail_u_lookup_throughput_pre_traffic);
  
  //Lookup for repeated traffic
  stopwatch_start();
  for (i = 0; i < REP_CNT; i++) {
      for (j = 0; j < REPEAT; j++)
        nh = sail_u_lookup(rep_ips[i]);
#ifdef TEST
      rep_res[i] = nh;
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_u_lookup_time = delay/(REP_CNT * REPEAT);
  res->sail_u_lookup_throughput_rep_traffic = (REP_CNT * REPEAT * 1000) / delay;
  res->sail_u_lookup_cpucycle = cpu_cycles/(REP_CNT * REPEAT);
  printf ("SAIL-U lookup throughput for repeated traffic = %f Mlps \n", res->sail_u_lookup_throughput_rep_traffic);

  sail_u_cleanup();

  printf("---------------------Checking SAIL-L-------------------------- \n");

  ret = sail_l_init();
  if (ret < 0) {
    puts("Failed to initialize SAIL-L");
    sail_l_cleanup();
    return -1;
  }

  //Insrting into SAIL-L
  stopwatch_start();
  for (i = 0; i < prefix_cnt; i++) {
    ret = sail_l_insert(prefixes[i], pre_lens[i], pre_nhs[i]);
#ifdef TEST
    if (ret) {
      sprintf(prefixStr, "%s/%d %d", ipv6_to_str(prefixes[i]), pre_lens[i], pre_nhs[i]);
      printf("Failed to insert %s into SAIL-L \n", prefixStr);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  //Calculate avg. insertion time in microsec
  res->sail_l_insert_time = delay / (1000 * prefix_cnt);
  printf ("SAIL-L insertion time per prefix = %f microsec \n", res->sail_l_insert_time);

  //Calculate memory consumption in MB
  res->sail_l_mem_consumption = calc_sail_l_mem();
  printf ("SAIL-L memory consumption = %f MB \n", res->sail_l_mem_consumption);

  //Lookup for real traffic
  stopwatch_start();
  for (i = 0; i < real_ip_cnt; i++) {
    nh = sail_l_lookup(real_ips[i]);
#ifdef TEST
    if (nh != real_res[i]) {
      printf("IP = %s\n", ipv6_to_str(real_ips[i]));
      printf ("SAIL-U next-hop = %d\n", real_res[i]);
      printf ("SAIL-L next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_l_lookup_throughput_real_traffic = (real_ip_cnt * 1000) / delay;
  printf ("SAIL-L lookup throughput for real traffic = %f Mlps \n", res->sail_l_lookup_throughput_real_traffic);

  //Lookup for random traffic
  stopwatch_start();
  for (i = 0; i < RND_CNT; i++) {
    nh = sail_l_lookup(rnd_ips[i]);
#ifdef TEST
    if (nh != rnd_res[i]) {
      printf("IP = %s\n", ipv6_to_str(rnd_ips[i]));
      printf ("SAIL-U next-hop = %d\n", rnd_res[i]);
      printf ("SAIL-L next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_l_lookup_throughput_rnd_traffic = (RND_CNT * 1000) / delay;
  printf ("SAIL-L lookup throughput for random traffic = %f Mlps \n", res->sail_l_lookup_throughput_rnd_traffic);

  //Lookup for sequential traffic
  stopwatch_start();
  for (i = 0; i < SEQ_CNT; i++) {
    nh = sail_l_lookup(seq_ips[i]);
#ifdef TEST
    if (nh != seq_res[i]) {
      printf("IP = %s \n", ipv6_to_str(seq_ips[i]));
      printf ("SAIL-U next-hop = %d\n", seq_res[i]);
      printf ("SAIL-L next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_l_lookup_throughput_seq_traffic = (SEQ_CNT * 1000) / delay;
  printf ("SAIL-L lookup throughput for sequential traffic = %f Mlps \n", res->sail_l_lookup_throughput_seq_traffic);
  
  //Lookup for prefix traffic
  stopwatch_start();
  for (i = 0; i < prefix_cnt; i++) {
    nh = sail_l_lookup(prefixes[i]);
#ifdef TEST
    if (nh != pre_res[i]) {
      printf("IP = %s\n", ipv6_to_str(prefixes[i]));
      printf ("SAIL-U next-hop = %d\n", pre_res[i]);
      printf ("SAIL-L next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_l_lookup_time = delay/prefix_cnt;
  res->sail_l_lookup_throughput_pre_traffic = (prefix_cnt * 1000) / delay;
  res->sail_l_lookup_cpucycle = cpu_cycles/prefix_cnt;
  printf ("SAIL-L lookup throughput for prefix traffic = %f Mlps \n", res->sail_l_lookup_throughput_pre_traffic);
  
  //Lookup for repeated traffic
  stopwatch_start();
  for (i = 0; i < REP_CNT; i++) {
    for (j = 0; j < REPEAT; j++)
      nh = sail_l_lookup(rep_ips[i]);
#ifdef TEST
    if (nh != rep_res[i]) {
      printf("IP = %s\n", ipv6_to_str(rep_ips[i]));
      printf ("SAIL-U next-hop = %d\n", rep_res[i]);
      printf ("SAIL-L next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->sail_l_lookup_time = delay/(REP_CNT * REPEAT);
  res->sail_l_lookup_throughput_rep_traffic = (REP_CNT * REPEAT * 1000) / delay;
  res->sail_l_lookup_cpucycle = cpu_cycles/(REP_CNT * REPEAT);
  printf ("SAIL-L lookup throughput for repeated traffic = %f Mlps \n", res->sail_l_lookup_throughput_rep_traffic);

  sail_l_cleanup();

  printf("---------------------Checking Poptrie-------------------------- \n");

  ret = poptrie_init();
  if (ret < 0) {
    puts("Failed to initialize poptrie");
    poptrie_cleanup();
    return -1;
  }

  //Inserting into Poptrie
  stopwatch_start();
  for (i = 0; i < prefix_cnt; i++) {
    ret = poptrie_insert(prefixes[i], pre_lens[i], pre_nhs[i]);
#ifdef TEST
    if (ret) {
      sprintf(prefixStr, "%s/%d %d", ipv6_to_str(prefixes[i]), pre_lens[i], pre_nhs[i]);
      printf("Failed to insert %s into poptrie \n", prefixStr);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  //Calculate avg. insertion time in microsec
  res->poptrie_insert_time = delay / (1000 * prefix_cnt);
  printf ("Poptrie insertion time per prefix = %f microsec \n", res->poptrie_insert_time);

  //Calculate memory consumption in MB
  res->poptrie_mem_consumption = calc_poptrie_mem();
  printf ("Poptrie memory consumption = %f MB \n", res->poptrie_mem_consumption);

  //Lookup for real traffic
  stopwatch_start();
  for (i = 0; i < real_ip_cnt; i++) {
    nh = poptrie_lookup(real_ips[i]);
#ifdef TEST
    if (nh != real_res[i]) {
      printf("IP = %s\n", ipv6_to_str(real_ips[i]));
      printf ("SAIL-U next-hop = %d\n", real_res[i]);
      printf ("Poptrie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->poptrie_lookup_throughput_real_traffic = (real_ip_cnt * 1000) / delay;
  printf ("Poptrie lookup throughput for real traffic = %f Mlps \n", res->poptrie_lookup_throughput_real_traffic);

  //Lookup for random traffic
  stopwatch_start();
  for (i = 0; i < RND_CNT; i++) {
    nh = poptrie_lookup(rnd_ips[i]);
#ifdef TEST
    if (nh != rnd_res[i]) {
      printf("IP = %s\n", ipv6_to_str(rnd_ips[i]));
      printf ("SAIL-U next-hop = %d\n", rnd_res[i]);
      printf ("Poptrie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->poptrie_lookup_throughput_rnd_traffic = (RND_CNT * 1000) / delay;
  printf ("Poptrie lookup throughput for random traffic = %f Mlps \n", res->poptrie_lookup_throughput_rnd_traffic);

  //Lookup for sequential traffic
  stopwatch_start();
  for (i = 0; i < SEQ_CNT; i++) {
    nh = poptrie_lookup(seq_ips[i]);
#ifdef TEST
    if (nh != seq_res[i]) {
      printf("IP = %s \n", ipv6_to_str(seq_ips[i]));
      printf ("SAIL-U next-hop = %d\n", seq_res[i]);
      printf ("Poptrie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->poptrie_lookup_throughput_seq_traffic = (SEQ_CNT * 1000) / delay;
  printf ("Poptrie lookup throughput for sequential traffic = %f Mlps \n", res->poptrie_lookup_throughput_seq_traffic);
  
  //Lookup for prefix traffic
  stopwatch_start();
  for (i = 0; i < prefix_cnt; i++) {
    nh = poptrie_lookup(prefixes[i]);
#ifdef TEST
    if (nh != pre_res[i]) {
      printf("IP = %s \n", ipv6_to_str(prefixes[i]));
      printf ("SAIL-U next-hop = %d\n", pre_res[i]);
      printf ("Poptrie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->poptrie_lookup_time = delay/prefix_cnt;
  res->poptrie_lookup_throughput_pre_traffic = (prefix_cnt * 1000) / delay;
  res->poptrie_lookup_cpucycle = cpu_cycles/prefix_cnt;
  printf ("Poptrie lookup throughput for prefix traffic = %f Mlps \n", res->poptrie_lookup_throughput_pre_traffic);

  //Lookup for repeated traffic
  stopwatch_start();
  for (i = 0; i < REP_CNT; i++) {
    for (j = 0; j < REPEAT; j++)
      nh = poptrie_lookup(rep_ips[i]);
#ifdef TEST
    if (nh != rep_res[i]) {
      printf("IP = %s \n", ipv6_to_str(rep_ips[i]));
      printf ("SAIL-U next-hop = %d\n", rep_res[i]);
      printf ("Poptrie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->poptrie_lookup_time = delay/(REP_CNT * REPEAT);
  res->poptrie_lookup_throughput_rep_traffic = (REP_CNT * REPEAT * 1000) / delay;
  res->poptrie_lookup_cpucycle = cpu_cycles/(REP_CNT * REPEAT);
  printf ("Poptrie lookup throughput for repeated traffic = %f Mlps \n", res->poptrie_lookup_throughput_rep_traffic);

  poptrie_cleanup();

  printf("---------------------Checking CP-Trie-------------------------- \n");

  ret = cptrie_init();
  if (ret < 0) {
    puts("Failed to initialize CP-Trie");
    cptrie_cleanup();
    return -1;
  }

  //Inserting into CP-Trie
  stopwatch_start();
  for (i = 0; i < prefix_cnt; i++) {
    ret = cptrie_insert(prefixes[i], pre_lens[i], pre_nhs[i]);
#ifdef TEST
    if (ret) {
      sprintf(prefixStr, "%s/%d %d", ipv6_to_str(prefixes[i]), pre_lens[i], pre_nhs[i]);
      printf("Failed to insert %s into CP-Trie \n", prefixStr);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  //Calculate avg. insertion time in microsec
  res->cptrie_insert_time = delay/(1000 * prefix_cnt);
  printf ("CP-Trie insertion time per prefix = %f microsec \n", res->cptrie_insert_time);

  //Calculate memory consumption in MB
  res->cptrie_mem_consumption = calc_cptrie_mem();
  printf ("CP-Trie memory consumption = %f MB \n", res->cptrie_mem_consumption);

  //Lookup for real traffic
  stopwatch_start();
  for (i = 0; i < real_ip_cnt; i++) {
    nh = cptrie_lookup(real_ips[i]);
#ifdef TEST
    if (nh != real_res[i]) {
      printf("IP = %s\n", ipv6_to_str(real_ips[i]));
      printf ("SAIL-U next-hop = %d\n", real_res[i]);
      printf ("CP-Trie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->cptrie_lookup_throughput_real_traffic = (real_ip_cnt * 1000) / delay;
  printf ("CP-Trie lookup throughput for real traffic = %f Mlps \n", res->cptrie_lookup_throughput_real_traffic);

  //Lookup for random traffic
  stopwatch_start();
  for (i = 0; i < RND_CNT; i++) {
    nh = cptrie_lookup(rnd_ips[i]);
#ifdef TEST
    if (nh != rnd_res[i]) {
      printf("IP = %s\n", ipv6_to_str(rnd_ips[i]));
      printf ("SAIL-U next-hop = %d\n", rnd_res[i]);
      printf ("CP-Trie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->cptrie_lookup_throughput_rnd_traffic = (RND_CNT * 1000) / delay;
  printf ("CP-Trie lookup throughput for random traffic = %f Mlps \n", res->cptrie_lookup_throughput_rnd_traffic);

  //Lookup for sequential traffic
  stopwatch_start();
  for (i = 0; i < SEQ_CNT; i++) {
    nh = cptrie_lookup(seq_ips[i]);
#ifdef TEST
    if (nh != seq_res[i]) {
      printf("IP = %s \n", ipv6_to_str(seq_ips[i]));
      printf ("SAIL-U next-hop = %d\n", seq_res[i]);
      printf ("CP-Trie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->cptrie_lookup_throughput_seq_traffic = (SEQ_CNT * 1000) / delay;
  printf ("CP-Trie lookup throughput for sequential traffic = %f Mlps \n", res->cptrie_lookup_throughput_seq_traffic);

  //Lookup for prefix traffic
  stopwatch_start();
  for (i = 0; i < prefix_cnt; i++) {
    nh = cptrie_lookup(prefixes[i]);
#ifdef TEST
    if (nh != pre_res[i]) {
      printf("IP = %s \n", ipv6_to_str(prefixes[i]));
      printf ("SAIL-U next-hop = %d\n", pre_res[i]);
      printf ("CP-Trie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->cptrie_lookup_time = delay/prefix_cnt;
  res->cptrie_lookup_throughput_pre_traffic = (prefix_cnt * 1000) / delay;
  res->cptrie_lookup_cpucycle = cpu_cycles/prefix_cnt;
  printf ("CP-Trie lookup throughput for prefix traffic = %f Mlps \n", res->cptrie_lookup_throughput_pre_traffic);
  
  //Lookup for repeated traffic
  stopwatch_start();
  for (i = 0; i < REP_CNT; i++) {
    for (j = 0; j < REPEAT; j++)
      nh = cptrie_lookup(rep_ips[i]);
#ifdef TEST
    if (nh != rep_res[i]) {
      printf("IP = %s \n", ipv6_to_str(rep_ips[i]));
      printf ("SAIL-U next-hop = %d\n", rep_res[i]);
      printf ("CP-Trie next-hop = %d\n", nh);
      return -1;
    }
#endif
  }
  stopwatch_stop(&delay, &cpu_cycles);
  res->cptrie_lookup_time = delay/(REP_CNT * REPEAT);
  res->cptrie_lookup_throughput_rep_traffic = (REP_CNT * REPEAT * 1000) / delay;
  res->cptrie_lookup_cpucycle = cpu_cycles/(REP_CNT * REPEAT);
  printf ("CP-Trie lookup throughput for repeated traffic = %f Mlps \n", res->cptrie_lookup_throughput_rep_traffic);

  cptrie_cleanup();

  return 0;
}

//Calculates average matched prefix length for different FIBs for
//different trafffics
static int calc_avg_matched_prefix_len(char *file){
  long long i = 0, j = 0;
  //Number of IPs in PRE traffic
  uint64_t prefix_cnt = 0;
  //Prefixes in the FIB
  __uint128_t prefixes[PRE_CNT];
  //Prefix lengths
   uint8_t pre_lens[PRE_CNT];
  //Next-hops for the prefixes
   uint8_t pre_nhs[PRE_CNT];
  FILE *fp;
  char v6str[256];
  char buff[4096];
  int prefixlen;
  int nexthop;
  int ret;
  struct in6_addr v6addr;
  uint64_t sum;

  if ((fp = fopen(file, "r")) == NULL) {
    puts("File not exists");
    return -1;
  }

  printf("\nReading FIB from file %s ....... \n", file);
  
  while ( !feof(fp) ) {
    if ( !fgets(buff, sizeof(buff), fp) )
      continue;
    memset(v6str, 0, sizeof(v6str));
    prefixlen = 0;
    nexthop = 0;
    ret = sscanf(buff, "%255[^'/']/%d\t%d", v6str, &prefixlen, &nexthop);
    if ( ret < 0 ) {
      puts ("The input file is not formatted properly");
      return -1;
    }
//    printf ("%s/%d %d\n", v6str, prefixlen, nexthop);

    ret = inet_pton(AF_INET6, v6str, &v6addr);
    if ( ret != 1 ) {
      puts ("Invalid IPv6 prefix");
      return -1;
    }

    if (prefix_cnt >= PRE_CNT) {
      puts ("The PRE traffic array is full");
      return -1;
    }

    //Store prefix info
    prefixes[prefix_cnt] = in6_addr_to_uint128(&v6addr);
    pre_lens[prefix_cnt] = prefixlen;
    pre_nhs[prefix_cnt] = nexthop;
    prefix_cnt++;
  }

  printf("---------------------SAIL-U-------------------------- \n");

  ret = sail_u_init();
  if (ret < 0) {
    puts("Failed to initialize SAIL-U");
    sail_u_cleanup();
    return -1;
  }

  //Insrting into SAIL-U
  for (i = 0; i < prefix_cnt; i++) {
    ret = sail_u_insert(prefixes[i], pre_lens[i], pre_nhs[i]);
  }

  //Matched prefix length for real traffic
  sum = 0;
  for (i = 0; i < real_ip_cnt; i++) {
    sum += sail_u_matched_prefix_len(real_ips[i]);
  }
  printf ("SAIL-U Real traffic prefix length = %llu\n", sum/real_ip_cnt);

  //Matched prefix length for random traffic
  sum = 0;
  for (i = 0; i < RND_CNT; i++) {
    sum += sail_u_matched_prefix_len(rnd_ips[i]);
  }
  printf ("SAIL-U Random traffic prefix length = %llu\n", sum/RND_CNT);

  //Matched prefix length for sequential traffic
  sum = 0;
  for (i = 0; i < SEQ_CNT; i++) {
    sum += sail_u_matched_prefix_len(seq_ips[i]);
  }
  printf ("SAIL-U Sequencial traffic prefix length = %llu\n", sum/SEQ_CNT);

  //Matched prefix length for prefix traffic
  sum = 0;
  for (i = 0; i < prefix_cnt; i++) {
    sum += sail_u_matched_prefix_len(prefixes[i]);
  }
  printf ("SAIL-U Prefix traffic prefix length = %llu\n", sum/prefix_cnt);
  
  //Matched prefix length for repeated traffic
  sum = 0;
  for (i = 0; i < prefix_cnt; i++) {
      for (j = 0; j < REPEAT; j++)
        sum += sail_u_matched_prefix_len(prefixes[i]);
  }
  printf ("SAIL-U Repeated traffic prefix length = %llu\n", sum/(prefix_cnt * REPEAT));

  sail_u_cleanup();

  printf("---------------------SAIL-L-------------------------- \n");

  ret = sail_l_init();
  if (ret < 0) {
    puts("Failed to initialize SAIL-L");
    sail_l_cleanup();
    return -1;
  }

  //Insrting into SAIL-L
  for (i = 0; i < prefix_cnt; i++) {
    ret = sail_l_insert(prefixes[i], pre_lens[i], pre_nhs[i]);
  }

  //Matched prefix length for real traffic
  sum = 0;
  for (i = 0; i < real_ip_cnt; i++) {
    sum += sail_l_matched_prefix_len(real_ips[i]);
  }
  printf ("SAIL-L Real traffic prefix length = %llu\n", sum/real_ip_cnt);

  //Matched prefix length for random traffic
  sum = 0;
  for (i = 0; i < RND_CNT; i++) {
    sum += sail_l_matched_prefix_len(rnd_ips[i]);
  }
  printf ("SAIL-L Random traffic prefix length = %llu\n", sum/RND_CNT);

  //Matched prefix length for sequential traffic
  sum = 0;
  for (i = 0; i < SEQ_CNT; i++) {
    sum += sail_l_matched_prefix_len(seq_ips[i]);
  }
  printf ("SAIL-L Sequencial traffic prefix length = %llu\n", sum/SEQ_CNT);
  
  //Matched prefix length for prefix traffic
  sum = 0;
  for (i = 0; i < prefix_cnt; i++) {
    sum += sail_l_matched_prefix_len(prefixes[i]);
  }
  printf ("SAIL-L Prefix traffic prefix length = %llu\n", sum/prefix_cnt);
  
  //Matched prefix length for repeated traffic
  sum = 0;
  for (i = 0; i < prefix_cnt; i++) {
    for (j = 0; j < REPEAT; j++)
      sum += sail_l_matched_prefix_len(prefixes[i]);
  }
  printf ("SAIL-L Repeated traffic prefix length = %llu\n", sum/(prefix_cnt * REPEAT));

  sail_l_cleanup();

  printf("---------------------Poptrie-------------------------- \n");

  ret = poptrie_init();
  if (ret < 0) {
    puts("Failed to initialize poptrie");
    poptrie_cleanup();
    return -1;
  }

  //Inserting into Poptrie
  for (i = 0; i < prefix_cnt; i++) {
    ret = poptrie_insert(prefixes[i], pre_lens[i], pre_nhs[i]);
  }

  //Matched prefix length for real traffic
  sum = 0;
  for (i = 0; i < real_ip_cnt; i++) {
    sum += poptrie_matched_prefix_len(real_ips[i]);
  }
  printf ("Poptrie Real traffic prefix length = %llu\n", sum/real_ip_cnt);

  //Matched prefix length for random traffic
  sum = 0;
  for (i = 0; i < RND_CNT; i++) {
    sum += poptrie_matched_prefix_len(rnd_ips[i]);
  }
  printf ("Poptrie Random traffic prefix length = %llu\n", sum/RND_CNT);

  //Matched prefix length for sequential traffic
  sum = 0;
  for (i = 0; i < SEQ_CNT; i++) {
    sum += poptrie_matched_prefix_len(seq_ips[i]);
  }
  printf ("Poptrie Sequencial traffic prefix length = %llu\n", sum/SEQ_CNT);
  
  //Matched prefix length for prefix traffic
  sum = 0;
  for (i = 0; i < prefix_cnt; i++) {
    sum += poptrie_matched_prefix_len(prefixes[i]);
  }
  printf ("Poptrie Prefix traffic prefix length = %llu\n", sum/prefix_cnt);

  //Matched prefix length for repeated traffic
  sum = 0;
  for (i = 0; i < prefix_cnt; i++) {
    for (j = 0; j < REPEAT; j++)
      sum += poptrie_matched_prefix_len(prefixes[i]);
  }
  printf ("Poptrie Repeated traffic prefix length = %llu\n", sum/(prefix_cnt * REPEAT));

  poptrie_cleanup();

  printf("---------------------CP-Trie-------------------------- \n");

  ret = cptrie_init();
  if (ret < 0) {
    puts("Failed to initialize CP-Trie");
    cptrie_cleanup();
    return -1;
  }

  //Inserting into CP-Trie
  for (i = 0; i < prefix_cnt; i++) {
    ret = cptrie_insert(prefixes[i], pre_lens[i], pre_nhs[i]);
  }

  //Matched prefix length for real traffic
  sum = 0;
  for (i = 0; i < real_ip_cnt; i++) {
    sum += cptrie_matched_prefix_len(real_ips[i]);
  }
  printf ("CP-Trie Real traffic prefix length = %llu\n", sum/real_ip_cnt);

  //Matched prefix length for random traffic
  sum = 0;
  for (i = 0; i < RND_CNT; i++) {
    sum += cptrie_matched_prefix_len(rnd_ips[i]);
  }
  printf ("CP-Trie Random traffic prefix length = %llu\n", sum/RND_CNT);

  //Matched prefix length for sequential traffic
  sum = 0;
  for (i = 0; i < SEQ_CNT; i++) {
    sum += cptrie_matched_prefix_len(seq_ips[i]);
  }
  printf ("CP-Trie Sequencial traffic prefix length = %llu\n", sum/SEQ_CNT);
  
  //Matched prefix length for prefix traffic
  sum = 0;
  for (i = 0; i < prefix_cnt; i++) {
    sum += cptrie_matched_prefix_len(prefixes[i]);
  }
  printf ("CP-Trie Prefix traffic prefix length = %llu\n", sum/prefix_cnt);
  
  //Matched prefix length for repeated traffic
  sum = 0;
  for (i = 0; i < prefix_cnt; i++) {
    for (j = 0; j < REPEAT; j++)
      sum += cptrie_matched_prefix_len(prefixes[i]);
  }
  printf ("CP-Trie Repeated traffic prefix length = %llu\n", sum/(prefix_cnt * REPEAT));

  cptrie_cleanup();

  return 0;
}

void write_summery (struct result *res, int num_fibs)
{
  FILE *output;

  output = fopen("summery.data", "w");

  dump_prefix_distribution (output);

  fprintf(output, "\n\n");

  for(int i = 0; i < num_fibs; i++) {
    fprintf(output, "FIB %d\n", i);
    fprintf(output, "...................................\n", i);
    fprintf(output, "Total prefixes: %llu\n", res[i].total_prefixes);
    fprintf(output, "Prefixes (49-64): %llu\n", res[i].prefixes_49_64);
    fprintf(output, "Prefixes (65-128): %llu\n", res[i].prefixes_65_128);
    fprintf(output,"\n");
    fprintf (output, "SAIL-U insertion: %f microsec \n", res[i].sail_u_insert_time);
    fprintf (output, "SAIL-L insertion: %f microsec \n", res[i].sail_l_insert_time);
    fprintf (output, "Poptrie insertion: %f microsec \n", res[i].poptrie_insert_time);
    fprintf (output, "CP-Trie insertion: %f microsec \n", res[i].cptrie_insert_time);
    fprintf(output,"\n");
    fprintf (output, "SAIL-U memory: %f MB \n", res[i].sail_u_mem_consumption);
    fprintf (output, "SAIL-L memory: %f MB \n", res[i].sail_l_mem_consumption);
    fprintf (output, "Poptrie memory: %f MB \n", res[i].poptrie_mem_consumption);
    fprintf (output, "CP-Trie memory: %f MB \n", res[i].cptrie_mem_consumption);
    fprintf (output, "CP-Trie consumes %f X memory compared to Poptrie\n", res[i].cptrie_mem_consumption/res[i].poptrie_mem_consumption);
    fprintf(output,"\n");
    fprintf (output, "SAIL-U lookup time: %f ns \n", res[i].sail_u_lookup_time);
    fprintf (output, "SAIL-L lookup time: %f ns \n", res[i].sail_l_lookup_time);
    fprintf (output, "Poptrie lookup time: %f ns \n", res[i].poptrie_lookup_time);
    fprintf (output, "CP-Trie lookup time: %f ns \n", res[i].cptrie_lookup_time);
    fprintf(output, "\n");
    fprintf (output, "SAIL-U lookup cpu cycle: %f \n", res[i].sail_u_lookup_cpucycle);
    fprintf (output, "SAIL-L lookup cpu cycle: %f \n", res[i].sail_l_lookup_cpucycle);
    fprintf (output, "Poptrie lookup cpu cycle: %f \n", res[i].poptrie_lookup_cpucycle);
    fprintf (output, "CP-Trie lookup cpu cycle: %f \n", res[i].cptrie_lookup_cpucycle);
    fprintf(output, "\n");
    fprintf(output, "Real traffic\n");
    fprintf(output, "--------------------------------------------------\n");
    fprintf (output, "SAIL-U lookup throughput: %f Mlps \n", res[i].sail_u_lookup_throughput_real_traffic);
    fprintf (output, "SAIL-L lookup throughput: %f Mlps \n", res[i].sail_l_lookup_throughput_real_traffic);
    fprintf (output, "Poptrie lookup throughput: %f Mlps \n", res[i].poptrie_lookup_throughput_real_traffic);
    fprintf (output, "CP-Trie lookup throughput: %f Mlps \n", res[i].cptrie_lookup_throughput_real_traffic);
    fprintf (output, "CP-Trie achieves %f X lookup throughput compared to Poptrie\n", res[i].cptrie_lookup_throughput_real_traffic/res[i].poptrie_lookup_throughput_real_traffic);
    fprintf(output, "\n");
    fprintf(output, "Random traffic\n");
    fprintf(output, "--------------------------------------------------\n");
    fprintf (output, "SAIL-U lookup throughput: %f Mlps \n", res[i].sail_u_lookup_throughput_rnd_traffic);
    fprintf (output, "SAIL-L lookup throughput: %f Mlps \n", res[i].sail_l_lookup_throughput_rnd_traffic);
    fprintf (output, "Poptrie lookup throughput: %f Mlps \n", res[i].poptrie_lookup_throughput_rnd_traffic);
    fprintf (output, "CP-Trie lookup throughput: %f Mlps \n", res[i].cptrie_lookup_throughput_rnd_traffic);
    fprintf (output, "CP-Trie achieves %f X lookup throughput compared to Poptrie\n", res[i].cptrie_lookup_throughput_rnd_traffic/res[i].poptrie_lookup_throughput_rnd_traffic);
    fprintf(output, "\n");
    fprintf(output, "Sequential traffic\n");
    fprintf(output, "--------------------------------------------------\n");
    fprintf (output, "SAIL-U lookup throughput: %f Mlps \n", res[i].sail_u_lookup_throughput_seq_traffic);
    fprintf (output, "SAIL-L lookup throughput: %f Mlps \n", res[i].sail_l_lookup_throughput_seq_traffic);
    fprintf (output, "Poptrie lookup throughput: %f Mlps \n", res[i].poptrie_lookup_throughput_seq_traffic);
    fprintf (output, "CP-Trie lookup throughput: %f Mlps \n", res[i].cptrie_lookup_throughput_seq_traffic);
    fprintf (output, "CP-Trie achieves %f X lookup throughput compared to Poptrie\n", res[i].cptrie_lookup_throughput_seq_traffic/res[i].poptrie_lookup_throughput_seq_traffic);
    fprintf(output, "\n");
    fprintf(output, "Prefix traffic\n");
    fprintf(output, "--------------------------------------------------\n");
    fprintf (output, "SAIL-U lookup throughput: %f Mlps \n", res[i].sail_u_lookup_throughput_pre_traffic);
    fprintf (output, "SAIL-L lookup throughput: %f Mlps \n", res[i].sail_l_lookup_throughput_pre_traffic);
    fprintf (output, "Poptrie lookup throughput: %f Mlps \n", res[i].poptrie_lookup_throughput_pre_traffic);
    fprintf (output, "CP-Trie lookup throughput: %f Mlps \n", res[i].cptrie_lookup_throughput_pre_traffic);
    fprintf (output, "CP-Trie achieves %f X lookup throughput compared to Poptrie\n", res[i].cptrie_lookup_throughput_pre_traffic/res[i].poptrie_lookup_throughput_pre_traffic);
    fprintf(output, "\n");
    fprintf(output, "Repeated traffic\n");
    fprintf(output, "--------------------------------------------------\n");
    fprintf (output, "SAIL-U lookup throughput: %f Mlps \n", res[i].sail_u_lookup_throughput_rep_traffic);
    fprintf (output, "SAIL-L lookup throughput: %f Mlps \n", res[i].sail_l_lookup_throughput_rep_traffic);
    fprintf (output, "Poptrie lookup throughput: %f Mlps \n", res[i].poptrie_lookup_throughput_rep_traffic);
    fprintf (output, "CP-Trie lookup throughput: %f Mlps \n", res[i].cptrie_lookup_throughput_rep_traffic);
    fprintf (output, "CP-Trie achieves %f X lookup throughput compared to Poptrie \n", res[i].cptrie_lookup_throughput_rep_traffic/res[i].poptrie_lookup_throughput_rep_traffic);
    fprintf(output, "\n");
  }

  fclose(output);
}

int read_real_traffic (char *file) {
  FILE *fp;
  char v6str[256];
  char buff[4096];
  int ret;
  struct in6_addr v6addr;

  if ((fp = fopen(file, "r")) == NULL) {
    puts("File not exists");
    return -1;
  }
  
  while ( !feof(fp) ) {
    if ( !fgets(buff, sizeof(buff), fp) )
      continue;
    memset(v6str, 0, sizeof(v6str));
    ret = sscanf(buff, "%[^'\n']s", v6str);
    if ( ret < 0 ) {
      puts ("Invalid IP");
      return -1;
    }

    ret = inet_pton(AF_INET6, v6str, &v6addr);
    if ( ret < 0 ) {
      printf ("Invalid IPv6 prefix %s \n", v6str);
      return -1;
    }

    if (real_ip_cnt >= REAL_CNT) {
      puts ("The real traffic array is full. Ignoring rest of the traffic");
      return 0;
    }

    real_ips[real_ip_cnt++] = in6_addr_to_uint128(&v6addr);
  }
  return 0;
}

//Total number of FIBs is 7
#define NUM_FIB 9

int main() {
  int ret = 0;
  long long i;
  struct xorshift128_state state = {1, 1, 1, 1};

  //Read real traffic from file
  ret = read_real_traffic("real_traffic");
  if (ret) {
    puts ("Error in reading real traffic from file"); 
    return -1;
  }

  //Generate random IPv6 addresses for random traffic.
  for (i = 0; i < RND_CNT;) {
    //Generate a new 128-bit random number
    xorshift128(&state);
    //Generate random IPv6 addresses within 0x2000::/4 
    rnd_ips[i++] = (((__uint128_t)0x2) << 124) | (xorshift_to_ipv6(&state) >> 4);
  }

//  //Generate 2^16 sequential IPv6 addresses within 2402:4f00:4000::/120
//  for (i = 0; i < SEQ_CNT; i++) {
//    seq_ips[i] = (((__uint128_t)0x24024F004000ULL) << 80) | ((__uint128_t)i);
//  }

#ifdef CALC_MATCHED_PREFIX_LEN
  calc_avg_matched_prefix_len ("fibs/ip6/routes-293");
  calc_avg_matched_prefix_len ("fibs/ip6/routes-852");
  calc_avg_matched_prefix_len ("fibs/ip6/routes-19016");
  calc_avg_matched_prefix_len ("fibs/ip6/routes-19151");
  calc_avg_matched_prefix_len ("fibs/ip6/routes-19653");
  calc_avg_matched_prefix_len ("fibs/ip6/routes-23367");
  calc_avg_matched_prefix_len ("fibs/ip6/routes-53828");
  calc_avg_matched_prefix_len ("fibs/ip6/routes-199524");
  calc_avg_matched_prefix_len ("fibs/ip6/routes-395570");
  exit (0);
#endif

  struct result res[NUM_FIB];
  memset (res, 0, sizeof (res));
  //Our stopwatch supports both high-resulation counter and 
  //CPU performance counter. Here we initialize stop watch with
  //CPU performance counter (TSC register).
  //It only needs to be called once. No cleanup is needed.
  stopwatch_init(TSC); 

  ret = test ("fibs/ip6/routes-293", &res[0]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");

  ret = test ("fibs/ip6/routes-852", &res[1]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");

  ret = test ("fibs/ip6/routes-19016", &res[2]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");

  ret = test ("fibs/ip6/routes-19151", &res[3]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");

  ret = test ("fibs/ip6/routes-19653", &res[4]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");

  ret = test ("fibs/ip6/routes-23367", &res[5]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");

  ret = test ("fibs/ip6/routes-53828", &res[6]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");
  
  ret = test ("fibs/ip6/routes-199524", &res[7]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");
  
  ret = test ("fibs/ip6/routes-395570", &res[8]);
  if (ret) 
    return -1;
  puts ("The test case passed\n");
  
  puts ("All tests have passed");
  write_summery(res, NUM_FIB);

#ifdef RECORD_TRAFFIC
  FILE *seq_traffic, *rnd_traffic, *rep_traffic;
  
  //Recording repeated traffic
  rep_traffic = fopen("rep_traffic", "w");
  for (i = 0; i < REP_CNT; i++) {
    fprintf(rep_traffic, "%s\n", ipv6_to_str(rep_ips[i]));
  }
  fclose(rep_traffic);

  //Recording sequential traffic
  seq_traffic = fopen("seq_traffic", "w");
  for (i = 0; i < SEQ_CNT; i++) {
    fprintf(seq_traffic, "%s\n", ipv6_to_str(seq_ips[i]));
  }
  fclose(seq_traffic);

  //Recording random traffic
  rnd_traffic = fopen("rnd_traffic", "w");
  for (i = 0; i < RND_CNT; i++) {
    fprintf(rnd_traffic, "%s\n", ipv6_to_str(rnd_ips[i]));
  }
  fclose(rnd_traffic);
#endif
}
