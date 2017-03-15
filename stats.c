/*
 * Copyright (c) 2009 Ryan Flannery <ryan.flannery@gmail.com>
 *  audio/volume by   Jacob Meuser <jakemsr@sdf.lonestar.org>
 *  patch by          Antoine Jacoutot <ajacoutot@openbsd.org>
 *  misc updates by   Dmitrij D. Czarkoff <czarkoff@gmail.cim>
 *  cpu consolidation Martin Brandenburg <martin@martinbrandenburg.com>
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

#include "stats.h"

/* extern's from stats.h */
volume_info_t volume;
power_info_t power;
sysinfo_t sysinfo;
brightness_info_t brightness;
char *time_fmt;


/* draw text in a given color at a given (x,y) */
int
render_text(XftColor *c, int x, int y, const char *str)
{
   XGlyphInfo extents;
   XftTextExtents8( XINFO.disp, XINFO.font, (XftChar8 *)str, strlen(str), &extents );

   XftDrawString8(XINFO.xftdraw, c, XINFO.font, x, y, (XftChar8 *)str, strlen(str));
   return extents.width;
}

/* format memory (measured in kilobytes) for display */
char *
fmtmem(int m)
{
   static char scratchpad[255];
   char scale = 'K';

   if (m >= 10000) {
      m = (m + 512) / 1024;
      scale = 'M';
   }

   if (m >= 10000) {
      m = (m + 512) / 1024;
      scale = 'G';
   }

   snprintf(scratchpad, sizeof(scratchpad), "%d%c", m, scale);
   return scratchpad;
}


/*****************************************************************************
 * volume stuff
 ****************************************************************************/

int
volume_check_dev(int fd, int class, char *name)
{
   mixer_devinfo_t devinfo;

   if (class < 0 || name == NULL)
      return (-1);

   devinfo.index = 0;
   while (ioctl(fd, AUDIO_MIXER_DEVINFO, &devinfo) >= 0) {
      if ((devinfo.type == AUDIO_MIXER_VALUE)
      &&  (devinfo.mixer_class == class)
      &&  (strncmp(devinfo.label.name, name, MAX_AUDIO_DEV_LEN) == 0))
         return (devinfo.index);

      devinfo.index++;
   }

   return (-1);
}

void
volume_init()
{
   mixer_devinfo_t devinfo;
   int oclass_idx, iclass_idx;

   volume.is_setup = false;

   /* open mixer */
   if ((volume.dev_fd = open("/dev/mixer", O_RDWR)) < 0) {
      warn("volume: failed to open /dev/mixer");
      return;
   }

   /* find the outputs and inputs classes */
   oclass_idx = iclass_idx = -1;
   devinfo.index = 0;
   while (ioctl(volume.dev_fd, AUDIO_MIXER_DEVINFO, &devinfo) >= 0) {

      if (devinfo.type != AUDIO_MIXER_CLASS) {
         devinfo.index++;
         continue;
	  }

      if (strncmp(devinfo.label.name, AudioCoutputs, MAX_AUDIO_DEV_LEN) == 0)
         oclass_idx = devinfo.index;
      if (strncmp(devinfo.label.name, AudioCinputs, MAX_AUDIO_DEV_LEN) == 0)
         iclass_idx = devinfo.index;

      if (oclass_idx != -1 && iclass_idx != -1)
         break;

      devinfo.index++;
   }

   /* find the master device */
   volume.master_idx = volume_check_dev(volume.dev_fd, oclass_idx, AudioNmaster);
   if (volume.master_idx == -1)
      volume.master_idx = volume_check_dev(volume.dev_fd, iclass_idx, AudioNdac);
   if (volume.master_idx == -1)
      volume.master_idx = volume_check_dev(volume.dev_fd, oclass_idx, AudioNdac);
   if (volume.master_idx == -1)
      volume.master_idx = volume_check_dev(volume.dev_fd, oclass_idx, AudioNoutput);

   if (volume.master_idx == -1) {
      warnx("volume: failed to find \"master\" mixer device");
      return;
   }

   devinfo.index = volume.master_idx;
   if (ioctl(volume.dev_fd, AUDIO_MIXER_DEVINFO, &devinfo) == -1) {
      warn("AUDIO_MIXER_DEVINFO");
      return;
   }

   volume.max = AUDIO_MAX_GAIN;
   volume.nchan = devinfo.un.v.num_channels;

   /* finished... now close the device and reopen as read only */
   close(volume.dev_fd);
   volume.dev_fd = open("/dev/mixer", O_RDONLY);
   if (volume.dev_fd < 0) {
      warn("volume: failed to re-open /dev/mixer");
      return;
   }

   volume.is_setup = true;
}

