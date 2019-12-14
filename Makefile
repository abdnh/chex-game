.POSIX:
CC = cc
CFLAGS = -W -O
LDLIBS = -lallegro -lallegro_primitives -lallegro_font -lallegro_ttf -lallegro_image

all: chex-game
chex-game: hex-game.o weighted-quick-union.o
	$(CC) $(LDFLAGS) -o chex-game hex-game.o weighted-quick-union.o $(LDLIBS)
hex-game.o: hex-game.c
weighted-quick-union.o : weighted-quick-union.c weighted-quick-union.h

clean:
	rm -f chex-game hex-game.o weighted-quick-union.o
