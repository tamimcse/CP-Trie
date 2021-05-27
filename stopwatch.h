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
#ifndef STOPWATCH_H_
#define STOPWATCH_H_

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

/*
 *We can implement a stop watch using clock_gettime(), getrusage() or TSC
 *register. Here we use TSC register because it's frequency is same as CPU.
 *getrusage() is useful if we need User and System time separately which is not
 *the case here. The resolution of getrusage() is also microsecond. 
 *clock_gettime() on the other hand provides us with ns resolution.
 *However TSC has smaller than 1ns resolution. This is why we use
 *TSC here.
 */
enum clock_type {TSC = 0, CLOCK = 1};

void stopwatch_init(enum clock_type t);
int stopwatch_start();
int stopwatch_stop(double *delay, double *cpu_cycle);

#endif /* STOPWATCH_H_ */
