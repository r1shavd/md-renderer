# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -O2 -Iinclude -Ilib -Ilib/cmark -Ilib/mongoose -pthread

# Target binary path
TARGET = bin/md-renderer

# Base files shared by both build variations
CMARK_SRC = lib/cmark/blocks.c \
            lib/cmark/inlines.c \
            lib/cmark/buffer.c \
            lib/cmark/node.c \
            lib/cmark/references.c \
            lib/cmark/render.c \
            lib/cmark/html.c \
            lib/cmark/scanners.c \
            lib/cmark/utf8.c \
            lib/cmark/iterator.c \
            lib/cmark/cmark.c \
            lib/cmark/cmark_ctype.c \
            lib/cmark/houdini_html_e.c \
            lib/cmark/houdini_href_e.c \
            lib/cmark/houdini_html_u.c

BASE_SRC = src/main.c src/renderer.c $(CMARK_SRC)

# Default build sources (No Mongoose)
DEFAULT_SRC = $(BASE_SRC) src/server.c
DEFAULT_OBJ = $(DEFAULT_SRC:.c=.o)

# Mongoose build sources
MONGOOSE_SRC = $(BASE_SRC) src/server_mongoose.c lib/mongoose/mongoose.c
MONGOOSE_OBJ = $(MONGOOSE_SRC:.c=.o)

# Default rule (make)
all: $(TARGET)

# Standard target rule using default objects
$(TARGET): $(DEFAULT_OBJ)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $(TARGET) $(DEFAULT_OBJ)

# Mongoose option rule (make mongoose)
mongoose: $(MONGOOSE_OBJ)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $(TARGET) $(MONGOOSE_OBJ)

# Rule to compile any .c file into a .o file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleaning up binaries and object files
clean:
	rm -f $(TARGET) src/*.o src/*.swp include/*.swp lib/mongoose/*.o lib/cmark/*.o

.PHONY: all mongoose clean