void
volume_update()
{
   static mixer_ctrl_t vinfo;

   if (!volume.is_setup)
      return;

   /* query info */
   vinfo.dev = volume.master_idx;
   vinfo.type = AUDIO_MIXER_VALUE;
   vinfo.un.value.num_channels = volume.nchan;
   if (ioctl(volume.dev_fd, AUDIO_MIXER_READ, &(vinfo)) < 0) {
      warn("volume update: AUDIO_MIXER_READ");
      return;
   }

   /* record in global struct */
   if (volume.nchan == 1)
      volume.left = volume.right = vinfo.un.value.level[AUDIO_MIXER_LEVEL_MONO];
   else {
      volume.left  = vinfo.un.value.level[AUDIO_MIXER_LEVEL_LEFT];
      volume.right = vinfo.un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
   }
}

void
volume_close() {
   if (!volume.is_setup)
      return;

   close(volume.dev_fd);
}

int
volume_draw(XftColor *color, int x, int y)
{
   static char str[6];
   float left, right;
   int   lheight, rheight;
   int   startx;
   int   width = 5;  /* width of the bar-graphs */

   if (!volume.is_setup)
      return 0;

   startx = x;
   width = 5;

   /* get volume as percents */
   left  = roundf(100.0 * (float)volume.left  / (float)volume.max);
   right = roundf(100.0 * (float)volume.right / (float)volume.max);

   /* determine height of green-part of bar graphs */
   lheight = (int)(left  * (float)XINFO.height / 100.0);
   rheight = (int)(right * (float)XINFO.height / 100.0);

   /* start drawing... */
   x += render_text(color, x, y, "vol:");

   /* left volume % */
   snprintf(str, sizeof(str), "%d%%", (int)left);
   x += render_text(color, x, y, str) + 1;

   /* left graph */
   XftDrawRect(XINFO.xftdraw, &COLOR_RED, x, 0, width, XINFO.height);
   XftDrawRect(XINFO.xftdraw, &COLOR_GREEN, x, XINFO.height - lheight, width, lheight);
   x += width + 1;

   /* right graph */
   XftDrawRect(XINFO.xftdraw, &COLOR_RED, x, 0, width, XINFO.height);
   XftDrawRect(XINFO.xftdraw, &COLOR_GREEN, x, XINFO.height - rheight, width, lheight);
   x += width + 1;

   /* right volume % */
   snprintf(str, sizeof(str), "%d%%", (int)right);
   x += render_text(color, x, y, str);

   return x - startx;
}


/*****************************************************************************
 * power stuff
 ****************************************************************************/

void
power_init()
{
   power.is_setup = false;

   power.dev_fd = open("/dev/apm", O_RDONLY);
   if (power.dev_fd < 0) {
      warn("power: failed to open /dev/apm");
      return;
   }

   power.is_setup = true;
}

void
power_update()
{
   if (!power.is_setup)
      return;

   if (ioctl(power.dev_fd, APM_IOC_GETPOWER, &(power.info)) < 0)
      warn("power update: APM_IOC_GETPOWER");
}

void
power_close()
{
   if (!power.is_setup)
      return;

   close(power.dev_fd);
}

