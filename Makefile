.SUFFIXES:
MAKEFLAGS+=-r

config=debug

BUILD=build/$(config)

CFLAGS=-g -std=c99
LDFLAGS=

ifeq ($(config),release)
CFLAGS+=-O3
endif

ifeq ($(config),sanitize)
CFLAGS+=-fsanitize=address
LDFLAGS+=-fsanitize=address
endif

ifeq ($(config),coverage)
CFLAGS+=-coverage
LDFLAGS+=-coverage
endif

SOURCES=$(wildcard src/*.c) $(wildcard src/vm/*.c) $(wildcard src/compiler/*.c)
OBJECTS=$(SOURCES:%=$(BUILD)/%.o)

all: $(BUILD)/lusp

test: $(BUILD)/lusp

clean:
	rm -rf $(BUILD)

$(BUILD)/lusp: $(OBJECTS)
	$(CXX) $^ $(LDFLAGS) -o $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $< $(CFLAGS) -c -MMD -MP -o $@

-include $(OBJECTS:.o=.d)

.PHONY: all test clean
