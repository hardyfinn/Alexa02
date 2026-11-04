# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

# The LXR sound engine (C, from the open-source Erica Synths firmware)
LXR_SOURCES := $(shell find src/lxr/Src -name '*.c')
SOURCES += $(LXR_SOURCES)
SOURCES += $(wildcard src/*.cpp)

# the engine's own include folders, for the C files only
CFLAGS += -Isrc/lxr/port -Isrc/lxr/Inc -std=gnu11 -Wno-unused-variable -Wno-unused-but-set-variable \
	-Wno-sign-compare -Wno-missing-field-initializers -Wno-unused-function
# the C++ module only needs the bridge header
CXXFLAGS += -Isrc/lxr/port

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*) NOTICE.md

include $(RACK_DIR)/plugin.mk

# Renders every voice through the engine and checks that two modules don't
# disturb each other. Needs only a C compiler: make test
.PHONY: test
test:
	mkdir -p build/test
	$(CC) -std=gnu11 -O2 -Isrc/lxr/port -Isrc/lxr/Inc $(LXR_SOURCES) test/engine_test.c -lm -o build/test/engine_test
	build/test/engine_test build/test
