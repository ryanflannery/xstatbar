/*
 * Copyright (c) 2009 Ryan Flannery <ryan.flannery@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include "xstatbar.h"
#include "stats.h"

/* extern's from xstatbar.h */
xinfo_t  XINFO;

XftColor COLOR_RED,    COLOR_GREEN,   COLOR_BLUE,
         COLOR_YELLOW, COLOR_MAGENTA, COLOR_CYAN,
         COLOR_WHITE,  COLOR_BLACK;

/* signal flags */
volatile sig_atomic_t VSIG_QUIT = 0;

/* local functions */
void signal_handler(int sig);
void process_signals();
void cleanup();
void usage(const char *pname);
void setup_x(int x, int y, int w, int h, const char *font);
void draw(int);

int
main (int argc, char *argv[])
{
   const char *errstr;
   char *font;
   char  ch;
   int   x, y, w, h;
   int   sleep_seconds;
   int   consolidate_cpus = 0;

   /* set defaults */
   x = 0;
   y = 0;
   w = 1280;
   h = 13;
   font = "Fixed-6";
   time_fmt = "%a %d %b %Y %I:%M:%S %p";
   sleep_seconds = 1;

   /* parse command line */
   while ((ch = getopt(argc, argv, "x:y:w:h:s:f:t:Tc")) != -1) {
      switch (ch) {
         case 'x':
            x = strtonum(optarg, 0, INT_MAX, &errstr);
            if (errstr)
               errx(1, "illegal x value \"%s\": %s", optarg, errstr);
            break;

         case 'y':
            y = strtonum(optarg, 0, INT_MAX, &errstr);
            if (errstr)
               errx(1, "illegal y value \"%s\": %s", optarg, errstr);
            break;

         case 'w':
            w = strtonum(optarg, 0, INT_MAX, &errstr);
            if (errstr)
               errx(1, "illegal width value \"%s\": %s", optarg, errstr);
            break;

         case 'h':
            h = strtonum(optarg, 0, INT_MAX, &errstr);
            if (errstr)
               errx(1, "illegal height value \"%s\": %s", optarg, errstr);
            break;

         case 's':
            sleep_seconds = strtonum(optarg, 0, INT_MAX, &errstr);
            if (errstr)
               errx(1, "illegal sleep value \"%s\": %s", optarg, errstr);
            break;

         case 'f':
            font = strdup(optarg);
            if (font == NULL)
               err(1, "failed to strdup(3) font");
            break;

         case 't':
            time_fmt = strdup(optarg);
            if (time_fmt == NULL)
               err(1, "failed to strdup(3) time format");
            break;

         case 'T':
            time_fmt = "%a %d %b %Y %H:%M:%S";
            break;

         case 'c':
            consolidate_cpus = 1;
            break;

         case '?':
         default:
            usage(argv[0]);
            /* UNREACHABLE */
            break;
      }
   }

   /* init stat collectors */
   volume_init();
   power_init();
   sysinfo_init(45);

   /* setup X window */
   setup_x(x, y, w, h, font);

   /* shutdown function */
   signal(SIGINT,  signal_handler);

   while (1) {
      /* handle any signals */
      process_signals();

      /* update stats */
      volume_update();
      power_update();
      sysinfo_update();

      /* draw */
      draw(consolidate_cpus);
			XSync(XINFO.disp, False);

      /* sleep */
      sleep(sleep_seconds);
   }

   /* UNREACHABLE */
   return 0;
}

/* print usage and exit */
void
usage(const char *pname)
{
   fprintf(stderr, "\
usage: %s [-x xoffset] [-y yoffset] [-w width] [-h height] [-s secs]\n\
          [-f font] [-t time-format] [-T]\n",
   pname);
   exit(0);
}

/* signal handlers */
void
signal_handler(int sig)
{
   switch (sig) {
      case SIGHUP:
      case SIGINT:
      case SIGQUIT:
      case SIGTERM:
         VSIG_QUIT = 1;
         break;
   }
}

void
process_signals()
{
   if (VSIG_QUIT) {
      cleanup();
      VSIG_QUIT = 0;
   }

}

/* exit handler */
void
cleanup()
{
   /* x teardown */
   XdbeDeallocateBackBufferName(XINFO.disp, XINFO.backbuf);
   XClearWindow(XINFO.disp,   XINFO.win);
   XDestroyWindow(XINFO.disp, XINFO.win);
   XftDrawDestroy( XINFO.xftdraw );
	 XCloseDisplay(XINFO.disp);

   /* stats teardown */
   volume_close();
   power_close();
   sysinfo_close();

   exit(0);
}

