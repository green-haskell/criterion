#include <unistd.h>
#include "rapl.h"

double criterion_getenergypacket(void)
{

    char core = 0;  // NOTE: Just core 0
    int fd_msr = 0;

    struct rapl_raw_power_counters rpc;

    fd_msr = rapl_open_msr( core );

    struct rapl_units r_units;
    if ( !rapl_get_units( fd_msr , &r_units ) ) {
        return -1;
    }
    //rapl_print_units( &r_units );

    rapl_get_raw_power_counters( fd_msr , &r_units , &rpc );
    //rapl_print_raw_power_counters ( fd_msr, &r_units );

    close( fd_msr );

    return rapl_get_energy( &rpc );
}
