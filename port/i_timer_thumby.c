/*
 * ThumbyDOOM — i_timer stub. The actual timer functions are in
 * i_system_thumby.c since they share the time_us_64() source.
 * This file exists in case any vendor caller expects i_timer.c
 * to define its own symbols (none currently — left intentionally
 * empty).
 */
#include "config.h"
