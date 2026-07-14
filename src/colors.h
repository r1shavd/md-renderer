/*
	COLORS

This is a custom header file generated for adding ANSII color themes to the
CLI based tools that we develop. These generally work for Linux/Unix/Darwin
kernel based platforms, but with modern support of Powershell rendering, these
color codes are also usable for windows CLI application.

The color codes are defined as a macros, and are cross platform.
The color codes are blank strings in case of unsupported platforms, thus
feel free to include them in your application without the worry.

steps:
 * copy this header file to include/ or inc/ folder of your project
 * add the line to the source file <file>.c
 	```
	#include "colors.h"
	```
 * use the compiler flags: gcc -I include/ -o <binary> <source-files>

Author: Rishav (github.com/r1shavd)
Creation date: Sep, 2026

*/

#ifndef COLORS_H
#define COLORS_H

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(_WIN32)
    #define DEFAULT "\033[0m"
    #define BLACK   "\033[0;90m"
    #define RED     "\033[0;91m"
    #define GREEN   "\033[0;92m"
    #define YELLOW  "\033[0;93m"
    #define BLUE    "\033[0;94m"
    #define MAGENTA "\033[0;95m"
    #define CYAN    "\033[0;96m"
    #define WHITE   "\033[0;97m"
#else
    #define DEFAULT ""
    #define BLACK   ""
    #define RED     ""
    #define GREEN   ""
    #define YELLOW  ""
    #define BLUE    ""
    #define MAGENTA ""
    #define CYAN    ""
    #define WHITE   ""
#endif

#endif
