# Makefile para Quoridor Pac-Man (versión un solo archivo) — Linux
# Si compilaste raylib a mano en otro lugar, ajustá RAYLIB_PATH.

RAYLIB_PATH = /usr/local

CC      = gcc
CFLAGS  = -Wall -I$(RAYLIB_PATH)/include
LDFLAGS = -L$(RAYLIB_PATH)/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

OUT = quoridor_pacman

all: $(OUT)

$(OUT): quoridor_pacman.c
	$(CC) quoridor_pacman.c $(CFLAGS) $(LDFLAGS) -o $(OUT)

clean:
	rm -f $(OUT)
