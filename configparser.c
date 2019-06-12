#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "irqbalance.h"

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
            // TODO: Maybe realloc more generously and then trim at
            // the end
            void *_u = realloc(conf->cpus_with_prio, (i+1)*sizeof(int));
            conf->cpus_with_prio[i] = (int)strtol(token, &endchar, 10);
            ALOGE("%s: CPU%d: %d\n", __func__, i, conf->cpus_with_prio[i]);
            conf->num_cpus_with_prio = i;
            ALOGE("%s: cpus=%d:\n", __func__, conf->num_cpus_with_prio);
            ret = 0;
            if (*endchar != '\0') {
                break;
            }
        }
    }
    return ret;
}

int parse_ignored_irqs(char *irq_str) {
    int i = 0;
    int ret = -1;
    char *token;
    char *endchar;
    token = strtok(irq_str, ",");
    ignored_irqs = malloc(sizeof(int));
    ignored_irqs[0] = (int)strtol(token, &endchar, 10);
    /* printf("IRQ %d banned\n", ignored_irqs[i]); */
    ALOGI("%s: IRQ %d banned\n", __func__, ignored_irqs[i]);
    while (token != NULL) {
        i++;
        token = strtok(NULL, ",");
        if (token != NULL) {
            void *_u = realloc(ignored_irqs, (i+1)*sizeof(int));
            // no Wunused
            //_u = NULL;
            //realloc(ignored_irqs, (i+1)*sizeof(int));
            /* printf("IRQ %d banned\n", ignored_irqs[i]); */
            ignored_irqs[i] = (int)strtol(token, &endchar, 10);
            ALOGE("%s: IRQ %d banned\n", __func__, ignored_irqs[i]);
            num_ignored_irqs = i;
            ret = 0;
            if (*endchar != '\0') {
                break;
            }
        }
    }
    return ret;
}

int parse_thread_delay(char *irq_str) {
    char *token;
    char *endchar;
    token = strtok(irq_str, ",");
    THREAD_DELAY = (u64)strtol(token, &endchar, 10);
    /* printf("thread delay=%d\n", THREAD_DELAY); */
    // THREAD_DELAY is of type u64
    ALOGE("%s: delay=%llu\n", __func__, THREAD_DELAY);
    ALOGE("%s: %llu\n", __func__, THREAD_DELAY);
    return 0;
}

int read_irqbalance_conf(struct irqbalance_config *conf) {
    FILE *fp = fopen("/vendor/etc/irqbalance.conf", "r");
    int ret = -1;

    if (!fp) {
        return -ENOENT;
    }

    while (!feof(fp)) {
        static char _line[50];
        memset(_line, 0, sizeof _line);

        _line[0] = fgetc(fp);
        /* Ignore comment lines */
        if (_line[0] == '#') {
            fscanf(fp, "%*[^\n]\n", NULL);
            continue;
        }
        fscanf(fp, "%49s\n", _line + 1);
        char *key;
        char *value;
        key = strtok(_line, "=");
        value = strtok(NULL, "\n");
        if (strcmp(key, "PRIO") == 0) {
            ALOGI("%s: parse_cpu_prios\n", __func__);
            ret = parse_cpu_prios(value, conf);
            if (ret != 0) {
                ALOGE("%s: parse_cpu_prios != 0, return!", __func__);
            }
            ALOGE("%s: cpus=%d:\n", __func__, conf->num_cpus_with_prio);
        } else if (strcmp(key, "IGNORED_IRQ") == 0) {
            ALOGI("%s: parse_ignored_irqs\n", __func__);
            ret =parse_ignored_irqs(value);
            if (ret != 0) {
                ALOGE("%s: parse_ignored_irqs != 0, return!", __func__);
            }
        } else if (strcmp(key, "THREAD_DELAY") == 0) {
            ALOGI("%s: parse_thread_delay\n", __func__);
            ret = parse_thread_delay(value);
            if (ret != 0) {
                ALOGE("%s: parse_thread_delay != 0, return!", __func__);
            }
        }
    }

    fclose(fp);

    return 0;
}
