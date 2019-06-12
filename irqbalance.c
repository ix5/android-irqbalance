#define LOG_TAG "irqbalance"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//#include <log/log.h>

#include "irqbalance.h"

//#if defined(LOG_VDEBUG) && LOG_VDEBUG
//#define ALOGVV ALOGV
//#else
//#define ALOGVV(...) \
//    {}
//#endif

/* static struct irqbalance_config *conf; */
static int *cpus_with_prio;
static int num_cpus_with_prio;

static int read_cpudata(cpudata_t *cpudata, int core) {
    ALOGE("%s: Handling core %d", __func__, core);
    FILE *fp = fopen("/proc/stat", "r");
    int coredummy;
    int i, ret;

    if (!fp) {
        ALOGE("%s: Couldn't open %s", __func__, "/proc/stat");
        return -ENOENT;
    }

    /* Skip other CPUs/non-IRQ CPUs/CPU-total */
    /* for (i = 0; i < (core + OFFS_CPUS + 1); i++) { */
    /* (Assume every CPU is a IRQ CPU) */
    for (i = 0; i < (core + 1); i++) {
        fscanf(fp, "%*[^\n]\n", NULL);
    }

    ret = fscanf(fp, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu\n", &coredummy,
                 &cpudata->cputime_user,
                 &cpudata->cputime_nice,
                 &cpudata->cputime_system,
                 &cpudata->cputime_idle,
                 &cpudata->cputime_iowait,
                 &cpudata->cputime_irq,
                 &cpudata->cputime_softirq,
                 &cpudata->cputime_steal,
                 &cpudata->cputime_guest,
                 &cpudata->cputime_guest_nice);

    // 11 = EAGAIN
    if (ret != 11) {
        if (cpudata->online) {
            ALOGW("%s: --- marking cpu%d as offline\n", __func__, core);
            cpudata->online = false;
        }
        return -EINVAL;
    }

    if (!cpudata->online)
        ALOGI("%s: +++ marking cpu%d as online\n", __func__, core);

    cpudata->online = true;

    fclose(fp);
    return 0;
}

static void irqbalance_load_watchdog(void) {
    int i, ret;

    /* for (i = 0; i < NUM_CPU_CORES; i++) { */
    for (i = 0; i < num_cpus_with_prio; i++) {
        int core = __cpudata[i].core;

        // store current cpudata for calcucation of cpu
        // utilization
        cpudata_t old_cpudata = __cpudata[i];

        // read the current cputimes
        ret = read_cpudata(&__cpudata[i], i);
        if (ret) {
            if (__cpudata[i].online)
                ALOGE("%s: failed to read cpudata for core %d: %s (%d)", __func__, core, strerror(-ret), -ret);
            break;
        }

#define TOTAL_CPUTIME(_data)                                               \
    (                                                                      \
        _data.cputime_user + _data.cputime_nice + _data.cputime_system +   \
        _data.cputime_iowait + _data.cputime_irq + _data.cputime_softirq + \
        _data.cputime_steal + _data.cputime_guest + _data.cputime_guest_nice)

        u64 old_total_cputime = TOTAL_CPUTIME(old_cpudata);
        u64 cur_total_cputime = TOTAL_CPUTIME(__cpudata[i]);

        u64 cputime_delta = cur_total_cputime - old_total_cputime;
        u64 cputime_total_delta =
            (cur_total_cputime + __cpudata[i].cputime_idle) -
            (old_total_cputime + old_cpudata.cputime_idle);

        (&__cpudata[i])->cpu_util = (cputime_delta * 100) / cputime_total_delta;
        ALOGV("%s: +++ core%d: util=%llu (%llu/%llu)\n", __func__, core, __cpudata[i].cpu_util, cputime_delta, cputime_total_delta);
    }
}

