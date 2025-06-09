/*
- * Real Time Tester
- *
- * Based on:
- * https://wiki.linuxfoundation.org/realtime/documentation/howto/applications/application_base
- * https://wiki.linuxfoundation.org/realtime/documentation/howto/applications/cyclic
- *
- * This was built using VS on Windows and WSL as the Target System
- * 
- * Use vcpkg to install pthreads
- * https://github.com/microsoft/vcpkg
- *
- * Add CMAKE_TOOLCHAIN_FILE variable to CMakePresets.json
- * "CMAKE_TOOLCHAIN_FILE": "[path to vcpkg]/scripts/buildsystems/vcpkg.cmake"
- *
- */
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>
#include <float.h>
#include <signal.h>

#include "rt-tester.h"

#if defined(_WIN32)
#define OS "Windows"
#elif defined(__linux__)
#define OS "Linux"
#elif defined(__APPLE__)
#define OS "MacOS"
#else
#define OS "Unknown OS"
#endif


volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t print_stats = 0;


/**
 * @brief The signal handler function to handle termination signals.
 * 
 * @param sig The signal number.
 */
void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
    {
        keep_running = 0;
    }
    else if (sig == SIGUSR1)
    {
        print_stats = 1;
    }
}


/**
 * @brief This function prints the platform information such as system name, node name, release, version, machine type, and number of online processors.
 * 
 */
void print_platform_info()
{
    struct utsname uts;
    if (uname(&uts) == -1)
    {
        perror("uname failed");
        return;
    }

    printf("\nPlatform Information:\n");
    printf("  System: %s\n", uts.sysname);
    printf("  Node: %s\n", uts.nodename);
    printf("  Release: %s\n", uts.release);
    printf("  Version: %s\n", uts.version);
    printf("  Machine: %s\n", uts.machine);
    
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs == -1)
    {
        perror("sysconf failed");
    }
    else
    {
        printf("  Online processors: %ld\n", nprocs);
    }
}


/**
 * @brief Prints the help message for the program.
 * 
 * @param progname The name of the program.
 */
void print_help(const char* progname)
{
    printf("Usage: %s [options]\n", progname);
    printf("Options:\n");
    printf("  -p, --period MS       Period in milliseconds (default: 1.0)\n");
    printf("  -r, --rate HZ         Console refresh rate in Hz (default: 5)\n");
    printf("  -d, --duration SEC    Run for specified seconds then exit\n");
    printf("  -s, --stress N        Number of CPU stress threads (default: 0)\n");
    printf("  --policy POL          Scheduling policy (other|rr|fifo, default: fifo)\n");
    printf("  --priority PRI        Thread priority (1-99 for RT policies, default: 80)\n");
    printf("  -l, --log FILE        Log output to file\n");
    printf("  --no-color            Disable colored output\n");
    printf("  -i, --info            Show platform information\n");
    printf("  -h, --help            Show this help message\n");
}

/**
 * @brief Prints the logo and configuration information.
 * 
 * @param tid Thread ID.
 * @param policy Scheduling policy.
 * @param priority Thread priority.
 * @param period_ms Period in milliseconds.
 * @param print_per_sec Console refresh rate in Hz.
 * @param stress_threads Number of CPU stress threads.
 * @param duration_sec Duration in seconds to run the task.
 * @param log_file Log file name, if any.
 * @param use_color Whether to use colored output.
 */
