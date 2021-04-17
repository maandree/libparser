.POSIX:

CONFIGFILE = config.mk
include $(CONFIGFILE)


all: libparser.a libparser-generate calc-example/calc
libparser.o: libparser.c libparser.h
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

calc-example/calc: calc-example/calc.o calc-example/calc-syntax.o libparser.a
	$(CC) -o $@ calc-example/calc.o calc-example/calc-syntax.o libparser.a $(LDFLAGS)

calc-example/calc-syntax.c: libparser-generate calc-example/calc.syntax
	./libparser-generate _expr < calc-example/calc.syntax > $@

install: libparser.a libparser-generate
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/include"
	cp -- libparser-generate "$(DESTDIR)$(PREFIX)/bin"
	cp -- libparser.a "$(DESTDIR)$(PREFIX)/lib"
	cp -- libparser.h "$(DESTDIR)$(PREFIX)/include"

uninstall:
	-rm -f -- "$(DESTDIR)$(PREFIX)/bin/libparser-generate"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparser.a"
	-rm -f -- "$(DESTDIR)$(PREFIX)/include/libparser.h"

clean:
	-rm -f -- *.o *.lo *.a *.so *.su *-example/*.o *-example/*.su *-example/*-syntax.c libparser-generate
	-rm -f -- calc-example/calc

.SUFFIXES:
.SUFFIXES: .c .o .lo

.PHONY: all install uninstall clean
