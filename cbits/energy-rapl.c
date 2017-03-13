#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "rapl.h"

#define MAX_PACKAGES  32
#define TICK_INTERVAL 10  // in ms

static int fd_msr[MAX_PACKAGES];
static struct rapl_units r_units;
static int units_ok;
static int num_packages;

static struct sigaction sa;
static volatile sig_atomic_t collecting;
static struct rapl_raw_power_counters r_before[MAX_PACKAGES];
static struct rapl_raw_power_counters r_after[MAX_PACKAGES];
static struct rapl_power_diff r_diff;

static volatile double total_package_energy;
    // <GM>
static volatile double total_dram_energy = 0.0;
    // </GM>

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

static void init_num_packages(int *cpu_to_use, int nr_cpus)
{
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

    //printf("Total packages found: %d\n", num_packages);
    //printf("CPUs used: ");
    //for (i = 0; i < num_packages; i++)
    //    printf("%d%s", cpu_to_use[i], i == (num_packages-1) ? "" : ", ");
    //printf("\n");
}

static void timer_handler(int signum)
{
    if (collecting == 0 || !units_ok) {
        return;
    }

    int i;
    for (i = 0; i < num_packages; i++) {
        rapl_get_raw_power_counters(fd_msr[i], &r_units, &r_after[i]);
        rapl_get_power_diff(&r_before[i], &r_after[i], &r_diff);
        r_before[i] = r_after[i];

            // <GM>
        /*
        if (r_diff.pkg < 0)
            continue;

        total_package_energy += r_diff.pkg;
        */
            // </GM>

            // <GM>
        if (r_diff.pkg >= 0) {
            total_package_energy += r_diff.pkg;

        }

        if (r_diff.dram >= 0) {
            total_dram_energy += r_diff.dram;

        }
            // </GM>

    }

}

static void init_timer(void)
{
    collecting = 0;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = &timer_handler;
    sigaction(SIGALRM, &sa, NULL);

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = TICK_INTERVAL * 1000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec =  TICK_INTERVAL * 1000;

    setitimer(ITIMER_REAL, &timer, NULL);
}

void criterion_initrapl(void)
{
    int nr_cpus = get_kernel_nr_cpus();
    int cpu_to_use[nr_cpus];
    init_num_packages(cpu_to_use, nr_cpus);

    int i;
    for (i = 0; i < num_packages; i++) {
        fd_msr[i] = rapl_open_msr(cpu_to_use[i]);
        if (fd_msr[i] <= 0) {
            char msg[256];
            sprintf(msg, "Could not open msr %d\n", cpu_to_use[i]);
            perror(msg);
        }
    }

    units_ok = rapl_get_units(fd_msr[0], &r_units);
    init_timer();
}

void criterion_startcollectingenergy(void)
{
    if (!units_ok) {
        perror("Invalid units!");
        return;
    }

    int i;
    for (i = 0; i < num_packages; i++) {
        rapl_get_raw_power_counters(fd_msr[i], &r_units, &r_before[i]);
    }

    total_package_energy = 0.0;

        // <GM>
    total_dram_energy = 0.0;
        // </GM>

    collecting = 1;
}

double criterion_getpackageenergy(void)
{
    if (!units_ok)
        return -1;

    collecting = 0;
    return total_package_energy;
}


    // <GM>
double criterion_getdramenergy(void)
{
    if (!units_ok)
        return -1;

    collecting = 0;
    return total_dram_energy;
}
    // </GM>


void criterion_finishrapl(void)
{
    int i;
    for (i = 0; i < num_packages; i++)
        close(fd_msr[i]);
}
