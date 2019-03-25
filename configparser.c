#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "irqbalance.h"

/* #include <cutils/log.h> */

void parse_cpu_prios(char *prio_str) {
    int i = 0;
    char *token;
    char *endchar;
    token = strtok(prio_str, ",");
    cpus_with_prio = malloc(sizeof(int));
    cpus_with_prio[0] = (int)strtol(token, &endchar, 10);
    printf("CPU%d: %d\n", 0, cpus_with_prio[0]);
    while (token != NULL) {
        i++;
        token = strtok(NULL, ",");
        if (token != NULL) {
            // TODO: Maybe realloc more generously and then trim at
            // the end
            void *_u = realloc(cpus_with_prio, (i+1)*sizeof(int));
            cpus_with_prio[i] = (int)strtol(token, &endchar, 10);
            printf("CPU%d: %d\n", i, cpus_with_prio[i]);
            num_cpus_with_prio = i;
            if (*endchar != '\0') {
                break;
            }
        }
    }
}

void parse_ignored_irqs(char *irq_str) {
    int i = 0;
    char *token;
    char *endchar;
    token = strtok(irq_str, ",");
    ignored_irqs = malloc(sizeof(int));
    ignored_irqs[0] = (int)strtol(token, &endchar, 10);
    printf("IRQ %d banned\n", ignored_irqs[i]);
    while (token != NULL) {
        i++;
        token = strtok(NULL, ",");
        if (token != NULL) {
            void *_u = realloc(ignored_irqs, (i+1)*sizeof(int));
            ignored_irqs[i] = (int)strtol(token, &endchar, 10);
            printf("IRQ %d banned\n", ignored_irqs[i]);
            num_ignored_irqs = i;
            if (*endchar != '\0') {
                break;
            }
        }
    }
}

void parse_thread_delay(char *irq_str) {
    char *token;
    char *endchar;
    token = strtok(irq_str, ",");
    THREAD_DELAY = (u64)strtol(token, &endchar, 10);
    printf("thread delay=%d\n", THREAD_DELAY);
}

int read_irq_conf() {
    /* FILE *fp = fopen("/vendor/etc/irqbalance.conf", "r"); */
    FILE *fp = fopen("irqbalance.conf", "r");

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
        /* printf("key=%s value=%s\n", key, value); */
        if (strcmp(key, "PRIO") == 0) {
            parse_cpu_prios(value);
        } else if (strcmp(key, "IGNORED_IRQ") == 0) {
            parse_ignored_irqs(value);
        } else if (strcmp(key, "THREAD_DELAY") == 0) {
            parse_thread_delay(value);
        }
    }

    fclose(fp);
    return 0;
}
