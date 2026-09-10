CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

municipios: municipios.c
	$(CC) $(CFLAGS) -o municipios municipios.c

clean:
	rm -f municipios municipios.bin
