TARGET = huffx

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS = 

SRCS = huffx_cli.c huffx_core.c huffx_file.c huffx_crypto.c

OBJS = $(SRCS:%.c=obj/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

obj/%.o: %.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p obj

clean:
	rm -rf obj $(TARGET)

re: clean all

.PHONY: all clean re