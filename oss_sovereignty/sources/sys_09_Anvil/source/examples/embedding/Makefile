







EMBED_DIR = micropython_embed
PROG = embed

CFLAGS += -I.
CFLAGS += -I$(EMBED_DIR)
CFLAGS += -I$(EMBED_DIR)/port
CFLAGS += -Wall -Og -fno-common

SRC += main.c
SRC += $(wildcard $(EMBED_DIR)/*/*.c) $(wildcard $(EMBED_DIR)/*/*/*.c)
OBJ += $(SRC:.c=.o)

$(PROG): $(OBJ)
	$(CC) -o $@ $^

clean:
	/bin/rm -f $(OBJ) $(PROG)
