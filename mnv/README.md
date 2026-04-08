# [![MNV The editor](https://github.com/Project-Tick/Project-Tick/raw/master/mnv/runtime/mnvlogo.gif)](https://www.projecttick.com)

[![CI](https://github.com/Project-Tick/Project-Tick/actions/workflows/ci.yml/badge.svg)](https://github.com/Project-Tick/Project-Tick/actions/workflows/ci.yml)
[![Coverity Scan](https://scan.coverity.com/projects/241/badge.svg)](https://scan.coverity.com/projects/mnv)
[![Packages](https://repology.org/badge/tiny-repos/mnv.svg)](https://repology.org/metapackage/mnv)

If you find a bug or want to discuss the best way to add a new feature, please
[open an issue](https://github.com/Project-Tick/Project-Tick/issues/new/choose).
If you have a question or want to discuss the best way to do something with
MNV, you can use [Discussions of Project Tick](https://github.com/Project-Tick/Project-Tick/discussions).

## What is MNV?

Firstly, MNV is fork of Vi IMproved. MNV is a greatly improved version of the
good old UNIX editor [Vi](https://en.wikipedia.org/wiki/Vi_(text_editor)). Many
new features have been added: multi-level undo, syntax highlighting, command
line history, on-line help, spell checking, filename completion, block
operations, script language, etc.  There is also a Graphical User Interface
(GUI) available.  Still, Vi compatibility is maintained, those who have Vi "in
the fingers" will feel at home.
See [`runtime/doc/vi_diff.txt`](runtime/doc/vi_diff.txt) for differences with
Vi.

This editor is very useful for editing programs and other plain text files.
All commands are given with normal keyboard characters, so those who can type
with ten fingers can work very fast.  Additionally, function keys can be
mapped to commands by the user, and the mouse can be used.

MNV also aims to provide a (mostly) POSIX-compatible vi implementation, when
compiled with a minimal feature set (typically called mnv.tiny), which is used
by many Linux distributions as the default vi editor.

MNV runs under MS-Windows (7, 8, 10, 11), macOS, Haiku, VMS and almost all
flavours of UNIX.  Porting to other systems should not be very difficult.
Older versions of MNV run on MS-DOS, MS-Windows 95/98/Me/NT/2000/XP/Vista,
Amiga DOS, Atari MiNT, BeOS, RISC OS and OS/2.  These are no longer maintained.

## Distribution

You can often use your favorite package manager to install MNV.  On Mac and
Linux a small version of MNV is pre-installed, you still need to install MNV
if you want more features.

There are separate distributions for Unix, PC, Amiga and some other systems.
This `README.md` file comes with the runtime archive.  It includes the
documentation, syntax files and other files that are used at runtime.  To run
MNV you must get either one of the binary archives or a source archive.
Which one you need depends on the system you want to run it on and whether you
want or must compile it yourself.  Check https://www.mnv.org/download.php for
an overview of currently available distributions.

Some popular places to get the latest MNV:
* Check out the git repository from [GitHub](https://github.com/Project-Tick/Project-Tick).
* Get the source code as an [archive](https://github.com/Project-Tick/Project-Tick/tags).

## Compiling

If you obtained a binary distribution you don't need to compile MNV.  If you
obtained a source distribution, all the stuff for compiling MNV is in the
[`src`](./src/) directory.  See [`src/INSTALL`](./src/INSTALL) for instructions.

## Installation

See one of these files for system-specific instructions.  Either in the
[READMEdir directory](./READMEdir/) (in the repository) or
the top directory (if you unpack an archive):

```
README_ami.txt		Amiga
README_unix.txt		Unix
README_dos.txt		MS-DOS and MS-Windows
README_mac.txt		Macintosh
README_haiku.txt	Haiku
README_vms.txt		VMS
```

There are other `README_*.txt` files, depending on the distribution you used.

## Documentation

The MNV tutor is a one hour training course for beginners.  Often it can be
started as `mnvtutor`.  See `:help tutor` for more information.

The best is to use `:help` in MNV.  If you don't have an executable yet, read
[`runtime/doc/help.txt`](./runtime/doc/help.txt).
It contains pointers to the other documentation files.
The User Manual reads like a book and is recommended to learn to use
MNV.  See `:help user-manual`.

## Copying

MNV is Free. You can use and copy it as much as you like. Please read the file
[`runtime/doc/license.txt`](./runtime/doc/license.txt)
for details (do `:help license` inside MNV).

Summary of the license: There are no restrictions on using or distributing an
unmodified copy of MNV.  Parts of MNV may also be distributed, but the license
text must always be included.  For modified versions, a few restrictions apply.
The license is GPL compatible, you may compile MNV with GPL libraries and
distribute it.

## Contributing

If you would like to help make MNV better, see the
[CONTRIBUTING.md](./CONTRIBUTING.md) file.

## Main author

Most of Vim was created by Bram Moolenaar `<Bram@vim.org>`
[Bram-Moolenaar](https://vimhelp.org/version9.txt.html#Bram-Moolenaar) but MNV maintained by [Mehmet Samet Duman](https://github.com/YongDo-Hyun) behalf of [Project Tick](https://github.com/Project-Tick).

This is `README.md` for version 10.0 of MNV: MNV is not Vim.
