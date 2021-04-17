PREFIX    = /usr
MANPREFIX = $(PREFIX)/share/man

CC = cc

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -I"$$(pwd)"
CFLAGS   = -std=c99 -Wall -O2
LDFLAGS  = -s
