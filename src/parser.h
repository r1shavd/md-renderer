#ifndef RENDERER_H
#define RENDERER_H

#include "cmark/cmark.h"
#include "colors.h"

int parse_markdown(const char* markdownfile, const char* htmlfile);

#endif    /* RENDERER_H */