void print_logo(long tid, int policy, int priority, long double period_ms, long print_per_sec, 
                int stress_threads, int duration_sec, const char* log_file, bool use_color)
{
    const char* color_start = use_color ? CYAN : "";
    const char* color_end = use_color ? RESET : "";
    
    printf("%s", color_start);
    printf(R"(
  ____ _____   _            _            
 |  _ \_   _| | |_ ___  ___| |_ ___ _ __ 
 | |_) || |   | __/ _ \/ __| __/ _ \ '__|
 |  _ < | |   | ||  __/\__ \ ||  __/ |   
 |_| \_\|_|    \__\___||___/\__\___|_|    ver: %s                                       
)",
    VERSION);
    printf("%s", color_end);

    printf("\nConfiguration:\n");
    printf("  Thread ID: %ld\n", tid);
    printf("  Scheduler policy: %s%s%s\n", color_start, 
        policy == SCHED_OTHER ? "SCHED_OTHER" :
        (policy == SCHED_RR ? "SCHED_RR" :
        (policy == SCHED_FIFO ? "SCHED_FIFO" : "Unknown")), color_end);
    printf("  Priority: %s%d%s\n", color_start, priority, color_end);
    printf("  Period: %s%.4Lf ms%s\n", color_start, period_ms, color_end);
    printf("  Console refresh rate: %s%ld Hz%s\n", color_start, print_per_sec, color_end);
    printf("  CPU stress threads: %s%d%s\n", color_start, stress_threads, color_end);
    if (duration_sec > 0)
    {
        printf("  Duration: %s%d seconds%s\n", color_start, duration_sec, color_end);
    }
    if (log_file)
    {
        printf("  Log file: %s%s%s\n", color_start, log_file, color_end);
    }
    printf("\n");
}

/**
 * @brief A simple CPU stressor function that runs an infinite loop to consume CPU cycles.
 * 
 * @param arg Unused argument.
 * @return NULL
 */
void* cpu_stressor(void* arg)
{
    (void)arg;
    while (1)
    {
        for (int i = 0; i < 1000000; i++);
    }
    return NULL;
}

/**
 * @brief Spawns a specified number of CPU stress threads.
 * 
 * @param count Number of stress threads to spawn.
 */
void spawn_stress_threads(int count)
{
    pthread_t t;
    pthread_attr_t attr;
    
    for (int i = 0; i < count; i++)
    {
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&t, &attr, cpu_stressor, NULL) != 0)
        {
            perror("Failed to create stress thread");
        }
    }
}

/**
 * @brief Calculates the difference in nanoseconds between two timespec structures.
 * 
 * @param time1 The later time.
 * @param time0 The earlier time.
 * @return The difference in nanoseconds as a long double.
 */
long double diff_nanosec(const struct timespec* time1, const struct timespec* time0)
{
    return 1000000000 * (time1->tv_sec - time0->tv_sec) + (time1->tv_nsec - time0->tv_nsec);
}

/**
 * @brief Increments the next period by the specified period in nanoseconds.
 * 
 * @param pinfo Pointer to the period_info structure containing the next period and period in nanoseconds.
 */
static void inc_period(struct period_info* pinfo)
{
    pinfo->next_period.tv_nsec += pinfo->period_ns;
    while (pinfo->next_period.tv_nsec >= 1000000000)
    {
        pinfo->next_period.tv_sec++;
        pinfo->next_period.tv_nsec -= 1000000000;
    }
}

/**
 * @brief Initializes the period_info structure with the specified period in nanoseconds and print rate.
 * 
 * @param pinfo Pointer to the period_info structure to initialize.
 * @param period_ns Period in nanoseconds.
 * @param print_per_sec Print rate in Hz.
 */
static void periodic_task_init(struct period_info* pinfo, long period_ns, long print_per_sec)
{
    pinfo->period_ns = period_ns;
    pinfo->print_rate = print_per_sec == 0 ? 0 : 1000000000 / (period_ns * print_per_sec);
    clock_gettime(CLOCK_MONOTONIC, &(pinfo->next_period));
    
    // Initialize stats
    pinfo->total_cycles = 0;
    pinfo->total_delay = 0;
    pinfo->max_delay = 0;
    pinfo->min_delay = DBL_MAX;
    pinfo->missed_deadlines = 0;
}

/**
 * @brief Waits for the next period to start, blocking until the next period's time.
 * 
 * @param pinfo Pointer to the period_info structure containing the next period.
 */
