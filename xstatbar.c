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
       COLOR_WHITE,  COLOR_BLACK, COLOR_GREY;


/* signal flags */
volatile sig_atomic_t VSIG_QUIT = 0;


/* local functions */
void signal_handler(int sig);
void process_signals();
void cleanup();
void usage(const char *pname);
void setup_x(int x, int y, int w, int h, int b, const char *font);
void draw();

struct applet {
   int (*draw) (XColor, int, int);
   XColor *color;
} applets[] = {
   { cpu_draw,    &COLOR_WHITE },
   { mem_draw,    &COLOR_WHITE },
   { procs_draw,  &COLOR_WHITE },
   { power_draw,  &COLOR_WHITE },
   { volume_draw, &COLOR_WHITE },
   { time_draw,   &COLOR_CYAN  },
   { border_draw, &COLOR_GREY  },
};

int
main (int argc, char *argv[])
{
   const char *errstr;
   char *font;
   char  ch;
   int   x, y, w, h, b;
   int   sleep_seconds;

   /* set defaults */
   x = 0;
   y = 0;
   w = 0;
   h = 13;
   b = 0;
   font = "*-fixed-*-9-*";
   time_fmt = "%a %d %b %Y %I:%M:%S %p";
   sleep_seconds = 1;

   /* parse command line */
   while ((ch = getopt(argc, argv, "b:x:y:w:h:s:f:t:T")) != -1) {
      switch (ch) {
	 case 'b':
            b = strtonum(optarg, 0, INT_MAX, &errstr);
            if (errstr)
               errx(1, "illegal b value \"%s\": %s", optarg, errstr);
            break;

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
   setup_x(x, y, w, h, b, font);

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
   XrmDestroyDatabase(XINFO.xrdb);
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

/* get resource from X Resource database */
const char *
get_resource(const char *resource)
{
	static char name[256], class[256], *type;
	XrmValue value;

	if (!XINFO.xrdb)
		return NULL;
#define RESCLASS "xstatbar"
#define RESNAME "XStatBar"
	snprintf(name, sizeof(name), "%s.%s", RESNAME, resource);
	snprintf(class, sizeof(class), "%s.%s", RESCLASS, resource);
	XrmGetResource(XINFO.xrdb, name, class, &type, &value);
	if (value.addr)
		return value.addr;
	return NULL;
}

/* setup all colors used */
void
setup_colors()
{
   static char *color_names[] = { "red", "green", "blue", "yellow",
      "magenta", "cyan", "white", "black", "grey" };

   static XColor *xcolors[] = { &COLOR_RED, &COLOR_GREEN, &COLOR_BLUE,
      &COLOR_YELLOW, &COLOR_MAGENTA, &COLOR_CYAN,
      &COLOR_WHITE, &COLOR_BLACK, &COLOR_GREY };

   const int num_colors = 9;
   const char *color;
   char resname[8];
   int i;
   Colormap cm;

   cm = DefaultColormap(XINFO.disp, 0);

   for (i = 0; i < num_colors; i++) {
      snprintf(resname, sizeof(resname), "color%d", i);
      color = get_resource(resname);
      if (XParseColor(XINFO.disp, cm, color ? color : color_names[i], xcolors[i]) == 0)
         errx(1, "failed to parse color \"%s\"", color ? color : color_names[i]);

      if (XAllocColor(XINFO.disp, cm, xcolors[i]) == 0)
         errx(1, "failed to allocate color \"%s\"", color ? color : color_names[i]);
   }
}

/* setup x window */
void
setup_x(int x, int y, int w, int h, int b, const char *font)
{
   XSetWindowAttributes x11_window_attributes;
   Atom type;
   unsigned long struts[12];
   char *xrms = NULL;

   /* open display */
   if (!(XINFO.disp = XOpenDisplay(NULL)))
      errx(1, "can't open X11 display.");
   /* initialize resource manager */
   XrmInitialize();
   /* setup various defaults/settings */
   XINFO.screen = DefaultScreen(XINFO.disp);
   XINFO.width  = w ? w : DisplayWidth(XINFO.disp, XINFO.screen);
   XINFO.height = h;
   XINFO.border = b;
   XINFO.depth  = DefaultDepth(XINFO.disp, XINFO.screen);
   XINFO.vis    = DefaultVisual(XINFO.disp, XINFO.screen);
   XINFO.gc     = DefaultGC(XINFO.disp, XINFO.screen);
   x11_window_attributes.override_redirect = 0;
   if(!(XINFO.xrdb = XrmGetDatabase(XINFO.disp))) {
      xrms = XResourceManagerString(XINFO.disp);
      if (xrms)
         XINFO.xrdb = XrmGetStringDatabase(xrms);
   }
   /* create window */
   XINFO.win = XCreateWindow(
      XINFO.disp, DefaultRootWindow(XINFO.disp),
      x, y,
      XINFO.width, XINFO.height,
      1,
      CopyFromParent, InputOutput, XINFO.vis,
      CWOverrideRedirect, &x11_window_attributes
   );

   /* setup window manager hints */
   type = XInternAtom(XINFO.disp, "_NET_WM_WINDOW_TYPE_DOCK", False);
   XChangeProperty(XINFO.disp, XINFO.win, XInternAtom(XINFO.disp, "_NET_WM_WINDOW_TYPE", False),
		   XA_ATOM, 32, PropModeReplace, (unsigned char*)&type, 1);
   bzero(struts, sizeof(struts));
   enum { left, right, top, bottom, left_start_y, left_end_y, right_start_y,
	  right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x };
   if (y <= DisplayHeight(XINFO.disp, XINFO.screen)/2) {
	   struts[top] = y + XINFO.height;
	   struts[top_start_x] = x;
	   struts[top_end_x] = x + XINFO.width;
   } else {
	   struts[bottom] = DisplayHeight(XINFO.disp, XINFO.screen) - y;
	   struts[bottom_start_x] = x;
	   struts[bottom_end_x] = x + XINFO.width;
   }
   XChangeProperty(XINFO.disp, XINFO.win, XInternAtom(XINFO.disp, "_NET_WM_STRUT_PARTIAL", False),
		   XA_CARDINAL, 32, PropModeReplace, (unsigned char*)struts, 12);

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

   XStoreName(XINFO.disp, XINFO.win, "xstatbar");
   /* connect window to display */
   XMapWindow(XINFO.disp, XINFO.win);

   XMoveWindow(XINFO.disp, XINFO.win, x, y);
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
   int x, y, h, i;

   /* paint over the existing pixmap */
   XSetForeground(XINFO.disp, XINFO.gc, COLOR_BLACK.pixel);
   XFillRectangle(XINFO.disp, XINFO.buf, XINFO.gc,
      0, 0, XINFO.width, XINFO.height);

   /* determine starting x and y */
   h = XINFO.font->ascent + XINFO.font->descent;
   y = (XINFO.height / 2) - (h / 2) + XINFO.font->ascent;
   x = 2;

   /* draw stats */
   for (i = 0; i < sizeof(applets)/sizeof(applets[0]); i++)
      x += applets[i].draw(*(applets[i].color), x, y) + spacing;

   /* copy the buffer to the window and flush */
   XCopyArea(XINFO.disp, XINFO.buf, XINFO.win, XINFO.gc,
      0, 0, XINFO.width, XINFO.height, 0, 0);
   XFlush(XINFO.disp);
}

