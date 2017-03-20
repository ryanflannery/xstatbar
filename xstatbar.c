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

XftColor COLOR0, COLOR1, COLOR2, COLOR3,
         COLOR4, COLOR5, COLOR6, COLOR7;

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
   w = 0;
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
   XrmDestroyDatabase(XINFO.xrdb);
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
	printf("Name: %s \n", name);
	XrmGetResource(XINFO.xrdb, name, class, &type, &value);
	if (value.addr)
		return value.addr;
	return NULL;
}

XRenderColor *
calc_color(const char *name, XRenderColor *def)
{
  const char *color;
	color = get_resource(name);

	if (color) {
    if (color[0] == '#') {
			printf("HEX COLOR\n");
    } else {
			printf("LOOKUP\n");
		}
	} else {
    printf("No resource\n");
	}

	return def;
}

/* setup all colors used */
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

	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), calc_color("color0", &black),   &COLOR0);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &red,     &COLOR1);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &green,   &COLOR2);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &yellow,  &COLOR3);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &blue,    &COLOR4);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &magenta, &COLOR5);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &cyan,    &COLOR6);
	XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &white,   &COLOR7);
}

int
calculate_width_of_default_screen()
{
	int nsizes;
	XRRScreenSize* randrsize = XRRSizes(XINFO.disp, XINFO.screen, &nsizes);

  if (nsizes != 0) {
    Rotation current = 0;
    XRRScreenConfiguration * sc;

    sc = XRRGetScreenInfo (XINFO.disp, RootWindow (XINFO.disp, XINFO.screen));
    int current_size = XRRConfigCurrentConfiguration (sc, &current);

    if (current_size < nsizes) {

      XRRRotations(XINFO.disp, XINFO.screen, &current);
      randrsize += current_size;

      bool rot = current & RR_Rotate_90 || current & RR_Rotate_270;
      return rot ? randrsize->height : randrsize->width;
		}
  }
  return DisplayWidth(XINFO.disp, XINFO.screen);
}

/* setup x window */
void
setup_x(int x, int y, int w, int h, const char *font)
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
   XINFO.height = h;
   XINFO.depth  = DefaultDepth(XINFO.disp, XINFO.screen);
   XINFO.vis    = DefaultVisual(XINFO.disp, XINFO.screen);
   XINFO.width  = w ? w : calculate_width_of_default_screen();
	 x11_window_attributes.override_redirect = 1;

   if(!(XINFO.xrdb = XrmGetDatabase(XINFO.disp))) {
      xrms = XResourceManagerString(XINFO.disp);
      if (xrms)
         XINFO.xrdb = XrmGetStringDatabase(xrms);
			else
				printf("No xrdb\n");

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

   XMoveWindow(XINFO.disp, XINFO.win, x, y);
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
   XftDrawRect(XINFO.xftdraw, &COLOR0, 0, 0, XINFO.width, XINFO.height);


   /* determine starting x and y */
   y = XINFO.height - XINFO.font->descent;
   x = 0;

   /* start drawing stats */
   if (consolidate_cpus)
      x += cpu_draw(-1, &COLOR7, x, y) + spacing;
   else
      for (cpu = 0; cpu < sysinfo.ncpu; cpu++)
         x += cpu_draw(cpu, &COLOR7, x, y) + spacing;

   x += mem_draw(&COLOR7, x, y) + spacing;
   x += procs_draw(&COLOR7, x, y) + spacing;
   x += power_draw(&COLOR7, x, y) + spacing;
   x += volume_draw(&COLOR7, x, y) + spacing;
   time_draw(&COLOR3, x, y);

   swap_buf();
   XFlush(XINFO.disp);
}

