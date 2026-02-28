# date

Standalone musl-libc-based Linux port of FreeBSD `date` for Project Tick BSD/Linux Distribution.

## Build

```sh
gmake -f GNUmakefile
gmake -f GNUmakefile CC=musl-gcc
```

## Test

```sh
gmake -f GNUmakefile test
gmake -f GNUmakefile test CC=musl-gcc
```

## Notes

- Port strategy is Linux-native syscall/API mapping, not a FreeBSD userland ABI shim.
- Time reads and writes use `clock_gettime(2)`, `clock_getres(2)`, and `clock_settime(2)`.
- `-r file` uses Linux `stat(2)` nanosecond timestamps via `st_mtim`.
- Time zone selection uses the libc `TZ` mechanism (`setenv("TZ", ...)` + `tzset()`), so named zones depend on installed tzdata and POSIX `TZ` strings work without glibc-specific behavior.
- `%N` formatting, ISO-8601 rendering, and FreeBSD `-v` adjustments are implemented in local project code so the port stays musl-clean.
- Unsupported semantics are explicit: FreeBSD `-n` is rejected on Linux because there is no equivalent timed/network-set path here.
- Setting the real-time clock still requires Linux `CAP_SYS_TIME`; the port does not emulate FreeBSD `utmpx`/syslog side effects when the clock changes.
