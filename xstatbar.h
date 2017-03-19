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

#ifndef XSTATBAR_H
#define XSTATBAR_H

/* X */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xdbe.h>
#include <X11/extensions/Xrandr.h>

/* structure to wrap all necessary x stuff */
typedef struct xinfo {
   Display       *disp;
   Window         win;
   Visual        *vis;
   XftFont       *font;
   XftDraw			 *xftdraw;
   XdbeBackBuffer backbuf;

   int            screen;
   int            depth;
   unsigned int   width;
   unsigned int   height;
} xinfo_t;
extern xinfo_t XINFO;


/* the actual x-color object globals */
extern XftColor COLOR_RED,    COLOR_GREEN,   COLOR_BLUE,
              COLOR_YELLOW, COLOR_MAGENTA, COLOR_CYAN,
              COLOR_WHITE,  COLOR_BLACK;

#endif