static void irqbalance_main() {
    int i, j, n, swp;
    int total_util;
    int irqs_processed;
    int online_cpus;
    char irqpath[255];
    /* cpudata_t c_cpudata[NUM_CPU_CORES]; */
    cpudata_t c_cpudata[num_cpus_with_prio];

    /* memcpy(c_cpudata, __cpudata, sizeof(cpudata_t) * NUM_CPU_CORES); */
    memcpy(c_cpudata, __cpudata, sizeof(cpudata_t) * num_cpus_with_prio);

    /* n = NUM_CPU_CORES; */
    n = num_cpus_with_prio;
    do {
        swp = 0;
        for (i = 0; i < (n - 1); ++i) {
            if (c_cpudata[i].cpu_util > c_cpudata[i + 1].cpu_util) {
                cpudata_t tmp = c_cpudata[i];
                c_cpudata[i] = c_cpudata[i + 1];
                c_cpudata[i + 1] = tmp;
                swp = 1;
            }
        }
        n = n - 1;
    } while (swp);

    irqs_processed = 0;
    total_util = 0;
    online_cpus = 0;

    /* for (i = 0; i < NUM_CPU_CORES; i++) { */
    for (i = 0; i < num_cpus_with_prio; i++) {
        total_util += (int)c_cpudata[i].cpu_util;
        if (c_cpudata[i].online)
            online_cpus++;
    }

    /* for (i = NUM_CPU_CORES - 1; i >= 0 && irqs_processed < irqs_num; i--) { */
    for (i = num_cpus_with_prio - 1; i >= 0 && irqs_processed < irqs_num; i--) {
        int online_index = (online_cpus - 1);
        int core = c_cpudata[i].core;
        int util = (int)c_cpudata[i].cpu_util;
        double ratio = util / 100.0;

        if (!c_cpudata[i].online)
            continue;

        int irqs_left = (irqs_num - irqs_processed);
        int irqnum = (int)ceil((irqs_left - (sqrt(ratio) * irqs_left)) / online_index);
        if (online_index == 0)
            irqnum = irqs_left;

        ALOGV("%s: +++ core%d(%02x): util=%d;ratio=%f;irqnum=%d;irqs_processed=%d\n",
              __func__, core, 1 << core, util, ratio, irqnum, irqs_processed);
        for (j = 0; j < irqnum && irqs_processed < irqs_num; j++) {
            int irqidx = irqs_processed + j;
            sprintf(irqpath, "/proc/irq/%d/smp_affinity", irqs[irqidx]);
            //ALOGVV("%s: +++ balancing IRQ %d (%s)\n", __func__, irqs[irqidx], irqpath);
            ALOGV("%s: +++ balancing IRQ %d (%s)\n", __func__, irqs[irqidx], irqpath);

            FILE *irqfp = fopen(irqpath, "w");
            if (irqfp) {
                fprintf(irqfp, "%02x\n", 1 << core);
                fflush(irqfp);
                fclose(irqfp);
            } else {
                ALOGV("%s: --- failed to open IRQ SMP affinity file for IRQ %d\n", __func__, irqs[irqidx]);
            }
        }

        irqs_processed += irqnum;

        if (online_cpus <= 0)
            break;

        if (c_cpudata[i].online)
            online_cpus--;
    }
}

static void irqbalance_loop() {
    struct timespec time_start;
    struct timespec time_end;
    u64 time_start_val;
    u64 time_end_val;
    u64 time_delta;
    u64 sleep_delta;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        time_start_val = (time_start.tv_sec * 1e9) + time_start.tv_nsec;

        irqbalance_load_watchdog();
        irqbalance_main();

        clock_gettime(CLOCK_MONOTONIC, &time_end);
        time_end_val = (time_end.tv_sec * 1e9) + time_end.tv_nsec;
        time_delta = (u64)((time_end_val - time_start_val) / 1e3);
        sleep_delta = THREAD_DELAY - time_delta;
        if (THREAD_DELAY < time_delta)
            sleep_delta = 0;

        ALOGV("%s: --- sleeping for %llums\n", __func__, sleep_delta / 1000);
        usleep(sleep_delta);
    }
}

