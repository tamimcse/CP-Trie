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
#include "stopwatch.h"

struct timespec clock_start, clock_end;
uint64_t tick_start, tick_end;
bool is_started = false;
enum clock_type type;
float ticks_per_ns;

static __inline__ uint64_t rdtsc()
{
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

static double calc_cpu_frequency ()
{
  time_t basetime;
  uint64_t stclk, endclk, nclks;
  double cpu_freq;

  basetime = time(NULL);    /* time returns seconds */
  while (time(NULL)==basetime);
  stclk=rdtsc();    /* rdtsc is an assembly instruction */
  basetime=time(NULL);
  while (time(NULL)==basetime);
  endclk = rdtsc();
  nclks = endclk-stclk;
  cpu_freq = (double)nclks/1000000000;
  //3.1 GHz in our case
  return cpu_freq;
}
 
static double diff (struct timespec start, struct timespec end)
{
  long seconds = end.tv_sec - start.tv_sec;
  long ns = end.tv_nsec - start.tv_nsec;

  if (start.tv_nsec > end.tv_nsec) { // clock underflow   
    seconds--; 
    ns += 1000000000; 
  }

  return (double)seconds * (double)1000000000 + (double)ns;
}

void stopwatch_init(enum clock_type t)
{
  type = t;
  ticks_per_ns = calc_cpu_frequency();
  printf ("CPU frequency is %f GHz\n", ticks_per_ns);
}

int stopwatch_start()
{
  //Already started
  if (is_started)
    return -1;

  switch (type) {
    case CLOCK:
      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &clock_start);
      break;
    case TSC:
      tick_start = rdtsc();
      break;
  }

  is_started = true;
  return 0;
}

//Sets the latency in ns
int stopwatch_stop(double *delay, double *cpu_cycle)
{
  //The clocks needs to be started before stop
  if (!is_started)
    return -1;

  switch (type) {
    case CLOCK:
      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &clock_end);
      *delay = diff(clock_start, clock_end);
      //It does not report CPU cycle
      *cpu_cycle = 0;
      break;

    case TSC:
      tick_end = rdtsc();
      *delay = (tick_end - tick_start) / ticks_per_ns;
      *cpu_cycle = tick_end - tick_start;
      break;
  }

  is_started = false;
  return 0;
}