void
setup_colors()
{
  XRenderColor white   = { .red = 0xffff, .green = 0xffff,	.blue = 0xffff, .alpha = 0xffff };
  XRenderColor red     = { .red = 0xffff, .green = 0x0,			.blue = 0x0,		.alpha = 0xffff };
  XRenderColor green   = { .red = 0x0,    .green = 0xf000,	.blue = 0x0,		.alpha = 0xffff };
  XRenderColor blue    = { .red = 0x0,    .green = 0x0,			.blue = 0xffff, .alpha = 0xffff };
  XRenderColor yellow  = { .red = 0xffff, .green = 0xffff,	.blue = 0x0,		.alpha = 0xffff };
  XRenderColor magenta = { .red = 0xffff, .green = 0x0,			.blue = 0xffff,	.alpha = 0xffff };
  XRenderColor cyan    = { .red = 0x0,    .green = 0xffff,	.blue = 0xffff,	.alpha = 0xffff };
  XRenderColor black   = { .red = 0x0,    .green = 0x0,			.blue = 0x0,		.alpha = 0xaaaa };

	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &white, &COLOR_WHITE);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &red, &COLOR_RED);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &green, &COLOR_GREEN);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &blue, &COLOR_BLUE);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &yellow, &COLOR_YELLOW);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &magenta, &COLOR_MAGENTA);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &cyan, &COLOR_CYAN);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &black, &COLOR_BLACK);
}

/* setup x window */
void
setup_x(int x, int y, int w, int h, const char *font)
{
   XSetWindowAttributes x11_window_attributes;

   /* open display */
   if (!(XINFO.disp = XOpenDisplay(NULL)))
      errx(1, "can't open X11 display.");

   /* setup various defaults/settings */
   XINFO.screen = DefaultScreen(XINFO.disp);
   XINFO.width  = w;
   XINFO.height = h;
   XINFO.depth  = DefaultDepth(XINFO.disp, XINFO.screen);
   XINFO.vis    = DefaultVisual(XINFO.disp, XINFO.screen);
	 x11_window_attributes.override_redirect = 1;

   /* create window */
   XINFO.win = XCreateWindow(
      XINFO.disp, DefaultRootWindow(XINFO.disp),
      x, y,
      XINFO.width, XINFO.height,
      1,
      CopyFromParent, InputOutput, XINFO.vis,
      CWOverrideRedirect, &x11_window_attributes
   );

   XINFO.backbuf = XdbeAllocateBackBufferName(XINFO.disp, XINFO.win, XdbeBackground);
   XINFO.xftdraw = XftDrawCreate(XINFO.disp, XINFO.backbuf,
                                 DefaultVisual(XINFO.disp,XINFO.screen),
                                 DefaultColormap( XINFO.disp, XINFO.screen ) );

   /* setup font */
   XINFO.font = XftFontOpenName(XINFO.disp, XINFO.screen, font); 
   if (!XINFO.font)
     errx(1, "XLoadQueryFont failed for \"%s\"", font);

   /* connect window to display */
   XMapWindow(XINFO.disp, XINFO.win);

	 setup_colors();
}

void
swap_buf()
{
  XdbeSwapInfo swpinfo[1] = {{XINFO.win, XdbeBackground}};
  XdbeSwapBuffers(XINFO.disp, swpinfo, 1);
}

/* draw all stats */
void
draw(int consolidate_cpus)
{
   static int spacing = 10;
   int x, y;
   int cpu;
 
   /* paint over the existing pixmap */
   swap_buf();
   XftDrawRect(XINFO.xftdraw, &COLOR_BLACK, 0, 0, XINFO.width, XINFO.height);

   /* determine starting x and y */
   y = XINFO.height - XINFO.font->descent;
   x = 0;

   /* start drawing stats */
   if (consolidate_cpus)
      x += cpu_draw(-1, &COLOR_WHITE, x, y) + spacing;
   else
      for (cpu = 0; cpu < sysinfo.ncpu; cpu++)
         x += cpu_draw(cpu, &COLOR_WHITE, x, y) + spacing;

   x += mem_draw(&COLOR_WHITE, x, y) + spacing;
   x += procs_draw(&COLOR_WHITE, x, y) + spacing;
   x += power_draw(&COLOR_WHITE, x, y) + spacing;
   x += volume_draw(&COLOR_WHITE, x, y) + spacing;
   time_draw(&COLOR_YELLOW, x, y);

   swap_buf();
   XFlush(XINFO.disp);
}

