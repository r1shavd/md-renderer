# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -O2 -Iinclude -Ilib -Ilib/cmark -Ilib/mongoose

# Target binary path
TARGET = bin/mdparser

# All source files explicitly listed
SRC = src/main.c src/server.c src/renderer.c \
      lib/mongoose/mongoose.c \
      lib/cmark/blocks.c \
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

# Automatically convert the entire SRC list of .c files into .o files
OBJ = $(SRC:.c=.o)

# Default rule
all: $(TARGET)

# Link ALL object files automatically using $(OBJ)
$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Rule to compile any .c file into a .o file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleaning up binaries and object files
clean:
	rm -f $(TARGET) src/*.o lib/mongoose/*.o lib/cmark/*.o

.PHONY: all clean