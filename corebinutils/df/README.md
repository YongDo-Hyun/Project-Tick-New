# df

Standalone musl-libc-based Linux port of FreeBSD `df` for Project Tick BSD/Linux Distribution.

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

- Port strategy is Linux-native mount and stat collection, not a FreeBSD ABI shim.
- Mount enumeration uses `/proc/self/mountinfo`; capacity and inode counts use `statvfs(3)` on each selected mountpoint.
- FreeBSD `getmntinfo(3)`, `sysctl(vfs.conflist)`, `getbsize(3)`, `humanize_number(3)`, and `libxo` are removed from the standalone port.
- `-l` is implemented with Linux-oriented remote-filesystem detection based on mount type, `_netdev`, and remote-style sources such as `host:/path` or `//server/share`.
- Linux has no equivalent of FreeBSD's cached `statfs` refresh control, so `-n` fails with an explicit error instead of silently degrading.
- Linux also has no `MNT_IGNORE` mount flag equivalent; as a result `-a` is accepted but has no additional effect in this port.
