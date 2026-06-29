#ifndef RENDERER_H
#define RENDERER_H

#include "cmark/cmark.h"
#include "colors.h"

int renderMarkdownFile(const char* markdownfile, char* htmlfile);

#endif /* RENDERER_H */