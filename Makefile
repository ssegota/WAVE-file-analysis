CC=gcc
CFLAGS=-Wall
LIBS= -lsndfile -lm -lSPTK

make: projekt_sandi_segota.c
	$(CC) $(CFLAGS) projekt_sandi_segota.c $(LIBS) -o projekt_sandi_segota
