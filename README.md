# ed

Standalone musl-libc-based Linux port of FreeBSD `ed` for Project Tick BSD/Linux Distribution.

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

- Port strategy is direct Linux-native cleanup of the FreeBSD source, not a BSD ABI shim.
- The original multi-file editor core and FreeBSD regression corpus are preserved, but the Linux build surface is standalone: `GNUmakefile`, shell test entrypoint, and musl-safe libc usage.
- Scratch-buffer storage uses `mkstemp(3)` in `TMPDIR` or `/tmp`, then `fdopen(3)` for the editor's temp file stream.
- Shell escapes and filter I/O (`!`, `r !cmd`, `w !cmd`) use `system(3)`, `popen(3)`, and `pclose(3)`.
- Terminal resize handling uses Linux `ioctl(TIOCGWINSZ)` on stdin.
- Regex handling stays on POSIX `regcomp(3)` / `regexec(3)` from the active libc.
- FreeBSD `strlcpy(3)` usage was replaced with a local implementation so the port does not depend on glibc extensions or `libbsd`.

## Linux Semantics

- Supported: core editing commands, shell escapes, global commands, undo, binary buffer handling, and the upstream FreeBSD regression scripts.
- Supported: restricted `red` mode path checks from the original source.
- Unsupported: historic crypt mode. Startup `-x` exits with an explicit Linux error, and the interactive `x` command reports `crypt mode is not supported on Linux`.
- Known inherited quirk: the implicit newline print command still accepts the historic `,1` address form from this `ed` lineage. The Linux test harness allows only that single upstream deviation so other parser regressions still fail hard.

## What is ed?

ed is an 8-bit-clean, POSIX-compliant line editor.  It should work with
any regular expression package that conforms to the POSIX interface
standard, such as GNU regex(3).

If reliable signals are supported (e.g., POSIX sigaction(2)), it should
compile with little trouble.  Otherwise, the macros SPL1() and SPL0()
should be redefined to disable interrupts.

The following compiler directives are recognized:
NO_REALLOC_NULL	- if realloc(3) does not accept a NULL pointer
BACKWARDS	- for backwards compatibility
NEED_INSQUE	- if insque(3) is missing

The file `POSIX' describes extensions to and deviations from the POSIX
standard.

The ./test directory contains regression tests for ed. The README
file in that directory explains how to run these.

For a description of the ed algorithm, see Kernighan and Plauger's book
"Software Tools in Pascal," Addison-Wesley, 1981.