int
power_draw(XftColor *color, int x, int y)
{
   static char str[1000];
   char *state;
   int startx, width, h;

   if (!power.is_setup)
      return 0;

   startx = x;
   width = 5;

   switch (power.info.ac_state) {
      case APM_AC_OFF:
         state = "BAT";
         break;
      case APM_AC_ON:
         state = "AC";
         break;
      default:
         return 0;
         break;
   }

   /* draw the state */
   snprintf(str, sizeof(str), "%s:", state);
   x += render_text(color, x, y, str) + 1;

   /* draw the graph */
   h = power.info.battery_life * XINFO.height / 100;
   XftDrawRect(XINFO.xftdraw, &COLOR_RED, x, 0, width, XINFO.height);
   XftDrawRect(XINFO.xftdraw, &COLOR_GREEN, x, XINFO.height - h, width, h);

   x += width + 1;

   /* draw the percent and time remaining */
   
   snprintf(str, sizeof(str), (power.info.minutes_left != (u_int)-1) ? "(%d%%,%dm)"
      : "(%d%%)", power.info.battery_life, power.info.minutes_left);

   x += render_text(color, x, y, str);
   return x - startx;
}


/*****************************************************************************
 * sysinf stuff (cpu/mem/procs)
 ****************************************************************************/

void
sysinfo_init(int hist_size)
{
   size_t size;
   int mib[] = { CTL_HW, HW_NCPU };
   int i, j, k;

   /* history size and starting column */
   sysinfo.current    = 0;
   sysinfo.hist_size  = hist_size;

   /* init process counters */
   sysinfo.swap_used = sysinfo.swap_total = 0;
   sysinfo.procs_active = sysinfo.procs_total = 0;

   /* setup page-shift */
   i = getpagesize();
   sysinfo.pageshift = 0;
   while (i > 1) {
      sysinfo.pageshift++;
      i >>= 1;
   }
   sysinfo.pageshift -= 10;

   /* allocate memory history */
   if ((sysinfo.memory = calloc(hist_size, sizeof(int*))) == NULL)
      err(1, "sysinfo init: memory calloc failed");

   for (i = 0; i < hist_size; i++) {
      if ((sysinfo.memory[i] = calloc(3, sizeof(int))) == NULL)
         err(1, "sysinfo init: memory[%d] calloc failed", i);

      for (j = 0; j < 3; j++)
         sysinfo.memory[i][j] = 0;
   }

   /* get number of cpu's */
   size = sizeof(sysinfo.ncpu);
   if (sysctl(mib, 2, &(sysinfo.ncpu), &size, NULL, 0) == -1)
      err(1, "sysinfo init: sysctl HW.NCPU failed");

   /* allocate cpu history */
   sysinfo.cpu_raw   = calloc(sysinfo.ncpu, sizeof(uint64_t**));
   sysinfo.cpu_pcnts = calloc(sysinfo.ncpu, sizeof(int**));
   if (sysinfo.cpu_raw == NULL || sysinfo.cpu_pcnts == NULL)
      err(1, "sysinfo init: cpu_raw/cpu_pcnts calloc failed");

   for (i = 0; i < sysinfo.ncpu; i++) {
      sysinfo.cpu_raw[i]   = calloc(hist_size, sizeof(uint64_t*));
      sysinfo.cpu_pcnts[i] = calloc(hist_size, sizeof(int*));
      if (sysinfo.cpu_raw[i] == NULL || sysinfo.cpu_pcnts[i] == NULL)
         err(1, "sysinfo init: cpu_raw/cpu_pcnts[%d] calloc failed", i);

      for (j = 0; j < hist_size; j++) {
         sysinfo.cpu_raw[i][j]   = calloc(CPUSTATES, sizeof(uint64_t));
         sysinfo.cpu_pcnts[i][j] = calloc(CPUSTATES, sizeof(int));
         if (sysinfo.cpu_raw[i][j] == NULL || sysinfo.cpu_pcnts[i][j] == NULL)
            err(1, "sysinfo init: cpu_raw/cpu_pcnts[%d][%d] calloc failed", i, j);

         for (k = 0; k < CPUSTATES; k++) {
            sysinfo.cpu_raw[i][j][k] = 0;
            sysinfo.cpu_pcnts[i][j][k] = 0;
         }
      }
   }

   /* do an initial reading (needed to setup initial data for graphs) */
   sysinfo_update();
}

