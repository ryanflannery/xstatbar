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

  XftColorFree(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &COLOR0);
  XftColorFree(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &COLOR1);
  XftColorFree(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &COLOR2);
  XftColorFree(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &COLOR3);
  XftColorFree(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &COLOR4);
  XftColorFree(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &COLOR5);
  XftColorFree(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &COLOR6);
  XftColorFree(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), &COLOR7);


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

XRenderColor*
hex_to_color(const char *hex)
{
  if (strlen(hex) < 7) {
    return NULL;
  } 

  XRenderColor *color = malloc(sizeof(XRenderColor));
  long number = strtoll( &hex[1], NULL, 16);
  long r = number >> 16;
  long g = number >> 8 & 0xFF;
  long b = number & 0xFF;


  color->red = ((r + 1) * 64 * 16) - 1;
  color->green = ((g + 1) * 64 * 16) - 1;
  color->blue = ((b + 1) * 64 * 16) - 1;
  color->alpha = 0xffff;

  return color;
}

void
calc_color(const char *name, XRenderColor *def, XftColor *col)
{
  const char *color;
  color = get_resource(name);
  XftColor lookup_color;
  XRenderColor *hex_color;

  if (color) {
    if (color[0] == '#') {
      hex_color = hex_to_color(color);
      if (XftColorAllocValue(XINFO.disp, 
            XINFO.vis, 
            DefaultColormap( XINFO.disp, XINFO.screen ), 
            hex_color, 
            &lookup_color) ) {
        *col = lookup_color;
        return;
      }
     } else {
      if (XftColorAllocName(XINFO.disp, 
            XINFO.vis, 
            DefaultColormap( XINFO.disp, XINFO.screen ), 
            color, 
            &lookup_color) ) {
        *col = lookup_color;
        return;
      } else {
      }
    }
  }

  XftColorAllocValue(XINFO.disp, XINFO.vis, DefaultColormap( XINFO.disp, XINFO.screen ), def, col);
}

/* setup all colors used */
void
setup_colors()
{
  XRenderColor color0  = { .red = 0x0,    .green = 0x0,    .blue = 0x0,    .alpha = 0xffff };
  XRenderColor color1  = { .red = 0xffff, .green = 0x0,    .blue = 0x0,    .alpha = 0xffff };
  XRenderColor color2  = { .red = 0x0,    .green = 0xf000, .blue = 0x0,    .alpha = 0xffff };
  XRenderColor color3  = { .red = 0xffff, .green = 0xffff, .blue = 0x0,    .alpha = 0xffff };
  XRenderColor color4  = { .red = 0x0,    .green = 0x0,    .blue = 0xffff, .alpha = 0xffff };
  XRenderColor color5  = { .red = 0xffff, .green = 0x0,    .blue = 0xffff, .alpha = 0xffff };
  XRenderColor color6  = { .red = 0x0,    .green = 0xffff, .blue = 0xffff, .alpha = 0xffff };
  XRenderColor color7  = { .red = 0xffff, .green = 0xffff, .blue = 0xffff, .alpha = 0xffff };

  calc_color("color0", &color0, &COLOR0);
  calc_color("color1", &color1, &COLOR1);
  calc_color("color2", &color2, &COLOR2);
  calc_color("color3", &color3, &COLOR3);
  calc_color("color4", &color4, &COLOR4);
  calc_color("color5", &color5, &COLOR5);
  calc_color("color6", &color6, &COLOR6);
  calc_color("color7", &color7, &COLOR7);
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

