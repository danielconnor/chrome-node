/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <kvm.h>
#include <paths.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <vm/vm_param.h> /* VM_LOADAVG */
#include <time.h>
#include <stdlib.h>
#include <unistd.h> /* sysconf */
#include <fcntl.h>

#undef NANOSEC
#define NANOSEC 1000000000


static char *process_title;


uint64_t uv_hrtime(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * NANOSEC + ts.tv_nsec);
}


int uv_exepath(char* buffer, size_t* size) {
  int mib[4];
  size_t cb;

  if (!buffer || !size) {
    return -1;
  }

#ifdef __DragonFly__
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_ARGS;
  mib[3] = getpid();
#else
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;
#endif

  cb = *size;
  if (sysctl(mib, 4, buffer, &cb, NULL, 0) < 0) {
    *size = 0;
    return -1;
  }
  *size = strlen(buffer);

  return 0;
}


uint64_t uv_get_free_memory(void) {
  int freecount;
  size_t size = sizeof(freecount);

  if(sysctlbyname("vm.stats.vm.v_free_count",
                  &freecount, &size, NULL, 0) == -1){
    return -1;
  }
  return (uint64_t) freecount * sysconf(_SC_PAGESIZE);

}


uint64_t uv_get_total_memory(void) {
  unsigned long info;
  int which[] = {CTL_HW, HW_PHYSMEM};

  size_t size = sizeof(info);

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return -1;
  }

  return (uint64_t) info;
}


void uv_loadavg(double avg[3]) {
  struct loadavg info;
  size_t size = sizeof(info);
  int which[] = {CTL_VM, VM_LOADAVG};

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) return;

  avg[0] = (double) info.ldavg[0] / info.fscale;
  avg[1] = (double) info.ldavg[1] / info.fscale;
  avg[2] = (double) info.ldavg[2] / info.fscale;
}


char** uv_setup_args(int argc, char** argv) {
  process_title = argc ? strdup(argv[0]) : NULL;
  return argv;
}


uv_err_t uv_set_process_title(const char* title) {
  if (process_title) free(process_title);
  process_title = strdup(title);
  setproctitle(title);
  return uv_ok_;
}


uv_err_t uv_get_process_title(char* buffer, size_t size) {
  if (process_title) {
    strncpy(buffer, process_title, size);
  } else {
    if (size > 0) {
      buffer[0] = '\0';
    }
  }

  return uv_ok_;
}


uv_err_t uv_resident_set_memory(size_t* rss) {
  kvm_t *kd = NULL;
  struct kinfo_proc *kinfo = NULL;
  pid_t pid;
  int nprocs;
  size_t page_size = getpagesize();

  pid = getpid();

  kd = kvm_open(NULL, _PATH_DEVNULL, NULL, O_RDONLY, "kvm_open");
  if (kd == NULL) goto error;

  kinfo = kvm_getprocs(kd, KERN_PROC_PID, pid, &nprocs);
  if (kinfo == NULL) goto error;

  *rss = kinfo->ki_rssize * page_size;

  kvm_close(kd);

  return uv_ok_;

error:
  if (kd) kvm_close(kd);
  return uv__new_sys_error(errno);
}


uv_err_t uv_uptime(double* uptime) {
  time_t now;
  struct timeval info;
  size_t size = sizeof(info);
  static int which[] = {CTL_KERN, KERN_BOOTTIME};

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return uv__new_sys_error(errno);
  }

  now = time(NULL);

  *uptime = (double)(now - info.tv_sec);
  return uv_ok_;
}


uv_err_t uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK),
               multiplier = ((uint64_t)1000L / ticks), cpuspeed, maxcpus,
               cur = 0;
  char model[512];
  int numcpus;
  size_t size;
  uv_cpu_info_t* cpu_info;

  size = sizeof(model);
  if (sysctlbyname("hw.model", &model, &size, NULL, 0) < 0) {
    return uv__new_sys_error(errno);
  }
  size = sizeof(numcpus);
  if (sysctlbyname("hw.ncpu", &numcpus, &size, NULL, 0) < 0) {
    return uv__new_sys_error(errno);
  }

  *cpu_infos = (uv_cpu_info_t*)malloc(numcpus * sizeof(uv_cpu_info_t));
  if (!(*cpu_infos)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  *count = numcpus;

  size = sizeof(cpuspeed);
  if (sysctlbyname("hw.clockrate", &cpuspeed, &size, NULL, 0) < 0) {
    free(*cpu_infos);
    return uv__new_sys_error(errno);
  }
  // kern.cp_times on FreeBSD i386 gives an array up to maxcpus instead of ncpu
  size = sizeof(maxcpus);
  if (sysctlbyname("kern.smp.maxcpus", &maxcpus, &size, NULL, 0) < 0) {
    free(*cpu_infos);
    return uv__new_sys_error(errno);
  }
  size = maxcpus * CPUSTATES * sizeof(long);
  long cp_times[size];
  if (sysctlbyname("kern.cp_times", &cp_times, &size, NULL, 0) < 0) {
    free(*cpu_infos);
    return uv__new_sys_error(errno);
  }

  for (int i = 0; i < numcpus; i++) {
    cpu_info = &(*cpu_infos)[i];
    
    cpu_info->cpu_times.user = (uint64_t)(cp_times[CP_USER+cur]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(cp_times[CP_NICE+cur]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(cp_times[CP_SYS+cur]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(cp_times[CP_IDLE+cur]) * multiplier;
    cpu_info->cpu_times.irq = (uint64_t)(cp_times[CP_INTR+cur]) * multiplier;

    cpu_info->model = strdup(model);
    cpu_info->speed = cpuspeed;

    cur+=CPUSTATES;
  }

  return uv_ok_;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(cpu_infos[i].model);
  }

  free(cpu_infos);
}


uv_err_t uv_interface_addresses(uv_interface_address_t** addresses,
  int* count) {
  /* TODO: implement */
  *addresses = NULL;
  *count = 0;
  return uv_ok_;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
}