void
sysinfo_update()
{
   static int mib_nprocs[] = { CTL_KERN, KERN_NPROCS };
   static int mib_vm[] = { CTL_VM, VM_METER };
   static int mib_cpus[] = { CTL_KERN, 0, 0 };
   static int diffs[CPUSTATES] = { 0 };
   struct vmtotal vminfo;
   struct swapent *swapdev;
   size_t    size;
   int       cpu, state;
   int       cur, prev;
   int       nticks, nswaps;

   /* update current column in historical data & figure out previous */
   sysinfo.current = (1 + sysinfo.current) % sysinfo.hist_size;
   cur  = sysinfo.current;
   prev = (cur == 0 ? sysinfo.hist_size - 1 : cur - 1);


   /* update number of total/active processes */
   size = sizeof(sysinfo.procs_total);
   if (sysctl(mib_nprocs, 2, &sysinfo.procs_total, &size, NULL, 0) == -1)
      warn("sysinfo update: sysctl KERN.NPROCS");
   /* TODO update procs_active here... is there easy way (sysctl)? */


   /* update mem history */
   size = sizeof(vminfo);
   if (sysctl(mib_vm, 2, &vminfo, &size, NULL, 0) < 0)
      err(1, "sysinfo update: VM.METER failed");

   sysinfo.memory[cur][MEM_ACT] = vminfo.t_arm << sysinfo.pageshift;
   sysinfo.memory[cur][MEM_TOT] = vminfo.t_rm << sysinfo.pageshift;
   sysinfo.memory[cur][MEM_FRE] = vminfo.t_free << sysinfo.pageshift;

   /* get swap status */
   sysinfo.swap_used = sysinfo.swap_total = 0;
   if ((nswaps = swapctl(SWAP_NSWAP, 0, 0)) == 0) {
      if ((swapdev = calloc(nswaps, sizeof(*swapdev))) == NULL)
        err(1, "sysinfo update: swapdev calloc failed (%d)", nswaps);
      if (swapctl(SWAP_STATS, swapdev, nswaps) == -1)
        err(1, "sysinfo update: swapctl(SWAP_STATS) failed");

      for (size = 0; size < nswaps; size++) {
        if (swapdev[size].se_flags & SWF_ENABLE) {
          sysinfo.swap_used  += swapdev[size].se_inuse / (1024 / DEV_BSIZE);
          sysinfo.swap_total += swapdev[size].se_nblks / (1024 / DEV_BSIZE);
        }
      }
      free(swapdev);
   }

   /* get states for each cpu. note this is raw # of ticks */
   size = CPUSTATES * sizeof(int64_t);
   if (sysinfo.ncpu > 1) {
      mib_cpus[1] = KERN_CPTIME2;
      for (cpu = 0; cpu < sysinfo.ncpu; cpu++) {
         mib_cpus[2] = cpu;
         if (sysctl(mib_cpus, 3, sysinfo.cpu_raw[cpu][cur], &size, NULL, 0) < 0)
            err(1, "sysinfo update: KERN.CPTIME2.%d failed", cpu);
      }
   } else {
      int i;
      long cpu_raw_tmp[CPUSTATES];
      size = sizeof(cpu_raw_tmp);
      mib_cpus[1] = KERN_CPTIME;
      
      if (sysctl(mib_cpus, 2, cpu_raw_tmp, &size, NULL, 0) < 0)
         err(1, "sysinfo update: KERN.CPTIME failed");

      for (i = 0; i < CPUSTATES; i++)
         sysinfo.cpu_raw[0][cur][i] = cpu_raw_tmp[i];
   }

   /* convert ticks to percentages */
   for (cpu = 0; cpu < sysinfo.ncpu; cpu++) {
      nticks = 0;
      for (state = 0; state < CPUSTATES; state++) {
         diffs[state] = sysinfo.cpu_raw[cpu][cur][state]
                      - sysinfo.cpu_raw[cpu][prev][state];

         if (diffs[state] < 0) {
            diffs[state] = INT64_MAX
                         - sysinfo.cpu_raw[cpu][prev][state]
                         - sysinfo.cpu_raw[cpu][cur][state];
         }
         nticks += diffs[state];
      }

      if (nticks == 0)
         nticks = 1;

      for (state = 0; state < CPUSTATES; state++) {
         sysinfo.cpu_pcnts[cpu][cur][state] =
            ((diffs[state] * 1000 + (nticks / 2)) / nticks) / 10;
      }
   }
}

