#include <stdio.h>
#include <unistd.h>
#include "rapl.h"

#define MAX_PACKAGES 32

static int fd_msr[MAX_PACKAGES];
static struct rapl_units r_units;
static int units_ok;
static int num_packages;

static int get_kernel_nr_cpus(void)
{
    int nr_cpus = 1;
    FILE *fff = fopen("/sys/devices/system/cpu/kernel_max","r");
    if (fff == NULL)
        return nr_cpus;

    int num_read = fscanf(fff, "%d", &nr_cpus);
    fclose(fff);
    return (num_read == 1) ? (nr_cpus + 1) : 1;
}

void criterion_initrapl(void)
{
    int nr_cpus = get_kernel_nr_cpus();
    int cpu_to_use[nr_cpus];

    int package;
    char filename[256];
    num_packages = 0;

    // Fill with placeholders
    int i;
    for (i = 0; i < nr_cpus; i++)
        cpu_to_use[i] = -1;

    int j = 0;
    // Detect the amount of packages
    while (1) {
       sprintf(filename, "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", j);
       FILE *fff = fopen(filename, "r");
       if (fff == NULL)
           break;

       int num_read = fscanf(fff, "%d", &package);
       fclose(fff);
       if (num_read != 1)
           break;

       // Check if a new package
       if ((package >= 0) && (package < nr_cpus)) {
           if (cpu_to_use[package] == -1) {
               cpu_to_use[package] = j;
               num_packages++;
           }
       } else {
           perror("Package outside of allowed range\n");
           break;
       }

       j++;
    }

    printf("Total packages found: %d\n", num_packages);

    for (i = 0; i < num_packages; i++) {
        fd_msr[i] = rapl_open_msr(cpu_to_use[i]);
        if (fd_msr[i] <= 0) {
            char msg[256];
            sprintf(msg, "Could not open msr %d\n", cpu_to_use[i]);
            perror(msg);
        }
    }

    units_ok = rapl_get_units(fd_msr[0], &r_units);
}

double criterion_getenergypacket(void)
{
    if (!units_ok) {
        perror("Invalid units!");
        return -1;
    }

    struct rapl_raw_power_counters rpc;
    double total_package_energy = 0.0;

    int i;
    for (i = 0; i < num_packages; i++) {
        rapl_get_raw_power_counters(fd_msr[i], &r_units, &rpc);
        total_package_energy += rpc.pkg;
    }

    return total_package_energy;
}

void criterion_finishrapl(void)
{
    int i;
    for (i = 0; i < num_packages; i++)
        close(fd_msr[i]);
}
