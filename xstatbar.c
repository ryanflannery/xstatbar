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
XColor COLOR_RED,    COLOR_GREEN,   COLOR_BLUE,
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
void draw();


int
main (int argc, char *argv[])
{
   const char *errstr;
   char *font;
   char  ch;
   int   x, y, w, h;
   int   sleep_seconds;

   /* set defaults */
   x = 0;
   y = 0;
   w = 1280;
   h = 13;
   font = "*-fixed-*-9-*";
   time_fmt = "%a %d %b %Y %I:%M:%S %p";
   sleep_seconds = 1;

   /* parse command line */
   while ((ch = getopt(argc, argv, "x:y:w:h:s:f:t:T")) != -1) {
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
      draw();

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
   XClearWindow(XINFO.disp,   XINFO.win);
   XFreePixmap(XINFO.disp,    XINFO.buf);
   XDestroyWindow(XINFO.disp, XINFO.win);
   XCloseDisplay(XINFO.disp);

   /* stats teardown */
   volume_close();
   power_close();
   sysinfo_close();

   exit(0);
}

/* setup all colors used */
void
setup_colors()
{
   static char *color_names[] = { "red", "green", "blue", "yellow",
      "magenta", "cyan", "white", "black" };

   static XColor *xcolors[] = { &COLOR_RED, &COLOR_GREEN, &COLOR_BLUE,
      &COLOR_YELLOW, &COLOR_MAGENTA, &COLOR_CYAN,
      &COLOR_WHITE, &COLOR_BLACK };

   const int num_colors = 8;
   int i;
   Colormap cm;

   cm = DefaultColormap(XINFO.disp, 0);

   for (i = 0; i < num_colors; i++) {
      if (XParseColor(XINFO.disp, cm, color_names[i], xcolors[i]) == 0)
         errx(1, "failed to parse color \"%s\"", color_names[i]);

      if (XAllocColor(XINFO.disp, cm, xcolors[i]) == 0)
         errx(1, "failed to allocate color \"%s\"", color_names[i]);
   }
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
   XINFO.gc     = DefaultGC(XINFO.disp, XINFO.screen);
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

   /* create pixmap used for double buffering */
   XINFO.buf = XCreatePixmap(
      XINFO.disp,  DefaultRootWindow(XINFO.disp),
      XINFO.width, XINFO.height,
      XINFO.depth
   );

   /* setup font */
   XINFO.font = XLoadQueryFont(XINFO.disp, font);
   if (!XINFO.font)
      errx(1, "XLoadQueryFont failed for \"%s\"", font);

   XSetFont(XINFO.disp, XINFO.gc, XINFO.font->fid);

   /* connect window to display */
   XMapWindow(XINFO.disp, XINFO.win);

   /* setup colors */
   setup_colors();
}

/* draw a simple divider between rendered stats */
int
draw_divider(XColor color, int x, int width)
{
   XSetForeground(XINFO.disp, XINFO.gc, color.pixel);
   XFillRectangle(XINFO.disp, XINFO.buf, XINFO.gc,
      x + 1, 0, width, XINFO.height);

   return width + 2;
}

/* draw all stats */
void
draw()
{
   static int spacing = 10;
   int x, y;
   int cpu;

   /* paint over the existing pixmap */
   XSetForeground(XINFO.disp, XINFO.gc, BlackPixel(XINFO.disp, XINFO.screen));
   XFillRectangle(XINFO.disp, XINFO.buf, XINFO.gc,
      0, 0, XINFO.width, XINFO.height);

   /* determine starting x and y */
   y = XINFO.height - XINFO.font->descent;
   x = 0;

   /* start drawing stats */
   for (cpu = 0; cpu < sysinfo.ncpu; cpu++)
      x += cpu_draw(cpu, COLOR_WHITE, x, y) + spacing;

   x += mem_draw(COLOR_WHITE, x, y) + spacing;
   x += procs_draw(COLOR_WHITE, x, y) + spacing;
   x += power_draw(COLOR_WHITE, x, y) + spacing;
   x += volume_draw(COLOR_WHITE, x, y) + spacing;
   time_draw(COLOR_CYAN, x, y);

   /* copy the buffer to the window and flush */
   XCopyArea(XINFO.disp, XINFO.buf, XINFO.win, XINFO.gc,
      0, 0, XINFO.width, XINFO.height, 0, 0);
   XFlush(XINFO.disp);
}