static void wait_rest_of_period(struct period_info* pinfo)
{
    inc_period(pinfo);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &pinfo->next_period, NULL);
}

/**
 * @brief Prints the statistics of the periodic task, including total cycles, missed deadlines, max/min/avg delays, and miss rate.
 * 
 * @param pinfo Pointer to the period_info structure containing the statistics.
 * @param use_color Whether to use colored output.
 */
static void print_statistics(const struct period_info* pinfo, bool use_color)
{
    const char* color_start = use_color ? YELLOW : "";
    const char* color_end = use_color ? RESET : "";
    
    long double avg_delay = pinfo->total_cycles > 0 ? pinfo->total_delay / pinfo->total_cycles : 0;
    
    printf("\n%sStatistics:%s\n", color_start, color_end);
    printf("  Total cycles: %s%lu%s\n", color_start, pinfo->total_cycles, color_end);
    printf("  Missed deadlines: %s%lu%s\n", color_start, pinfo->missed_deadlines, color_end);
    printf("  Max delay: %s%.4Lf ms%s\n", color_start, pinfo->max_delay / 1000000.0, color_end);
    printf("  Min delay: %s%.4Lf ms%s\n", color_start, pinfo->min_delay / 1000000.0, color_end);
    printf("  Avg delay: %s%.4Lf ms%s\n", color_start, avg_delay / 1000000.0, color_end);
    printf("  Miss rate: %s%.2f%%%s\n", color_start, 
           (pinfo->total_cycles > 0 ? (100.0 * pinfo->missed_deadlines / pinfo->total_cycles) : 0), 
           color_end);
}

/**
 * @brief Executes the real-time task, measuring the delay and task execution time, updating statistics, and printing information to console and log file.
 * 
 * @param pinfo Pointer to the period_info structure containing the next period and statistics.
 * @param log_file Pointer to the log file for writing output, if any.
 * @param use_color Whether to use colored output.
 */
static void do_rt_task(struct period_info* pinfo, FILE* log_file, bool use_color)
{
    long double delay_ns, task_ns;
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    delay_ns = diff_nanosec(&start, &(pinfo->next_period));

    //upsate statistics
    pinfo->total_cycles++;
    pinfo->total_delay += delay_ns;
    if (delay_ns > pinfo->max_delay) pinfo->max_delay = delay_ns;
    if (delay_ns < pinfo->min_delay) pinfo->min_delay = delay_ns;
    if (delay_ns > pinfo->period_ns) pinfo->missed_deadlines++;

    time_t rawtime;
    time(&rawtime);
    struct tm* timeinfo = localtime(&rawtime);

    bool period_exceeded = delay_ns > pinfo->period_ns;
    bool print_info = pinfo->print_rate != 0 && ((pinfo->next_period.tv_nsec / pinfo->period_ns) % pinfo->print_rate) == 0;

    clock_gettime(CLOCK_MONOTONIC, &end);
    task_ns = diff_nanosec(&end, &start);

    const char* color_red = use_color ? RED : "";
    const char* color_reset = use_color ? RESET : "";
    
    if (period_exceeded)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "[%02d:%02d:%02d] delay: %.4Lfms task: %.4Lfms\n", 
                 timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, 
                 delay_ns / 1000000.0, task_ns / 1000000.0);
        
        printf("\33[2K\r%s%s%s", color_red, buf, color_reset);
        if (log_file)
        {
            fprintf(log_file, "MISS: %s", buf);
            fflush(log_file);
        }
    } 
    else if (print_info)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "[%02d:%02d:%02d] delay: %.4Lfms task: %.4Lfms", 
                 timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, 
                 delay_ns / 1000000.0, task_ns / 1000000.0);
        
        printf("\33[2K\r%s", buf);
        fflush(stdout);
        
        if (log_file)
        {
            fprintf(log_file, "%s\n", buf);
            fflush(log_file);
        }
    }
    
    if (print_stats)
    {
        print_statistics(pinfo, use_color);
        print_stats = 0;
    }
}

