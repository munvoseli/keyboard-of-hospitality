all:
	gcc -g `pkg-config --cflags sdl2` board.c `pkg-config --libs sdl2` -lm
