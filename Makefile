PREFIX?=.

LIBS=libpulse libcjson sndfile portmidi
CFLAGS=-Wall -Wpedantic -Wextra -O3 $(shell pkg-config --cflags $(LIBS))
LDFLAGS= -lm -flto $(shell pkg-config --libs $(LIBS))

SOURCEDIR=src
BUILDDIR=build
SOURCES=$(wildcard $(SOURCEDIR)/*.c)
OBJECTS=$(SOURCES:$(SOURCEDIR)/%.c=$(BUILDDIR)/%.o)
DEPENDENCIES=$(SOURCES:$(SOURCEDIR)/%.c=$(BUILDDIR)/%.d)
PROGRAM=anomi

all: $(BUILDDIR)/$(PROGRAM)

debug: CFLAGS+=-g -O0 -Wno-cpp
debug: $(BUILDDIR)/$(PROGRAM)

$(BUILDDIR)/$(PROGRAM): $(OBJECTS) | $(BUILDDIR)
	@printf "  CCLD\t%s\n" $(@)
	@$(CC) $(LDFLAGS) -o $(@) $(^)

-include $(DEPENDENCIES)

$(BUILDDIR)/%.o: $(SOURCEDIR)/%.c | $(BUILDDIR)
	@printf "  CC\t%s\n" $(@)
	@$(CC) -MMD $(CFLAGS) -o $(@) -c $(<)

$(BUILDDIR):
	@mkdir -p $(@)

install:
	install -Dm755 $(BUILDDIR)/$(PROGRAM) $(PREFIX)/bin/$(PROGRAM)

clean:
	rm -rf $(BUILDDIR)

.PHONY: clean