/**
 * @brief The main function that initializes the periodic task, handles command line arguments, sets up signal handlers, and starts the cyclic task.
 * 
 * @param data Pointer to the task_data structure containing task parameters.
 * @return NULL
 */
void* simple_cyclic_task(void* data)
{
    struct task_data* tdata = (struct task_data*)data;
    
    struct sched_param sp;
    long tid = syscall(SYS_gettid);
    int policy = sched_getscheduler(tid);
    int priority = sched_getparam(0, &sp) ? 0 : sp.sched_priority;
    
    print_logo(tid, policy, priority, (long double)tdata->period_ns / 1000000, 
              tdata->print_per_sec, tdata->stress_threads, tdata->duration_sec, 
              tdata->log_file, tdata->use_color);

    struct period_info pinfo;
    periodic_task_init(&pinfo, tdata->period_ns, tdata->print_per_sec);

    time_t start_time;
    time(&start_time);
    
    FILE* log = tdata->log_file ? fopen(tdata->log_file, "a") : NULL;
    if (tdata->log_file && !log)
    {
        perror("Failed to open log file");
    }

    if (log)
    {
        fprintf(log, "\n--- New Test Run ---\n");
        fprintf(log, "Start time: %s", ctime(&start_time));
        fprintf(log, "Period: %.4f ms\n", (double)tdata->period_ns / 1000000.0);
        fprintf(log, "Policy: %s\n", 
               policy == SCHED_OTHER ? "SCHED_OTHER" :
               (policy == SCHED_RR ? "SCHED_RR" :
               (policy == SCHED_FIFO ? "SCHED_FIFO" : "Unknown")));
        fprintf(log, "Priority: %d\n", priority);
        fprintf(log, "CPU stress threads: %d\n", tdata->stress_threads);
        fflush(log);
    }

    while (keep_running)
    {
        do_rt_task(&pinfo, log, tdata->use_color);
        wait_rest_of_period(&pinfo);
        
        //check limit duratin
        if (tdata->duration_sec > 0)
        {
            time_t current_time;
            time(&current_time);
            if (difftime(current_time, start_time) >= tdata->duration_sec)
            {
                keep_running = 0;
            }
        }
    }

    print_statistics(&pinfo, tdata->use_color);
    
    if (log)
    {
        fprintf(log, "\n--- Test Results ---\n");
        fprintf(log, "Total cycles: %lu\n", pinfo.total_cycles);
        fprintf(log, "Missed deadlines: %lu\n", pinfo.missed_deadlines);
        fprintf(log, "Max delay: %.4Lf ms\n", pinfo.max_delay / 1000000.0);
        fprintf(log, "Min delay: %.4Lf ms\n", pinfo.min_delay / 1000000.0);
        fprintf(log, "Avg delay: %.4Lf ms\n", 
               pinfo.total_cycles > 0 ? pinfo.total_delay / pinfo.total_cycles / 1000000.0 : 0);
        fprintf(log, "Miss rate: %.2f%%\n", 
               pinfo.total_cycles > 0 ? (100.0 * pinfo.missed_deadlines / pinfo.total_cycles) : 0);
        
        time_t end_time;
        time(&end_time);
        fprintf(log, "End time: %s", ctime(&end_time));
        fclose(log);
    }

    return NULL;
}

