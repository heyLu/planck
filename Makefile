.PHONY: build bundle-and-build release bundle-test zip-test planck clean

VERSION = $(shell git describe --tags)

CC = clang

DEPS = javascriptcoregtk-4.0 libzip
CFLAGS = -Wall $(shell pkg-config --cflags $(DEPS)) -DDEBUG
LIBFLAGS = $(shell pkg-config --libs $(DEPS))

SOURCES = $(sort $(wildcard *.c) linenoise.c)
OBJECTS = $(SOURCES:.c=.o)

# Build only ton
build: ton

# Bundle cljs dependencies and build with that
bundle-and-build: planck
	cd planck; ./script/build-c

# Build release tarball
release:
	git archive --prefix=ton-$(VERSION)/ --output=ton-$(VERSION).tar HEAD
	# add bundle.c (with deps) and linenoise
	tar -uf ton-$(VERSION).tar --transform='s/^/ton-$(VERSION)\//' bundle.c linenoise.c linenoise.h
	gzip --force ton-$(VERSION).tar


ton: $(OBJECTS)
	$(CC) $(LIBFLAGS) $(OBJECTS) -o $@

linenoise.c: linenoise.h
	curl -LsSfo $@ https://github.com/antirez/linenoise/raw/master/linenoise.c

linenoise.h:
	curl -LsSfo $@ https://github.com/antirez/linenoise/raw/master/linenoise.h

planck:
	if [ ! -d planck ]; then git clone https://github.com/mfikes/planck; fi
	cp script/bundle-c planck/planck-cljs/script/bundle-c
	cp script/build-c planck/script/build-c

bundle-test:
	$(CC) -lz -DBUNDLE_TEST bundle.c -o $@

zip-test:
	$(CC) $(shell pkg-config --cflags --libs libzip) -DZIP_TEST zip.c -o $@

clean:
	rm -f ton bundle-test zip-test $(OBJECTS)

jsc-funcs:
	grep -Rh JS_EXPORT /usr/include/webkitgtk-4.0/JavaScriptCore | sed 's/^JS_EXPORT //' | grep -v '^#' > $@
