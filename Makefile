.POSIX:

LIB_MAJOR = 1
LIB_MINOR = 0
LIB_VERSION = $(LIB_MAJOR).$(LIB_MINOR)


CONFIGFILE = config.mk
include $(CONFIGFILE)

OS = linux
# Linux:   linux
# Mac OS:  macos
# Windows: windows
include mk/$(OS).mk



all: libparser.a libparser.$(LIBEXT) libparser-generate calc-example/calc
libparser.o: libparser.c libparser.h
libparser.lo: libparser.c libparser.h
calc-example/calc-syntax.o: calc-example/calc-syntax.c libparser.h

.c.o:
	$(CC) -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

.c.lo:
	$(CC) -fPIC -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

libparser-generate: libparser-generate.o
	$(CC) -o $@ libparser-generate.o $(LDFLAGS)

libparser.a: libparser.o
	$(AR) rc $@ libparser.o
	$(AR) -s $@

libparser.$(LIBEXT): libparser.lo
	$(CC) $(LIBFLAGS) -o $@ libparser.lo $(LDFLAGS)

calc-example/calc: calc-example/calc.o calc-example/calc-syntax.o libparser.a
	$(CC) -o $@ calc-example/calc.o calc-example/calc-syntax.o libparser.a $(LDFLAGS)

calc-example/calc-syntax.c: libparser-generate calc-example/calc.syntax
	./libparser-generate _expr < calc-example/calc.syntax > $@

install: libparser.a libparser.$(LIBEXT) libparser-generate
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/include"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man1/"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man3/"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man7/"
	cp -- libparser-generate "$(DESTDIR)$(PREFIX)/bin"
	cp -- libparser.a "$(DESTDIR)$(PREFIX)/lib"
	cp -- libparser.$(LIBEXT) "$(DESTDIR)$(PREFIX)/lib/libparser.$(LIBMINOREXT)"
	ln -sf -- libparser.$(LIBMINOREXT) "$(DESTDIR)$(PREFIX)/lib/libparser.$(LIBMAJOREXT)"
	ln -sf -- libparser.$(LIBMAJOREXT) "$(DESTDIR)$(PREFIX)/lib/libparser.$(LIBEXT)"
	cp -- libparser.h "$(DESTDIR)$(PREFIX)/include"
	cp -- libparser-generate.1 "$(DESTDIR)$(MANPREFIX)/man1/"
	cp -- libparser_parse_file.3 "$(DESTDIR)$(MANPREFIX)/man3/"
	cp -- libparser.7 "$(DESTDIR)$(MANPREFIX)/man7/"

uninstall:
	-rm -f -- "$(DESTDIR)$(PREFIX)/bin/libparser-generate"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparser.a"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparser.$(LIBMAJOREXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparser.$(LIBMINOREXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparser.$(LIBEXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/include/libparser.h"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man1/libparser-generate.1"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man3/libparser_parse_file.3"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man7/libparser.7"

clean:
	-rm -f -- *.o *.lo *.a *.so *.su *.dylib *.dll *-example/*.o *-example/*.su *-example/*-syntax.c
	-rm -f -- libparser-generate calc-example/calc

.SUFFIXES:
.SUFFIXES: .c .o .lo

.PHONY: all install uninstall clean
