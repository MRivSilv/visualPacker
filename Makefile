# VisualPacker
#
#   make        build            -> ./visualpacker
#   make run    build and run
#   make debug  sanitizer build  -> ./visualpacker-debug
#   make clean
#
# Dependencies: gtk4 (the GUI) and libalpm (pacman's own C library).

CC     ?= gcc
PREFIX ?= /usr/local

SRC := src/main.c src/gui.c src/packages.c
HDR := $(wildcard src/*.h)

PKGS       := gtk4 libalpm
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell pkg-config --libs $(PKGS))

ifeq ($(strip $(PKG_LIBS)),)
  $(error Missing gtk4 or libalpm. On Arch: sudo pacman -S gtk4 pacman)
endif

STD  := -std=c11
WARN := -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes \
        -Wmissing-prototypes -Wwrite-strings -Wvla -Wno-unused-parameter
BASE := $(STD) $(WARN)

# Hardening: cheap at runtime, and it turns several classes of memory bug
# from silent corruption into an immediate, obvious crash.
REL_CFLAGS  := -O2 -DNDEBUG -D_FORTIFY_SOURCE=3 -fstack-protector-strong \
               -fstack-clash-protection -fcf-protection=full
REL_LDFLAGS := -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack

# AddressSanitizer finds use-after-free, overruns and leaks;
# UndefinedBehaviorSanitizer finds overflow and bad pointer arithmetic.
DBG_CFLAGS := -Og -g3 -Werror -fno-omit-frame-pointer \
              -fsanitize=address,undefined

.PHONY: all run debug clean install

all: visualpacker

visualpacker: $(SRC) $(HDR)
	$(CC) $(BASE) $(REL_CFLAGS) $(PKG_CFLAGS) $(SRC) -o $@ $(PKG_LIBS) $(REL_LDFLAGS)

visualpacker-debug: $(SRC) $(HDR)
	$(CC) $(BASE) $(DBG_CFLAGS) $(PKG_CFLAGS) $(SRC) -o $@ $(PKG_LIBS)

run: visualpacker
	./visualpacker

debug: visualpacker-debug

install: visualpacker
	install -Dm755 visualpacker $(DESTDIR)$(PREFIX)/bin/visualpacker

clean:
	rm -f visualpacker visualpacker-debug