/**
 * @brief The main entry point of the program.
 * 
 * Parses command line arguments, sets up signal handlers, initializes memory locking, spawns stress threads if requested,
 * and starts the cyclic task with specified parameters.
 * 
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char* argv[])
{
    struct task_data data = {
        .period_ns = 1000000,    // 1ms default
        .print_per_sec = 5,       // 5Hz default
        .stress_threads = 0,      // No stress by default
        .duration_sec = 0,        // Run indefinitely
        .policy = SCHED_FIFO,     // FIFO by default
        .priority = 80,           // Default priority
        .log_file = NULL,         // No log by default
        .use_color = true         // Use color by default
    };

    static struct option long_options[] = {
        {"period", required_argument, 0, 'p'},
        {"rate", required_argument, 0, 'r'},
        {"duration", required_argument, 0, 'd'},
        {"stress", required_argument, 0, 's'},
        {"policy", required_argument, 0, 0},
        {"priority", required_argument, 0, 0},
        {"log", required_argument, 0, 'l'},
        {"no-color", no_argument, 0, 0},
        {"info", no_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "p:r:d:s:l:ih", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
            case 0:
                if (strcmp(long_options[option_index].name, "policy") == 0)
                {
                    if (strcmp(optarg, "other") == 0)
                    {
                        data.policy = SCHED_OTHER;
                    }
                    else if (strcmp(optarg, "rr") == 0)
                    {
                        data.policy = SCHED_RR;
                    }
                    else if (strcmp(optarg, "fifo") == 0)
                    {
                        data.policy = SCHED_FIFO;
                    }
                    else
                    {
                        fprintf(stderr, "Invalid policy: %s\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                } 
                else if (strcmp(long_options[option_index].name, "priority") == 0)
                {
                    data.priority = atoi(optarg);
                    if (data.priority < 1 || data.priority > 99)
                    {
                        fprintf(stderr, "Priority must be between 1 and 99\n");
                        exit(EXIT_FAILURE);
                    }
                }
                else if (strcmp(long_options[option_index].name, "no-color") == 0)
                {
                    data.use_color = false;
                }
                break;
            case 'p':
                data.period_ns = atof(optarg) * 1000000;
                break;
            case 'r':
                data.print_per_sec = atol(optarg);
                break;
            case 'd':
                data.duration_sec = atoi(optarg);
                break;
            case 's':
                data.stress_threads = atoi(optarg);
                break;
            case 'l':
                data.log_file = optarg;
                break;
            case 'i':
                print_platform_info();
                exit(EXIT_SUCCESS);
            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    //signal handlers
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }

    /* Lock memory */
    if (mlockall(MCL_CURRENT | MCL_FUTURE))
    {
        perror("mlockall failed");
        exit(EXIT_FAILURE);
    }

    // Spawn stress threads if requested
    if (data.stress_threads > 0)
    {
        spawn_stress_threads(data.stress_threads);
    }

    pthread_attr_t attr;
    pthread_t thread;
    int ret;

    /* Initialize pthread attributes */
    ret = pthread_attr_init(&attr);
    if (ret)
    {
        fprintf(stderr, "init pthread attributes failed: %s\n", strerror(ret));
        goto out;
    }

    /* Set a specific stack size */
    ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
    if (ret)
    {
        fprintf(stderr, "pthread setstacksize failed: %s\n", strerror(ret));
        goto out;
    }

    /* Set scheduler policy and priority of pthread */
    ret = pthread_attr_setschedpolicy(&attr, data.policy);
    if (ret)
    {
        fprintf(stderr, "pthread setschedpolicy failed: %s\n", strerror(ret));
        goto out;
    }

    struct sched_param param;
    param.sched_priority = data.priority;
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret)
    {
        fprintf(stderr, "pthread setschedparam failed: %s\n", strerror(ret));
        goto out;
    }

    /* Use scheduling parameters of attr */
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret)
    {
        fprintf(stderr, "pthread setinheritsched failed: %s\n", strerror(ret));
        goto out;
    }

    /* Create a pthread with specified attributes */
    ret = pthread_create(&thread, &attr, simple_cyclic_task, &data);
    if (ret)
    {
        fprintf(stderr, "create pthread failed: %s\n", strerror(ret));
        goto out;
    }

    /* Join the thread and wait until it is done */
    ret = pthread_join(thread, NULL);
    if (ret)
    {
        fprintf(stderr, "join pthread failed: %s\n", strerror(ret));
    }

out:
    return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}

// EXAMPLE EXECX: ./rt-tester --period 0.5 --rate 10 --stress 2 --duration 60 --policy fifo --priority 90 --log test.log