void
sysinfo_close()
{
   /* nothing now, but keep here in case */
}

int
cpu_draw(int cpu, XftColor *color, int x, int y)
{
   static char  str[1000];
   static char *cpuStateNames[] = { "u", "n", "s", "i", "I" };
   static XftColor *cpuStateColors[] = {
     &COLOR_RED, &COLOR_BLUE, &COLOR_YELLOW, &COLOR_MAGENTA, &COLOR_GREEN
   };
   int state, startx, time, col, h, i, j;

   startx = x;

   if (cpu == -1)
      snprintf(str, sizeof(str), "cpu: ");
   else
      snprintf(str, sizeof(str), "cpu%d: ", cpu);
   x += render_text(color, x, y, str) + 1;

   /* for the graph, draw a green rectangle to start with */
   XftDrawRect(XINFO.xftdraw, &COLOR_GREEN, x, 0, sysinfo.hist_size, XINFO.height);

   /* start adding every 'bar' to the bar-graph... */
   time = (sysinfo.current + 1) % sysinfo.hist_size;
   for (col = 0; col < sysinfo.hist_size; col++) {

      /* user time */
      h = 0;
      if (cpu == -1) {
         for (i = 0; i < 4; i++)
            for (j = 0; j < sysinfo.ncpu; j++)
               h += sysinfo.cpu_pcnts[j][time][i];
         h /= sysinfo.ncpu;
      } else
         for (i = 0; i < 4; i++) h += sysinfo.cpu_pcnts[cpu][time][i];
      h = h * XINFO.height / 100;
      XftDrawRect(XINFO.xftdraw, &COLOR_RED, x + col, XINFO.height - h, 1, h);

      /* nice time */
      h = 0;
      if (cpu == -1) {
         for (i = 1; i < 4; i++)
            for (j = 0; j < sysinfo.ncpu; j++)
               h += sysinfo.cpu_pcnts[j][time][i];
         h /= sysinfo.ncpu;
      } else
         for (i = 1; i < 4; i++) h += sysinfo.cpu_pcnts[cpu][time][i];
      h = h * XINFO.height / 100;
      XftDrawRect(XINFO.xftdraw, &COLOR_BLUE, x + col, XINFO.height - h, 1, h);

      /* system time */
      h = 0;
      if (cpu == -1) {
         for (i = 2; i < 4; i++)
            for (j = 0; j < sysinfo.ncpu; j++)
               h += sysinfo.cpu_pcnts[j][time][i];
         h /= sysinfo.ncpu;
      } else
         for (i = 2; i < 4; i++) h += sysinfo.cpu_pcnts[cpu][time][i];
      h = h * XINFO.height / 100;
      XftDrawRect(XINFO.xftdraw, &COLOR_YELLOW, x + col, XINFO.height - h, 1, h);

      /* interrupt time */
      if (cpu == -1) {
         for (j = 0; j < sysinfo.ncpu; j++)
            h = sysinfo.cpu_pcnts[j][time][3];
         h /= sysinfo.ncpu;
      } else
         h = sysinfo.cpu_pcnts[cpu][time][3];
      h = h * XINFO.height / 100;
      XftDrawRect(XINFO.xftdraw, &COLOR_MAGENTA, x + col, XINFO.height - h, 1, h);
      time = (time + 1) % sysinfo.hist_size;
   }

   x += sysinfo.hist_size + 1;

   /* draw the text */
   time = sysinfo.current;
   for (state = 0; state < CPUSTATES; state++) {
      if (cpu == -1) {
         h = 0;
         for (i = 0; i < sysinfo.ncpu; i++)
            h += sysinfo.cpu_pcnts[i][time][state];
         h /= sysinfo.ncpu;
         snprintf(str, sizeof(str), "%3d%%%s", h, cpuStateNames[state]);
      } else
         snprintf(str, sizeof(str), "%3d%%%s",
            sysinfo.cpu_pcnts[cpu][time][state], cpuStateNames[state]);

      x += render_text(cpuStateColors[state], x, y, str);
   }

   return x - startx;
}

