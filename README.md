# ST5039CMD Programming and Operating Systems — Coursework CW1

Process isolation, privilege separation and user-space sandboxing on a Unix-based
operating system. Two C programs, their test payloads, the diagrams and the written
report.

All programs were written and tested on Kali Linux. They use Linux/POSIX system
calls (`fork`, `execve`, `setresuid`, Unix domain sockets, `/proc`, `pthreads`,
signals) and will **not** build on Windows or macOS — use a Linux machine or VM.

---

## Repository layout

```
ST5039CMD_CW1_Deliverables/
├── report/
│   └── ST5039CMD_CW1_Report.docx     ← the written report (open in Word, F9 to update fields)
├── src/
│   ├── task1/                        ← privilege-separated authentication service
│   │   ├── common.h                  ← shared protocol (socket path + LoginRequest)
│   │   ├── frontend.c                ← unprivileged client (reads the credentials)
│   │   ├── backend.c                 ← privileged server (validates, drops privilege)
│   │   └── Makefile
│   ├── task2/                        ← user-space sandbox
│   │   ├── sandbox.c                 ← supervisor: fork/execve + 2 monitor threads
│   │   ├── benign.c                  ← payload: sleeps 1 s then exits
│   │   ├── cpu_hog.c                 ← payload: tight infinite CPU loop
│   │   ├── fork_bomb.c               ← payload: unbounded fork()
│   │   ├── ignore_sigterm.c          ← payload: traps SIGTERM, loops forever
│   │   ├── test_binary.c             ← payload: prints and sleeps in a loop
│   │   ├── sandbox.log               ← captured results (append-only audit trail)
│   │   └── Makefile
│   ├── malicious_client.c            ← interconnection: attacks Task 1, then goes rogue
│   └── frontend_sandbox_patch.txt    ← optional patch: login gates the sandbox
├── diagrams/                         ← figures used in the report (PNG)
└── evidence/                         ← terminal screenshots referenced in the report
    ├── task1/
    └── task2/
```

---

## Task 1 — Privilege separation in password validation

A login service split into two processes so that the component reading the password
never shares an address space with the component that validates it.

- **`frontend`** reads a username and password from the terminal and sends them over a
  Unix domain socket (`/tmp/auth_socket`). It holds no validation logic.
- **`backend`** receives the request, drops privilege with `setresuid` and verifies the
  drop at runtime with `geteuid`, compares the credentials, appends the outcome to
  `auth.log`, and clears its copy of the password buffer with `memset` before replying.

### Build and run

```bash
cd src/task1
make
# terminal 1:
./backend
# terminal 2:
./frontend        # try  admin / Password123  -> SUCCESS
                  # try  admin / anythingelse  -> FAILED
cat auth.log      # append-only record of both attempts
```

### What to look for
- The backend prints its real and effective UID **before and after** the `setresuid`
  call, then `Privileges dropped successfully.` — this is the runtime verification.
- The backend prints `Password buffer cleared from memory.` once validation is done.
- `auth.log` gains one `SUCCESS` / `FAILED` line per attempt.

> Note: as submitted, the backend runs as an ordinary user, so the UID is `1000` before
> and after the drop — it demonstrates the `setresuid` + runtime-check mechanism. The
> report's critical-evaluation section explains how a full setuid-root version would
> differ.

---

## Task 2 — User-space malware analysis sandbox

A supervisor that runs an untrusted binary under external monitoring. The untrusted
binary takes **no part** in its own monitoring or termination.

- `fork` + `execve` launch the target in its **own process group** (`setpgid`).
- Before `execve`, `RLIMIT_NPROC` caps how many processes the target may create, so a
  fork bomb cannot exhaust the system.
- Two **pthreads** monitor from the outside: one watches wall-clock time, one samples
  CPU time and resident memory from `/proc/<pid>/stat` and `/proc/<pid>/statm` every
  200 ms.
- The shared `child_running` flag is a **C11 `_Atomic int`** (not `volatile`) so all
  threads see a consistent value.
- On timeout the supervisor sends **`SIGKILL` to the whole process group** (`kill(-pgid,…)`)
  — uncatchable, so a target that ignores `SIGTERM` is stopped anyway.
- Every run appends a verdict block to `sandbox.log`.

### Build and run

```bash
cd src/task2
make                       # builds sandbox + all payloads
./sandbox ./benign         # EXITED  (finishes within the 5 s limit)
./sandbox ./cpu_hog        # KILLED  (signal 9) at the limit
./sandbox ./fork_bomb      # KILLED, contained by RLIMIT_NPROC + process-group kill
./sandbox ./ignore_sigterm # KILLED, because the sandbox uses SIGKILL not SIGTERM
cat sandbox.log            # the audit trail
```

### Results already captured (`sandbox.log`)

| Payload         | Verdict            | Runtime | Peak CPU | Peak RSS |
|-----------------|--------------------|---------|----------|----------|
| benign          | EXITED (code 0)    | 1.00 s  | 0.00 s   | 1408 KB  |
| cpu_hog         | KILLED (signal 9)  | 5.06 s  | 5.00 s   | 1236 KB  |
| fork_bomb       | KILLED (signal 9)  | 5.08 s  | 4.82 s   | 1568 KB  |
| ignore_sigterm  | KILLED (signal 9)  | 5.09 s  | 0.00 s   | 1380 KB  |

---

## Interconnection — one binary, two outcomes

`malicious_client.c` reuses Task 1's `common.h`, so it speaks the same protocol as the
real frontend. Run bare, it guesses passwords against the Task 1 server and then enters
an infinite CPU loop that never returns. Run under the Task 2 sandbox, the same binary
is caught at the time limit and killed. Build it inside `task1/` (it needs `common.h`),
then hand the resulting binary to the sandbox:

```bash
cp src/malicious_client.c src/task1/
cd src/task1
gcc -Wall -Wextra malicious_client.c -o test_binary
../task2/sandbox ./test_binary
```

`frontend_sandbox_patch.txt` shows an optional variant in which a successful login is
what launches the sandbox.

---

## Report

`report/ST5039CMD_CW1_Report.docx` (and the matching `.pdf`) is the full write-up:
design, implementation, testing, the answers to all investigation questions, critical
evaluation and Harvard references. It is organised as two self-contained chapters —
Task 1 (~2,000 words) and Task 2 (~3,000 words) — with the cover page, abstract,
contents lists, references and appendices outside the counted text.

The table of contents, list of figures and list of tables are already populated. If you
edit the document, press **Ctrl+A then F9** in Word to refresh them.
