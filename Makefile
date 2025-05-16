CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -D_GNU_SOURCE

all: guess_number_signals guess_number_fifo

guess_number_signals: guess_number_signals.c
	$(CC) $(CFLAGS) -o $@ $<

guess_number_fifo: guess_number_fifo.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f guess_number_signals guess_number_fifo
	rm -f /tmp/guess_number_fifo1 /tmp/guess_number_fifo2

.PHONY: all clean
