#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "imud.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_defaults(struct imud_config *cfg) {
    memset(cfg, 0, sizeof *cfg);
    snprintf(cfg->source, sizeof cfg->source, "/dev/mdr_imu");
    snprintf(cfg->host, sizeof cfg->host, "127.0.0.1");
    snprintf(cfg->port, sizeof cfg->port, "1883");
    snprintf(cfg->device_id, sizeof cfg->device_id, "mdr-dk1-01");
    snprintf(cfg->topic_prefix, sizeof cfg->topic_prefix, "mdr");
    cfg->period_ms = 1000;
    cfg->sample_rate = 100;
    cfg->alpha = 0.98;
}

static char *trim(char *s) {
    char *end;

    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return s;
}

/* 解析一行 key = value；空行与 # 开头的行跳过 */
static int apply_line(struct imud_config *cfg, char *line) {
    char *eq = strchr(line, '=');
    char *key;
    char *val;

    if (eq == NULL) {
        return 0;
    }
    *eq = '\0';
    key = trim(line);
    val = trim(eq + 1);
    if (*key == '\0' || *key == '#') {
        return 0;
    }

    if (strcmp(key, "source") == 0) {
        snprintf(cfg->source, sizeof cfg->source, "%s", val);
    } else if (strcmp(key, "host") == 0) {
        snprintf(cfg->host, sizeof cfg->host, "%s", val);
    } else if (strcmp(key, "port") == 0) {
        snprintf(cfg->port, sizeof cfg->port, "%s", val);
    } else if (strcmp(key, "device_id") == 0) {
        snprintf(cfg->device_id, sizeof cfg->device_id, "%s", val);
    } else if (strcmp(key, "topic_prefix") == 0) {
        snprintf(cfg->topic_prefix, sizeof cfg->topic_prefix, "%s", val);
    } else if (strcmp(key, "period_ms") == 0) {
        long v = strtol(val, NULL, 10);

        if (v < 10 || v > 3600000) {
            fprintf(stderr, "配置错误: period_ms = %ld 超出范围\n", v);
            return -1;
        }
        cfg->period_ms = (int)v;
    } else if (strcmp(key, "sample_rate") == 0) {
        long v = strtol(val, NULL, 10);

        if (v < 4 || v > 1000) {
            fprintf(stderr, "配置错误: sample_rate = %ld 超出范围\n", v);
            return -1;
        }
        cfg->sample_rate = (int)v;
    } else if (strcmp(key, "alpha") == 0) {
        double v = strtod(val, NULL);

        if (v < 0.0 || v >= 1.0) {
            fprintf(stderr, "配置错误: alpha = %f 超出范围\n", v);
            return -1;
        }
        cfg->alpha = v;
    } else {
        fprintf(stderr, "配置里未识别的项: %s\n", key);
    }
    return 0;
}

int config_load(const char *path, struct imud_config *cfg) {
    FILE *fp = fopen(path, "r");
    char line[512];
    struct imud_config next;

    if (fp == NULL) {
        return -1;
    }
    next = *cfg;
    while (fgets(line, sizeof line, fp) != NULL) {
        if (apply_line(&next, line) < 0) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    *cfg = next;
    return 0;
}