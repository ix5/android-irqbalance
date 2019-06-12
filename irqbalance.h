#ifndef _IRQBALANCE_H
#define _IRQBALANCE_H

#include <log/log.h>

#include <stdbool.h>

typedef unsigned char u8;
typedef unsigned long long u64;

typedef struct cpudata {
    // general
    u8 core;
    bool online;

    // cputimes read from /proc/stat
    u64 cputime_user;
    u64 cputime_nice;
    u64 cputime_system;
    u64 cputime_idle;
    u64 cputime_iowait;
    u64 cputime_irq;
    u64 cputime_softirq;
    u64 cputime_steal;
    u64 cputime_guest;
    u64 cputime_guest_nice;

    // utilization
    u64 cpu_util;

} cpudata_t;

struct irqbalance_config {
    int *cpus_with_prio;
    int num_cpus_with_prio;
    int *ignored_irqs;
    int num_ignored_irqs;
    u64 THREAD_DELAY;
};

static struct irqbalance_config irqb_conf;
static cpudata_t *__cpudata;

// Functions
int read_irqbalance_conf(struct irqbalance_config *conf);

#endif  // _IRQBALANCE_H
