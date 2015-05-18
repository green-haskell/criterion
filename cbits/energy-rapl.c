#include <unistd.h>
#include <pthread.h>
#include "rapl.h"

static int fd_msr = 0;
static struct rapl_units r_units;
static int units_ok;

static double energy_sum = 0;
pthread_t thread;

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

        if (diff.pkg < 0)
            continue;

        prev = next;
        energy_sum += diff.pkg;
    }
}

void criterion_initrapl(void)
{
    char core = 0;  // NOTE: Just core 0
    fd_msr = rapl_open_msr(core);
    units_ok = rapl_get_units(fd_msr, &r_units);

    pthread_create(&thread, NULL, thread_sum_energy_packet, NULL);
}

double criterion_getenergypacket(void)
{
    if (!units_ok || fd_msr == 0)
        return -1;

    return energy_sum;
}

void criterion_finishrapl(void)
{
    close(fd_msr);
}
