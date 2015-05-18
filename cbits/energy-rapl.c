#include <unistd.h>
#include <pthread.h>
#include "rapl.h"

static int fd_msr = 0;
static struct rapl_units r_units;
static int units_ok;

static double energy_sum = 0;
pthread_t thread;
pthread_mutex_t mutex;

void *thread_sum_energy_packet(void *arg)
{
    struct rapl_power_diff diff;
    struct rapl_raw_power_counters prev, next;
    rapl_get_raw_power_counters(fd_msr, &r_units, &prev);

    for (;;)
    {
        sleep(15);

        rapl_get_raw_power_counters(fd_msr, &r_units, &next);
        rapl_get_power_diff(&prev, &next, &diff);
        prev = next;

        if (diff.pkg < 0)
            continue;

        pthread_mutex_lock(&mutex);
        energy_sum += diff.pkg;
        pthread_mutex_unlock(&mutex);
    }

    pthread_exit((void *) 0);
}

void criterion_initrapl(void)
{
    char core = 0;  // NOTE: Just core 0
    fd_msr = rapl_open_msr(core);
    units_ok = rapl_get_units(fd_msr, &r_units);

    pthread_mutex_init(&mutex, NULL);
    pthread_create(&thread, NULL, thread_sum_energy_packet, NULL);
}

double criterion_getenergypacket(void)
{
    if (!units_ok || fd_msr == 0)
        return -1;

    double ret;
    pthread_mutex_lock(&mutex);
    ret = energy_sum;
    pthread_mutex_unlock(&mutex);
    return ret;
}

void criterion_finishrapl(void)
{
    close(fd_msr);
    pthread_mutex_destroy(&mutex);
}
