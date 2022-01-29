CC = gcc
CFLAGS = -IC:/raylib/src
LDFLAGS = -LC:/raylib/src

LDLIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
#LDLIBS += -static -lpthread

PLATFORM ?= PC

ifeq ($(PLATFORM), WEB)
	CC = emcc
	LDFLAGS = -LC:/raylib/web -s USE_GLFW=3 -s WASM=1 --preload-file data --shell-file minshell.html
	LDLIBS = -lraylib
	CFLAGS += -DPLATFORM_WEB
endif

build:
ifeq ($(PLATFORM), WEB)
	$(CC) -o index.html src/main.c $(CFLAGS) $(LDFLAGS) $(LDLIBS) -Wall
else
	$(CC) src/main.c $(CFLAGS) $(LDFLAGS) $(LDLIBS) -Wall -MMD -MP -o game.exe
endif