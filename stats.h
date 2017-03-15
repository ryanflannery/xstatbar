/*
 * Copyright (c) 2009 Ryan Flannery <ryan.flannery@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef STATS_H
#define STATS_H

#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <err.h>

#include <machine/apmvar.h>
#include <sys/audioio.h>
#include <sys/vmmeter.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/swap.h>

#include "xstatbar.h"

/*
 * The following are all global structs used to record the various stats
 * queried by xstatbar.
 */

/* volume */
typedef struct {
   bool  is_setup;

   int   dev_fd;
   int   master_idx;

   int   max;
   int   nchan;
   int   left;
   int   right;
} volume_info_t;
extern volume_info_t volume;

/* power */
typedef struct {
   bool   is_setup;
   int    dev_fd;
   struct apm_power_info   info;
} power_info_t;
extern power_info_t power;

/* system info (cpu + memory + proccess info) */
typedef struct {
   int       ncpu;         /* # of cpu's present */
   int       pageshift;    /* used to properly calculate memory stats */

   int       procs_active; /* # of active processes */
   int       procs_total;  /* total # of processes */

   int       swap_used;    /* swap space used */
   int       swap_total;   /* total amount of swap space */

   /* cpu/memory historical stuff (for graphs) */

   int    hist_size;       /* size of graphs/historical-arrays */
   int    current;         /* "current" spot in historical arrays */

   /* historical data (for graphs) */
#define MEM_ACT 0
#define MEM_TOT 1
#define MEM_FRE 2
   int        **memory;    /* [hist_size][3] */
   int      ***cpu_pcnts;  /* [ncpu][hist_size][CPUSTATES] */
   uint64_t ***cpu_raw;    /* [ncpu][hist_size][CPUSTATES] */
} sysinfo_t;
extern sysinfo_t sysinfo;

/* brightness - FIXME still working on this part */
typedef struct {
   int   brightness;
} brightness_info_t;
extern brightness_info_t brightness;

/* the format used by strftime(3) */
extern char *time_fmt;


/*
 * The following are used to initialize, update, and end the querying of
 * the above stats.
 */

/* volume */
void volume_init();
void volume_update();
void volume_close();

/* power */
void power_init();
void power_update();
void power_close();

/* sysinfo (includes cpu/memory/process information) */
void sysinfo_init(int hist_size);
void sysinfo_update();
void sysinfo_close();


/*
 * The following are used to draw the stats.  Each takes a color that is
 * used for coloring the TEXT and the text only.  Additionally, they take
 * an (x,y) for where to start drawing their stats.  They each return
 * the width, in pixels, of what they drew.
 */

int  volume_draw(XftColor *c, int x, int y);
int  power_draw(XftColor *c, int x, int y);
int  cpu_draw(int cpu, XftColor *c, int x, int y);
int  mem_draw(XftColor *c, int x, int y);
int  procs_draw(XftColor *c, int x, int y);
int  time_draw(XftColor *c, int x, int y);

#endif
