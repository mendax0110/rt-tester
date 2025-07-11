﻿#ifndef RT_TESTER_H
#define RT_TESTER_H

#if defined DLL_EXPORTS
#if defined WIN32
#define LIB_API(RetType) extern "C" __declspec(dllexport) RetType
#else
#define LIB_API(RetType) extern "C" RetType __attribute__((visibility("default")))
#endif
#else
#if defined WIN32
#define LIB_API(RetType) extern "C" __declspec(dllimport) RetType
#else
#define LIB_API(RetType) extern "C" RetType
#endif
#endif

#define VERSION "1.1"

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

/// @brief Task data structure for periodic tasks. \struct task_data
struct task_data
{
    long period_ns;
    long print_per_sec;
    int stress_threads;
    int duration_sec;
    int policy;
    int priority;
    const char* log_file;
    bool use_color;
};

/// @brief Periodic task information structure. \struct period_info
struct period_info
{
    struct timespec next_period;
    long period_ns;
    long print_rate;
    
    // Statistics
    unsigned long total_cycles;
    long double total_delay;
    long double max_delay;
    long double min_delay;
    unsigned long missed_deadlines;
};


#endif //RT_TESTER_H