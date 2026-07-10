#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdatomic.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

pid_t child_pid;

/*
 * child_running is read by two monitor threads (timeout_monitor and
 * resource_monitor) and written by main once the child has been reaped.
 * A plain int here would be a data race even though it "usually works"
 * on x86: the compiler is free to cache the value in a register inside
 * each monitor thread's loop (so a write from main might never be
 * observed), and the C standard gives no ordering guarantee between the
 * write in main and the reads in the threads. volatile only stops the
 * compiler from optimizing away the reload - it does NOT provide the
 * atomicity or cross-thread visibility that multiple threads need.
 * _Atomic (with atomic_load/atomic_store) gives both: every access is a
 * single indivisible operation and is a recognised synchronization point
 * under the C11 memory model, so all threads reliably see the same value.
 */
_Atomic int child_running = 1;

/* Peak resource stats. Only resource_monitor writes these, and main only
 * reads them after pthread_join(resource_thread, ...) has returned; a
 * pthread_join establishes a happens-before relationship with everything
 * the joined thread did, so no extra synchronization is needed here. */
static double peak_cpu_seconds = 0.0;
static long peak_rss_kb = 0;

static long clk_tck;
static long page_size_kb;

/* ---------------- /proc parsing helpers ---------------- */

/* Total CPU time (utime+stime) of pid, in seconds. -1.0 if unavailable
 * (e.g. the process has already exited). Reads /proc/<pid>/stat. */
double read_cpu_seconds(pid_t pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *f = fopen(path, "r");
    if (!f)
        return -1.0;

    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, f);
    fclose(f);

    if (n < 0)
    {
        free(line);
        return -1.0;
    }

    /* The 2nd field is "(comm)" and may itself contain spaces or
     * parentheses, so parse from the last ')' rather than counting
     * fields from the start of the line. After that: state, then 10
     * fields (ppid..cmajflt), then utime (field 14), then stime
     * (field 15). */
    char *rparen = strrchr(line, ')');
    if (!rparen)
    {
        free(line);
        return -1.0;
    }

    unsigned long utime = 0, stime = 0;
    int matched = sscanf(rparen + 1,
                          " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                          &utime, &stime);
    free(line);

    if (matched != 2)
        return -1.0;

    return (double)(utime + stime) / (double)clk_tck;
}

/* Resident set size of pid, in KB. -1 if unavailable. Reads
 * /proc/<pid>/statm, field 2 (resident pages). */
long read_rss_kb(pid_t pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/statm", pid);

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    long size_pages = 0, resident_pages = 0;
    int matched = fscanf(f, "%ld %ld", &size_pages, &resident_pages);
    fclose(f);

    if (matched != 2)
        return -1;

    return resident_pages * page_size_kb;
}

/* Number of processes currently owned by uid, counted directly from
 * /proc rather than shelling out, so the containment cap below has no
 * external dependency. */
static long count_processes_for_uid(uid_t uid)
{
    DIR *proc = opendir("/proc");
    if (!proc)
        return -1;

    long count = 0;
    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL)
    {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;

        char path[280];
        snprintf(path, sizeof(path), "/proc/%s", entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && st.st_uid == uid)
            count++;
    }
    closedir(proc);
    return count;
}

/* ---------------- monitor threads ---------------- */

void *timeout_monitor(void *arg)
{
    int timeout = *(int *)arg;

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    while (atomic_load(&child_running))
    {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - t0.tv_sec) + (now.tv_nsec - t0.tv_nsec) / 1e9;

        if (elapsed >= timeout)
        {
            if (atomic_load(&child_running))
            {
                printf("[Monitor] Time limit exceeded.\n");
                /* Negative pid == signal the whole process group, so any
                 * processes the child forked (e.g. a fork-bomb payload)
                 * are terminated along with it, not left as orphans. */
                kill(-child_pid, SIGKILL);
                printf("[Monitor] Child terminated using SIGKILL.\n");
            }
            break;
        }

        usleep(100000); /* poll every 100ms instead of one long sleep,
                            so we notice promptly once the child exits */
    }

    pthread_exit(NULL);
}

