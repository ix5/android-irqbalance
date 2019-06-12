#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "irqbalance.h"

#define IRQBALANCE_CONFIG_FILE "/vendor/etc/irqbalance.conf"

int parse_cpu_prios(char *prio_str, struct irqbalance_config *conf) {
    int i = 1;
    int ret = -1;
    char *token;
    char *endchar;
    token = strtok(prio_str, ",");
    conf->cpus_with_prio = malloc(sizeof(int));
    conf->cpus_with_prio[0] = (int)strtol(token, &endchar, 10);
    printf("%s: CPU%d: %d\n", __func__, 0, conf->cpus_with_prio[0]);
    while (token != NULL) {
        i++;
        token = strtok(NULL, ",");
        if (token != NULL) {
            // TODO: Maybe realloc more generously and then trim at the end
            void *_u = realloc(conf->cpus_with_prio, (i+1)*sizeof(int));
            conf->cpus_with_prio[i] = (int)strtol(token, &endchar, 10);
            conf->num_cpus_with_prio = i;
            ret = 0;
            if (*endchar != '\0') {
                break;
            }
        }
    }
    return ret;
}

int parse_ignored_irqs(char *irq_str, struct irqbalance_config *conf) {
    int i = 0;
    int ret = -1;
    char *token;
    char *endchar;
    token = strtok(irq_str, ",");
    conf->ignored_irqs = malloc(sizeof(int));
    conf->ignored_irqs[0] = (int)strtol(token, &endchar, 10);
    ALOGI("%s: IRQ %d banned\n", __func__, conf->ignored_irqs[i]);
    while (token != NULL) {
        i++;
        token = strtok(NULL, ",");
        if (token != NULL) {
            void *_u = realloc(conf->ignored_irqs, (i+1)*sizeof(int));
            conf->ignored_irqs[i] = (int)strtol(token, &endchar, 10);
            ALOGE("%s: IRQ %d banned\n", __func__, conf->ignored_irqs[i]);
            conf->num_ignored_irqs = i;
            ret = 0;
            if (*endchar != '\0') {
                break;
            }
        }
    }
    return ret;
}

int parse_thread_delay(char *irq_str, struct irqbalance_config *conf) {
    char *token;
    char *endchar;
    token = strtok(irq_str, ",");
    conf->THREAD_DELAY = (u64)strtol(token, &endchar, 10);
    // THREAD_DELAY is of type u64
    ALOGE("%s: delay=%llu\n", __func__, conf->THREAD_DELAY);
    ALOGE("%s: %llu\n", __func__, conf->THREAD_DELAY);
    return 0;
}

int read_irqbalance_conf(struct irqbalance_config *conf) {
    FILE *fp = fopen(IRQBALANCE_CONFIG_FILE, "r");
    int ret = -1;

    if (!fp) {
        return -ENOENT;
    }

    while (!feof(fp)) {
        char *key;
        char *value;
        static char _line[50];

        memset(_line, 0, sizeof _line);
        _line[0] = fgetc(fp);

        /* Ignore comment lines */
        if (_line[0] == '#') {
            fscanf(fp, "%*[^\n]\n", NULL);
            continue;
        }

        fscanf(fp, "%49s\n", _line + 1);
        key = strtok(_line, "=");
        value = strtok(NULL, "\n");

        /* TODO: Error handling, missing PRIO or IGNORED_IRQ is fatal */
        if (strcmp(key, "PRIO") == 0) {
            ret = parse_cpu_prios(value, conf);
            ALOGI("%s: Cores: %d\n", __func__, i, conf->num_cpus_with_prio);
        } else if (strcmp(key, "IGNORED_IRQ") == 0) {
            ret = parse_ignored_irqs(value, conf);
        } else if (strcmp(key, "THREAD_DELAY") == 0) {
            ret = parse_thread_delay(value, conf);
        }
    }

    fclose(fp);

    return 0;
}
