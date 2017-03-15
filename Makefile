# install locations
BINDIR=/usr/local/bin
MANDIR=/usr/local/man/man1

# build flags
CC?=/usr/bin/cc
CFLAGS+=-c -std=c99 -Wall -O2 -I/usr/X11R6/include -I/usr/X11R6/include/freetype2
LDFLAGS+=-L/usr/X11R6/lib -lX11 -lXext -lXrender -lXau -lXdmcp -lm -lXft

OBJS=xstatbar.o stats.o

xstatbar: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) $<

install: xstatbar
	/usr/bin/install -c -m 0555 xstatbar $(BINDIR)
	/usr/bin/install -c -m 0444 xstatbar.1 $(MANDIR)

uninstall:
	rm -f $(BINDIR)/xstatbar
	rm -f $(MANDIR)/xstatbar.1

clean:
	rm -f $(OBJS)
	rm -f xstatbar

