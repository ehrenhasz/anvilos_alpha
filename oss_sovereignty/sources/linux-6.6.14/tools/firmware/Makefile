


CFLAGS = -Wall -Wextra -g

all: ihex2fw
%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) ihex2fw

.PHONY: all clean