void *resource_monitor(void *arg)
{
    (void)arg;

    while (atomic_load(&child_running))
    {
        double cpu = read_cpu_seconds(child_pid);
        long rss = read_rss_kb(child_pid);

        if (cpu >= 0.0)
        {
            if (cpu > peak_cpu_seconds)
                peak_cpu_seconds = cpu;
            printf("[Monitor] Sample: cpu=%.2fs mem=%ldKB\n", cpu, rss >= 0 ? rss : 0);
        }
        if (rss > peak_rss_kb)
            peak_rss_kb = rss;

        usleep(200000); /* 200ms sampling interval */
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    pthread_t timeout_thread, resource_thread;
    int timeout = 5;
    const char *target = (argc > 1) ? argv[1] : "./test_binary";

    clk_tck = sysconf(_SC_CLK_TCK);
    page_size_kb = sysconf(_SC_PAGESIZE) / 1024;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    child_pid = fork();

    if (child_pid < 0)
    {
        perror("fork");
        return 1;
    }

    if (child_pid == 0)
    {
        printf("[Child] Executing %s...\n", target);

        /* Put the child in its own process group so the sandbox can
         * terminate the whole group (child + any processes it forks,
         * e.g. a fork-bomb payload) with a single kill(-pgid, ...)
         * instead of only the immediate child, which would otherwise
         * leave descendants running as orphans after the demo ends. */
        if (setpgid(0, 0) != 0)
            perror("[Child] setpgid");

        /* Containment: cap the number of processes this UID may hold,
         * relative to what's already running, so a fork-bomb style
         * payload cannot exhaust system-wide PIDs. Once the cap is
         * hit, fork() inside the child just fails with EAGAIN instead
         * of spawning further processes. */
        long current = count_processes_for_uid(getuid());
        struct rlimit rl;
        if (current > 0 && getrlimit(RLIMIT_NPROC, &rl) == 0)
        {
            rlim_t cap = (rlim_t)current + 40;
            if (rl.rlim_max != RLIM_INFINITY && cap > rl.rlim_max)
                cap = rl.rlim_max;

            rl.rlim_cur = cap;
            if (setrlimit(RLIMIT_NPROC, &rl) == 0)
                printf("[Child] Process limit capped at %ld.\n", (long)cap);
            else
                perror("[Child] setrlimit(RLIMIT_NPROC)");
        }

        char *args[] = {(char *)target, NULL};

        execve(target, args, NULL);

        perror("execve");
        exit(EXIT_FAILURE);
    }

    printf("[Parent] Target binary = %s\n", target);
    printf("[Parent] Child PID = %d\n", child_pid);

    pthread_create(&timeout_thread, NULL, timeout_monitor, &timeout);
    pthread_create(&resource_thread, NULL, resource_monitor, NULL);

    int status;

    waitpid(child_pid, &status, 0);

    atomic_store(&child_running, 0);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double total_runtime = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    pthread_join(timeout_thread, NULL);
    pthread_join(resource_thread, NULL);

    const char *verdict = "UNKNOWN";
    int signal_num = 0;
    int exit_code = 0;

    if (WIFEXITED(status))
    {
        verdict = "EXITED";
        exit_code = WEXITSTATUS(status);
        printf("[Parent] Child exited normally.\n");
    }
    else if (WIFSIGNALED(status))
    {
        verdict = "KILLED";
        signal_num = WTERMSIG(status);
        printf("[Parent] Child killed by signal %d\n", signal_num);
    }

    printf("[Parent] Total runtime: %.2fs\n", total_runtime);
    printf("[Parent] Peak CPU time: %.2fs\n", peak_cpu_seconds);
    printf("[Parent] Peak memory (RSS): %ldKB\n", peak_rss_kb);

    FILE *log = fopen("sandbox.log", "a");

    if (log)
    {
        time_t now = time(NULL);

        fprintf(log, "---- Execution finished at %s", ctime(&now));
        fprintf(log, "Target binary : %s\n", target);
        fprintf(log, "Child PID     : %d\n", child_pid);
        fprintf(log, "Verdict       : %s", verdict);
        if (signal_num)
            fprintf(log, " (signal %d / %s)", signal_num, strsignal(signal_num));
        else
            fprintf(log, " (exit code %d)", exit_code);
        fprintf(log, "\n");
        fprintf(log, "Total runtime : %.2fs\n", total_runtime);
        fprintf(log, "Peak CPU time : %.2fs\n", peak_cpu_seconds);
        fprintf(log, "Peak memory   : %ldKB\n\n", peak_rss_kb);

        fclose(log);
    }

    printf("[Parent] Log written to sandbox.log\n");

    return 0;
}
