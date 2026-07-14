# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -O2 -Isrc -Ilib -Ithird_party -Ithird_party/cmark -Ithird_party/mongoose -pthread

# Target binary path
TARGET = build/md-renderer

# Base files shared by both build variations (lib/cmark replaced with third_party/cmark)
CMARK_SRC = third_party/cmark/blocks.c \
            third_party/cmark/inlines.c \
            third_party/cmark/buffer.c \
            third_party/cmark/node.c \
            third_party/cmark/references.c \
            third_party/cmark/render.c \
            third_party/cmark/html.c \
            third_party/cmark/scanners.c \
            third_party/cmark/utf8.c \
            third_party/cmark/iterator.c \
            third_party/cmark/cmark.c \
            third_party/cmark/cmark_ctype.c \
            third_party/cmark/houdini_html_e.c \
            third_party/cmark/houdini_href_e.c \
            third_party/cmark/houdini_html_u.c

# Dynamic object mapping function for clean build subfolders
define TO_BUILD_OBJ
$(patsubst src/%.c,build/main/%.o,\
$(patsubst third_party/cmark/%.c,build/cmark/%.o,\
$(patsubst third_party/mongoose/%.c,build/mongoose/%.o,$(1))))
endef

# Default build sources (No Mongoose)
DEFAULT_SRC = src/main.c src/parser.c src/helpers.c src/server.c $(CMARK_SRC)
DEFAULT_OBJ = $(call TO_BUILD_OBJ,$(DEFAULT_SRC))

# Mongoose build sources
MONGOOSE_SRC = src/main.c src/parser.c src/helpers.c src/server_mongoose.c third_party/mongoose/mongoose.c $(CMARK_SRC)
MONGOOSE_OBJ = $(call TO_BUILD_OBJ,$(MONGOOSE_SRC))

# Default rule (make)
all: $(TARGET)

# Standard target rule using default objects
$(TARGET): $(DEFAULT_OBJ)
	@mkdir -p $(dir $(TARGET))
	$(CC) $(CFLAGS) -o $(TARGET) $(DEFAULT_OBJ)

# Mongoose option rule (make mongoose)
mongoose: CFLAGS += -DMONGOOSE
mongoose: $(MONGOOSE_OBJ)
	@mkdir -p $(dir $(TARGET))
	$(CC) $(CFLAGS) -o $(TARGET) $(MONGOOSE_OBJ)

# Rule to compile src/ app files into build/main/
build/main/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile third_party/cmark/ files into build/cmark/
build/cmark/%.o: third_party/cmark/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile third_party/mongoose/ files into build/mongoose/
build/mongoose/%.o: third_party/mongoose/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Cleaning up build folder entirely
clean:
	rm -rf build src/*.swp include/*.swp

.PHONY: all mongoose clean
