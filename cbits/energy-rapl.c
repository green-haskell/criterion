#include <unistd.h>
#include "rapl.h"

static int fd_msr = 0;
static struct rapl_units r_units;
static int units_ok;

void criterion_initrapl(void)
{
    char core = 0;  // NOTE: Just core 0
    fd_msr = rapl_open_msr(core);
    units_ok = rapl_get_units(fd_msr, &r_units);
}

double criterion_getenergypacket(void)
{
    if (!units_ok || fd_msr == 0) {
        return -1;
    }

    struct rapl_raw_power_counters rpc;

    rapl_get_raw_power_counters(fd_msr, &r_units, &rpc);

    return rpc.pkg;
}

void criterion_finishrapl(void)
{
    close(fd_msr);
}
