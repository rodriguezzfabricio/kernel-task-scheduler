# Task Scheduler (userspace + kernel)

A priority-based task scheduler with resource constraints, implemented two ways:

- `user_space/` — pthreads, mutexes/semaphores, and standard C data structures.
- `kernel_space/` — a Linux kernel module (`kqueue`) implementing the same
  scheduler and exposing it through custom syscalls.
- `kernel_module_v2/` — an alternate kernel-module implementation.
- `tests/` — test suites for the userspace and kernel implementations.
- `hello/` — a minimal "hello world" kernel module used as a build sanity check.

## Highlights
- Priority queues (high / medium / low) plus a waiting queue
- Per-task resource requirements; a task runs only when all its resources are free
- Highest-priority runnable task is scheduled first
- Custom syscalls: `add_task`, `schedule_tasks`, `acquire_resources`,
  `release_resources` (syscall numbers 549-552)

## Build
Userspace:
```bash
cd user_space && make
```

Kernel module: build against a Linux kernel source tree (set `KERNELDIR` in
`kernel_space/Makefile`, which defaults to the parent directory).