int
mem_draw(XftColor *color, int x, int y)
{
   int h, total;
   int startx;
   int col, time, cur;

   startx = x;
   cur = sysinfo.current;

   /* determine total memory */
   total = sysinfo.memory[cur][MEM_ACT]
         + sysinfo.memory[cur][MEM_TOT]
         + sysinfo.memory[cur][MEM_FRE];

   /* start drawing ... */
   x += render_text(color, x, y, "mem: ") + 1;

   /* green bg for graph */
   XftDrawRect(XINFO.xftdraw, &COLOR_GREEN, x, 0, sysinfo.hist_size, XINFO.height);

   /* start drawing each bar in the bar-graph */
   time = (sysinfo.current + 1) % sysinfo.hist_size;
   for (col = 0; col < sysinfo.hist_size; col += 1) {

      if ((sysinfo.memory[time][MEM_ACT] != 0)
      ||  (sysinfo.memory[time][MEM_TOT] != 0)
      ||  (sysinfo.memory[time][MEM_FRE] != 0)) {


        /* draw yellow (total) bar */
        h = (sysinfo.memory[time][MEM_TOT] + sysinfo.memory[time][MEM_ACT])
          * XINFO.height / total;

        XftDrawRect(XINFO.xftdraw, &COLOR_YELLOW, x + col, XINFO.height - h, 1, h);

        /* draw red (active) bar */
        h = sysinfo.memory[time][MEM_ACT] * XINFO.height / total;
        XftDrawRect(XINFO.xftdraw, &COLOR_RED, x + col, XINFO.height - h, 1, h);
      }

      time = (time + 1) % sysinfo.hist_size;
   }
   x += sysinfo.hist_size + 1;

   /* draw numbers */
   x += render_text(&COLOR_RED, x, y, fmtmem(sysinfo.memory[cur][MEM_ACT]));
   x += render_text(color, x, y, "/");
   x += render_text(&COLOR_YELLOW, x, y, fmtmem(sysinfo.memory[cur][MEM_TOT]));
   x += render_text(color, x, y, "/");
   x += render_text(&COLOR_GREEN, x, y, fmtmem(sysinfo.memory[cur][MEM_FRE]));

   /* draw swap, if any is used */
   if (sysinfo.swap_used > 0) {
      x += render_text(color, x, y, " swap:");
      x += render_text(&COLOR_RED, x, y, fmtmem(sysinfo.swap_used));
      x += render_text(color, x, y, "/");
      x += render_text(&COLOR_GREEN, x, y, fmtmem(sysinfo.swap_total));
   }

   return x - startx;
}

int
procs_draw(XftColor *color, int x, int y)
{
   static char str[1000];
   int startx;

   startx = x;
   x += render_text(color, x, y, "procs: ");

   /* FIXME finish getting the number of active processes
    * i, personally, like this...
   snprintf(str, sizeof(str), "%d", sysinfo.procs_active);
   x += render_text(COLOR_RED, x, y, str);

   x += render_text(color, x, y, "/");
   */

   snprintf(str, sizeof(str), "%d", sysinfo.procs_total);
   x += render_text(&COLOR_RED, x, y, str);

   return x - startx;
}


/*****************************************************************************
 * time
 ****************************************************************************/

int
time_draw(XftColor *color, int x, int y)
{
   static char timestr[1000];
   time_t now = time(NULL);

   /* first build the string */
   strftime(timestr, sizeof(timestr), time_fmt, localtime(&now));

   /* XXX hack to right-align it - rethink a more general way for this */
   XGlyphInfo extents;
   XftTextExtents8(XINFO.disp, XINFO.font, (XftChar8 *)timestr, strlen(timestr), &extents );
   return render_text(color, XINFO.width - extents.width, y, timestr);
}

