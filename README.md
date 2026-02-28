# getfacl

Standalone musl-libc-friendly Linux port of FreeBSD `getfacl` for Project Tick BSD/Linux Distribution.

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

## Port strategy

- FreeBSD `sys/acl.h` and `acl_*_np()` dependencies were removed.
- Linux-native ACL reads use `stat(2)` / `lstat(2)` plus `getxattr(2)` / `lgetxattr(2)`.
- POSIX ACL data is decoded directly from `system.posix_acl_access` and `system.posix_acl_default`.
- If no access ACL xattr exists, the tool synthesizes the base ACL from file mode bits instead of depending on a compat library.

## Supported semantics on Linux

- Access ACL output for regular files and directories.
- Default ACL output for directories when `system.posix_acl_default` is present.
- `-d`, `-h`, `-n`, `-q`, and `-s`.
- Reading path lists from standard input when no file operands are supplied or when `-` is used.

## Unsupported semantics on Linux

- FreeBSD NFSv4-oriented flags `-i` and `-v` have no Linux POSIX ACL equivalent in this port and fail with an explicit error.
- `-h` against a symbolic link fails with an explicit error because Linux does not support POSIX ACLs on symlink inodes.

## Notes

- The parser validates ACL xattr structure and rejects malformed data instead of silently printing partial output.
- Verified with `gmake -f GNUmakefile clean test` and `gmake -f GNUmakefile clean test CC=musl-gcc`.
