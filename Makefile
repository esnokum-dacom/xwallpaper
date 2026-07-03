CFLAGS += -std=c99 -O -Wall -Wextra -pedantic -Wold-style-declaration -Wmissing-prototypes -Wno-unused-parameter
CLIBS  += -lX11 -lXinerama -lXft -lGL $(shell pkg-config --cflags --libs xft imlib2)
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
CC     ?= gcc

all: xwall

config.h:
	cp config.def.h config.h

xwall: xwall.c xwall.h config.h Makefile
	$(CC) -O3 $(CFLAGS) -o xwall xwall.c $(CLIBS) $(LDFLAGS) 

install: all
	install -Dm755 xwall $(DESTDIR)$(BINDIR)/xwall

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/xwall

clean:
	rm -f xwall *.o config.h

.PHONY: all install uninstall clean

