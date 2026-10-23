#ifndef VFSLOG_H
#define VFSLOG_H

#include <stdio.h>

#include "sqlite3.h"

int vfslog_register(const char *name, FILE *out, int make_default);

#endif
