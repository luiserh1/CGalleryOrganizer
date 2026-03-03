#include "memory_metrics.h"

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <stdio.h>
#include <unistd.h>
#endif

long long GetCurrentRssBytes(void) {
#if defined(__APPLE__)
  mach_task_basic_info_data_t info;
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  kern_return_t kr = task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                               (task_info_t)&info, &count);
  if (kr != KERN_SUCCESS) {
    return -1;
  }
  return (long long)info.resident_size;
#elif defined(__linux__)
  FILE *f = fopen("/proc/self/statm", "r");
  if (!f) {
    return -1;
  }

  unsigned long pages = 0;
  if (fscanf(f, "%*s %lu", &pages) != 1) {
    fclose(f);
    return -1;
  }
  fclose(f);

  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    return -1;
  }
  return (long long)pages * (long long)page_size;
#else
  return -1;
#endif
}

long long GetPeakRssBytes(void) {
#if defined(_WIN32)
  return -1;
#else
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    return -1;
  }

#if defined(__APPLE__)
  return (long long)usage.ru_maxrss;
#elif defined(__linux__)
  return (long long)usage.ru_maxrss * 1024LL;
#else
  return -1;
#endif
#endif
}
