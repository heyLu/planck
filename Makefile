CC = clang

DEPS = javascriptcoregtk-4.0 libzip
CFLAGS = $(shell pkg-config --cflags $(DEPS)) -DDEBUG
LIBFLAGS = $(shell pkg-config --libs $(DEPS))

SOURCES = $(sort $(wildcard *.c) linenoise.c)
OBJECTS = $(SOURCES:.c=.o)

ton: $(OBJECTS)
	$(CC) $(LIBFLAGS) $(OBJECTS) -o $@

linenoise.c: linenoise.h
	curl -LsSfo $@ https://github.com/antirez/linenoise/raw/master/linenoise.c

linenoise.h:
	curl -LsSfo $@ https://github.com/antirez/linenoise/raw/master/linenoise.h

clean:
	rm -f ton $(OBJECTS)

jsc-funcs:
	grep -Rh JS_EXPORT /usr/include/webkitgtk-4.0/JavaScriptCore | sed 's/^JS_EXPORT //' | grep -v '^#' > $@
