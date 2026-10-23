#define _POSIX_C_SOURCE 200809L

#ifndef IMUD_CONFIG_H
#define IMUD_CONFIG_H

#include "imud.h"

void config_defaults(struct imud_config *cfg);
int config_load(const char *path, struct imud_config *cfg);

#endif
