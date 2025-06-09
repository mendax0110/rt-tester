# Real Time Tester
 
*A tool for testing real-time performance under Linux with SCHED_FIFO scheduling*

## Description

The Real Time Tester is a Linux application designed to evaluate real-time task scheduling performance under different system loads. It creates a periodic task with configurable scheduling parameters and measures timing accuracy while optionally generating CPU load.

Key features:
- Configurable task period (1μs - 1000ms)
- Multiple scheduling policies (FIFO, RR, OTHER)
- CPU load generation with stress threads
- Comprehensive statistics (min/max/avg delays, miss rates)
- Logging to file
- Platform information reporting

## Build Instructions

### Prerequisites
- Linux system (or WSL on Windows)
- GCC or Clang
- pthreads library
- CMake (optional)

### Building with CMake
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Cd into rt-tester directory
```bash
cd rt-tester
```

### Executing the Application
```bash
./rt-tester
```

## Usage with options
```bash
./rt-tester [options]

Options:
    -p, --period MS       Set task period in milliseconds (default: 100)
    -r, --rate HZ       Set task rate in Hertz (default: 5)
    -d, --duration SEC    Set test duration in seconds (default: 0)
    -s, --stress N      Set number of stress threads (default: 0)
    --policy POL  Set scheduling policy (FIFO, RR, OTHER, default: FIFO)
    --priority PRI      Set task priority (default: 80)
    -l, --log FILE      Log results to specified file
    --no-color       Disable colored output
    -i, --info         Show platform information
    -h, --help         Show this help message
```
