CC = gcc
CFLAGS = -Wall -Wextra -g -lpthread
SRC = src/manager.c
OUT = manager

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(OUT)