static int scan_for_irqs(void) {
    FILE *fp = fopen("/proc/interrupts", "r");
    int i, j, ret, sz, dummy;

    if (!fp) {
        return -ENOENT;
    }

    // skip table header
    fscanf(fp, "%*[^\n]\n", NULL);

    // first, count the valid numeric IRQs
    irqs_num = 0;

    do {
        ret = fscanf(fp, "%d: ", &dummy);
        if (ret == 1) {
            irqs_num++;

            // skip line
            fscanf(fp, "%*[^\n]\n", NULL);
        }
    } while (ret == 1);

    // do not allocate memory for blocked IRQs
    /* irqs_num -= irq_blacklist_num; */
    irqs_num -= num_ignored_irqs;

    // reverse
    fseek(fp, 0, SEEK_SET);

    // skip table header again
    fscanf(fp, "%*[^\n]\n", NULL);

    // allocate memory
    sz = irqs_num * sizeof(int);
    irqs = malloc(sz);
    if (!irqs) {
        ALOGE("%s: could not allocate memory for IRQ list (%d bytes)\n", __func__, sz);
        return -ENOMEM;
    }

    // finally, read the IRQs
    for (i = 0; i < irqs_num;) {
        fscanf(fp, "%d: ", &irqs[i]);

        for (j = 0; j < irq_blacklist_num; j++) {
            /* if (irq_blacklist[j] == irqs[i]) { */
            if (ignored_irqs[j] == irqs[i]) {
                ALOGV("%s: --- Skipping IRQ %d: blacklisted\n", __func__, irqs[i]);
                goto skipirq;
            }
        }

        i++;

    skipirq:
        // skip line
        fscanf(fp, "%*[^\n]\n", NULL);
    }

    ALOGV("%s: +++ IRQS = %d\n", __func__, irqs_num);
    for (i = 0; i < irqs_num; i++) {
        ALOGV("%s: +++  %d\n", __func__, irqs[i]);
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    int i, ret;
    int dtsz;

    /* ALOGE("%s: *conf: %p\n", __func__, (void*)conf); */
    ALOGE("%s: *conf: %p\n", __func__, (void*)&irqb_conf);
    /* ALOGE("%s: conf->num_cpus_with_prio: %d\n", __func__, conf->num_cpus_with_prio); */
    ALOGE("%s: conf->num_cpus_with_prio: %d\n", __func__, irqb_conf.num_cpus_with_prio);
    /* conf->num_cpus_with_prio = -1; */
    /* conf->cpus_with_prio = NULL; */
    /* conf->THREAD_DELAY = 1000000; */

    ALOGE("%s: initializing irqbalance configuration\n", __func__);
    ret = read_irqbalance_conf(&irqb_conf);
    /* ret = read_irqbalance_conf(conf); */
    if (ret) {
        return ret;
    }
    /* cpus_with_prio = conf->cpus_with_prio; */
    /* num_cpus_with_prio = conf->num_cpus_with_prio; */
    cpus_with_prio = irqb_conf.cpus_with_prio;
    num_cpus_with_prio = irqb_conf.num_cpus_with_prio;

    ALOGE("%s: scanning for IRQs\n", __func__);
    ret = scan_for_irqs();
    if (ret) {
        return ret;
    }

    ALOGI("%s: allocating memory for CPU data\n", __func__);
    /* dtsz = sizeof(cpudata_t) * NUM_CPU_CORES; */
    dtsz = sizeof(cpudata_t) * num_cpus_with_prio;
    ALOGI("%s: cores=%d\n", __func__, num_cpus_with_prio);
    ALOGI("%s: allocating dtsz=%d\n", __func__, dtsz);

    __cpudata = malloc(dtsz);
    if (!__cpudata) {
        ALOGE("%s: could not allocate memory for CPU data (%d bytes)\n", __func__, dtsz);
        return -ENOMEM;
    }

    memset(__cpudata, 0, dtsz);

    ALOGI("%s: initializing CPU data\n", __func__);
    /* for (i = 0; i < NUM_CPU_CORES; i++) { */
    for (i = 0; i < num_cpus_with_prio; i++) {
        cpudata_t *dt = &__cpudata[i];

        // set core of cpudata - required for after-sort in IRQ balancing
        dt->core = i + OFFS_CPUS;

        ret = read_cpudata(dt, i);
        if (ret) {
            ALOGE("%s: fail to read initial cpudata for core %d", __func__, i);
            /* return ret; */
        }
    }

    ALOGI("%s: starting IRQ balancing loop\n", __func__);
    irqbalance_loop();

    // should never be reached
    ALOGE("%s: exited main loop, this should not happen! terminating\n", __func__);
    return 0;
}
