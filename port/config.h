/*
 * ThumbyDOOM — minimal config.h replacement.
 *
 * Stand-in for the autotools-generated config.h vendor/rp2040-doom
 * normally produces. Strips out everything we don't need (libpng,
 * samplerate, mmap) since we're a bare-metal build with no host libs.
 */
#ifndef _THUMBY_CONFIG_H
#define _THUMBY_CONFIG_H

#define PACKAGE_NAME      "ThumbyDOOM"
#define PACKAGE_TARNAME   "thumbydoom"
#define PACKAGE_VERSION   "0.1"
#define PACKAGE_STRING    "ThumbyDOOM 0.1"
#define PROGRAM_PREFIX    "thumby-"

#define HAVE_DECL_STRCASECMP  1
#define HAVE_DECL_STRNCASECMP 1

#endif
