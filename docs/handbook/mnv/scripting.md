# MNV — Command-Line Interface and Scripting

## Executable Invocation

MNV is a single binary that adapts its behaviour based on the name it is
launched with.  The `parse_command_name()` function in `src/main.c` determines
the mode:

```c
static void parse_command_name(mparm_T *parmp);
```

| Invocation | Mode | Effect |
|---|---|---|
| `mnv` | Normal | Standard editor |
| `ex` | Ex | Starts in `:` line mode |
| `view` | Read-only | Equivalent to `mnv -R` |
| `rmnv` | Restricted | Disables shell commands, file writes |
| `rview` | Restricted+RO | Restricted + read-only |
| `mnvdiff` | Diff | Opens files in diff mode (`mnv -d`) |
| `vi` / `vim` | Compat aliases | Normal mode |
| `gmnv` | GUI | Starts graphical interface |
| `gview` | GUI+RO | GUI + read-only |
| `gmnvdiff` | GUI+Diff | GUI diff mode |
| `emnv` | Easy GUI | GUI in easy mode (insert mode by default) |
| `eview` | Easy+RO | Easy mode + read-only |

---

## Command-Line Arguments

The `command_line_scan()` function in `src/main.c` parses all command-line
arguments:

```c
static void command_line_scan(mparm_T *parmp);
```

### File Arguments

Any non-option arguments are treated as files to edit:

```
mnv file1.c file2.c file3.c
```

These populate the argument list, managed by `src/arglist.c`.

### Standard Options

| Flag | Description |
|---|---|
| `-R` | Read-only mode |
| `-Z` | Restricted mode (no shell access) |
| `-g` | Start GUI (equivalent to `gmnv`) |
| `-d` | Diff mode — open 2-8 files side by side |
| `-b` | Binary mode |
| `-l` | Lisp mode |
| `-M` | Not modifiable — don't allow changes |
| `-e` | Ex mode (`:` prompt) |
| `-E` | Improved Ex mode |
| `-s` | Silent/batch mode (Ex mode, no prompts) |
| `-y` | Easy mode |
| `-f` | Foreground — don't fork (GUI) |
| `-v` | Force terminal mode even if `gmnv` |
| `-n` | No swap file |
| `-r` | Recovery mode (list swap files or recover) |
| `-L` | Same as `-r` |
| `-p[N]` | Open N tab pages |
| `-o[N]` | Open N horizontal splits |
| `-O[N]` | Open N vertical splits |
| `-t tag` | Edit file containing `tag` |
| `-q file` | Start in quickfix mode with `file` |

### Pre- and Post-Commands

| Flag | Description |
|---|---|
| `--cmd <command>` | Execute `<command>` **before** sourcing `.mnvrc`. Up to 10. |
| `-c <command>` / `+<command>` | Execute `<command>` **after** loading files. Up to 10. |
| `+` | Start at last line of first file |
| `+{num}` | Start at line `{num}` of first file |

The error message for too many commands:

```c
N_("Too many \"+command\", \"-c command\" or \"--cmd command\" arguments"),
#define ME_EXTRA_CMD  4
```

### Startup Control

| Flag | Description |
|---|---|
| `--clean` | Skip all config files (`.mnvrc`, plugins) |
| `-u <file>` | Use `<file>` instead of `.mnvrc` |
| `-U <file>` | Use `<file>` instead of `.gmnvrc` |
| `-i <file>` | Use `<file>` instead of `.mnvinfo` |
| `--noplugin` | Don't load any plugins |
| `--startuptime <file>` | Log startup timing to `<file>` |
| `--log <file>` | Enable channel/job logging to `<file>` |

The `--startuptime` flag is scanned in `early_arg_scan()`:

```c
if (STRICMP(argv[i], "--startuptime") == 0 && time_fd == NULL)
{
    time_fd = mch_fopen(argv[i + 1], "a");
    TIME_MSG("--- MNV STARTING ---");
}
```

### Display Options

| Flag | Description |
|---|---|
| `-T <terminal>` | Set terminal type |
| `--not-a-term` | Skip terminal checks |
| `--ttyfail` | Exit if stdin is not a terminal |

### Client-Server

| Flag | Description |
|---|---|
| `--servername <name>` | Set server name |
| `--serverlist` | List running MNV servers |
| `--remote <files>` | Open files in an existing MNV |
| `--remote-send <keys>` | Send keys to a running MNV |
| `--remote-expr <expr>` | Evaluate expression in a running MNV |
| `--remote-wait` | Like `--remote` but wait for completion |
| `--remote-tab` | Like `--remote` but open in new tab |

### Information

| Flag | Description |
|---|---|
| `-h` / `--help` | Print usage and exit |
| `--version` | Print version and exit |

### Error Handling

Unrecognised options produce:

```c
static char *(main_errors[]) =
{
    N_("Unknown option argument"),      // ME_UNKNOWN_OPTION
    N_("Too many edit arguments"),       // ME_TOO_MANY_ARGS
    N_("Argument missing after"),        // ME_ARG_MISSING
    N_("Garbage after option argument"), // ME_GARBAGE
    N_("Too many \"+command\", \"-c command\" or \"--cmd command\" arguments"),
    N_("Invalid argument for"),          // ME_INVALID_ARG
};
```

---

## Startup Sequence

When `main()` runs (see `src/main.c`), parameter processing happens in
several phases:

### 1. Early Argument Scan

```c
static void early_arg_scan(mparm_T *parmp);
```

Scans specifically for `--startuptime`, `--log`, and `--clean` **before** any
initialisation, because these affect how initialisation proceeds.

### 2. Common Init Phase 1

`common_init_1()` — allocator, hash tables, global options, message system.

### 3. Common Init Phase 2

`common_init_2(&params)` — terminal detection, default options, langmap.

### 4. Full Command-Line Scan

`command_line_scan(&params)` — processes every flag and file argument.

### 5. TTY Check

```c
static void check_tty(mparm_T *parmp);
```

Verifies stdin/stdout are terminals when running interactively.

### 6. Source Startup Scripts

```c
static void source_startup_scripts(mparm_T *parmp);
```

Loads scripts in this order (unless `--clean` or `-u NONE`):

1. System-wide `.mnvrc` (e.g. `/etc/mnv/mnvrc`).
2. User `.mnvrc` (`$HOME/.mnvrc` or `$XDG_CONFIG_HOME/mnv/mnvrc`).
3. The `.gmnvrc` equivalent if GUI.
4. `defaults.mnv` (new-user defaults).

### 7. Pre-Commands

```c
static void exe_pre_commands(mparm_T *parmp);
```

Executes `--cmd` arguments.

### 8. Edit Buffers

```c
static void edit_buffers(mparm_T *parmp, char_u *cwd);
```

Opens file arguments into buffers and windows.

### 9. Post-Commands

```c
static void exe_commands(mparm_T *parmp);
```

Executes `-c` / `+` arguments.

### 10. GUI Start

```c
static void main_start_gui(void);
```

If GUI mode is detected, starts the GUI event loop.

---

## MNVscript: The Built-in Scripting Language

MNV includes a full scripting language for automation, plugins, and
configuration.  Two variants exist:

### Legacy MNVscript

The original `:let`, `:if`, `:while`, `:function` syntax, interpreted at
runtime by `src/eval.c`.  Expression parsing uses a recursive-descent parser:

```c
static int eval0_simple_funccal(...);
static int eval2(char_u **arg, typval_T *rettv, evalarg_T *evalarg);
static int eval3(...);
// ... through eval9()
```

The central value type is `typval_T` (`src/structs.h`), a tagged union
supporting:

- Numbers (`VAR_NUMBER`)
- Strings (`VAR_STRING`)
- Floats (`VAR_FLOAT`)
- Lists (`VAR_LIST`, `src/list.c`)
- Dictionaries (`VAR_DICT`, `src/dict.c`)
- Blobs (`VAR_BLOB`, `src/blob.c`)
- Tuples (`VAR_TUPLE`, `src/tuple.c`)
- Funcref / Partial (`VAR_FUNC`, `VAR_PARTIAL`)
- Jobs (`VAR_JOB`)
- Channels (`VAR_CHANNEL`)
- Classes / Objects (`VAR_CLASS`, `VAR_OBJECT`)

Variable scoping uses namespace prefixes (`g:`, `b:`, `w:`, `t:`, `l:`, `s:`,
`v:`), managed by `src/evalvars.c`.

Built-in functions are implemented in `src/evalfunc.c`.

### MNV9 Script

The modern dialect, activated by `:mnv9script` at the top of a script file or
by the `:mnv9cmd` modifier.  Detection:

```c
int
in_mnv9script(void)
{
    return (current_sctx.sc_version == SCRIPT_VERSION_MNV9
                     || (cmdmod.cmod_flags & CMOD_MNV9CMD))
                && !(cmdmod.cmod_flags & CMOD_LEGACY);
}
```

MNV9 features:

- **Type annotations**: `var name: string = "hello"`.
- **Compiled to bytecode**: `src/mnv9compile.c` compiles, `src/mnv9execute.c`
  runs the instructions defined in `src/mnv9instr.c`.
- **Classes and interfaces**: `src/mnv9class.c` — `class`, `interface`,
  `extends`, `implements`.
- **Generics**: `src/mnv9generics.c` — `<T>`, `<K, V>`.
- **Import/Export**: `src/mnv9script.c` — `import` / `export` for module
  systems.
- **`def` functions**: Compiled functions replacing `function`/`endfunction`.
- **Strict mode**: variables must be declared, types are checked.

Script version identification:

```c
#define SCRIPT_VERSION_MAX  4
#define SCRIPT_VERSION_MNV9 999999
```

### Profiling

When `FEAT_PROFILE` is defined, MNV can profile scripts and functions:

```
:profile start profile.log
:profile func *
:profile file *.mnv
```

Profiling is implemented in `src/profiler.c`.

### Debugging

MNV includes a built-in script debugger (`src/debugger.c`):

```
:breakadd func MyFunction
:debug call MyFunction()
```

---

## Ex Commands

Ex commands (`:` commands) are the backbone of MNV's command-line mode.  They
are defined in `src/ex_cmds.h` and dispatched by `src/ex_docmd.c`:

```c
static char_u *do_one_cmd(...);
```

Command index tables in `ex_cmdidxs.h` (generated by `create_cmdidxs.mnv`)
enable fast lookup.

### Command Execution

Every ex command receives an `exarg_T` struct containing:

- The command address range (line numbers).
- `:` modifiers (`:silent`, `:verbose`, `:sandbox`, `:lockmarks`, etc.).
- The command argument string.
- Flags for bang (`!`), register, count.

### Notable Command Families

| Family | Files | Examples |
|---|---|---|
| File operations | `ex_cmds.c` | `:write`, `:edit`, `:saveas` |
| Buffer management | `ex_cmds.c`, `buffer.c` | `:bnext`, `:bdelete`, `:buffers` |
| Window commands | `window.c` | `:split`, `:vsplit`, `:close`, `:only` |
| Script evaluation | `ex_eval.c` | `:try`, `:catch`, `:throw`, `:finally` |
| Source / Runtime | `ex_cmds2.c`, `scriptfile.c` | `:source`, `:runtime` |
| Help | `help.c` | `:help`, `:helpgrep` |
| Quickfix | `quickfix.c` | `:make`, `:copen`, `:cnext`, `:grep` |
| Autocmds | `autocmd.c` | `:autocmd`, `:doautocmd`, `:augroup` |
| Terminal | `terminal.c` | `:terminal` |
| Session | `session.c` | `:mksession`, `:mkview` |
| Diff | `diff.c` | `:diffthis`, `:diffoff`, `:diffupdate` |
| Fold | `fold.c` | `:fold`, `:foldopen`, `:foldclose` |

---

## Autocommands

`src/autocmd.c` implements the event-driven scripting system:

```
:autocmd BufWritePre *.c  call CleanWhitespace()
:autocmd FileType python  setlocal tabstop=4
```

Autocommand events cover the full editor lifecycle: buffer loading, writing,
window events, filetype detection, terminal activity, cursor movement, etc.

The `autocmd_init()` function (called early in `main()`) initialises the
autocommand tables.

---

## Key Mapping

`src/map.c` handles:

- `:map` / `:noremap` / `:unmap` for normal mode.
- `:imap` / `:inoremap` for insert mode.
- `:cmap` / `:cnoremap` for command-line mode.
- `:vmap` / `:vnoremap` for visual mode.
- `:tmap` / `:tnoremap` for terminal mode.
- And all operator / select mode variants.

The mapping engine integrates with `src/getchar.c` (typeahead buffer) to remap
key sequences on the fly.

---

## Registers

`src/register.c` manages:

- Named registers (`"a` – `"z`).
- Numbered registers (`"0` – `"9`).
- Small delete register (`"-`).
- System clipboard registers (`"*`, `"+`).
- Expression register (`"=`).
- Search register (`"/`).
- Last inserted text (`".`).
- Read-only registers (`"%` filename, `"#` alternate, `":` last command).
- Black hole register (`"_`).

The `"+` and `"*` registers bridge to the system clipboard via
`src/clipboard.c` (and on Wayland via `src/wayland.c`).

---

## Configuration Files

### `.mnvrc`

The primary user configuration file.  Sourced during startup.  Can contain any
ex commands, option settings, key mappings, autocommands, and function
definitions.

### `.gmnvrc`

GUI-specific configuration.  Sourced after `.mnvrc` when the GUI starts.

### `.mnvinfo`

Persistent session state across MNV invocations.  Implemented in
`src/mnvinfo.c`, controlled by the `FEAT_MNVINFO` guard:

```c
#ifdef FEAT_NORMAL
# define FEAT_MNVINFO
#endif
```

Stores:
- Command-line history.
- Search patterns.
- Named marks.
- Register contents.
- File marks (last cursor positions).
- Jump list.

---

## The `mparm_T` Struct

The main-parameter struct passed through all startup functions:

```c
static mparm_T params;
```

Key fields:

- `argc`, `argv` — raw command-line arguments.
- `want_full_screen` — TRUE by default.
- `use_debug_break_level` — debugger break level.
- `window_count` — number of windows requested by `-o`/`-O`.
- `clean` — `--clean` flag.
- `edit_type` — one of `EDIT_NONE`, `EDIT_FILE`, `EDIT_STDIN`, `EDIT_TAG`,
  `EDIT_QF`.

```c
#define EDIT_NONE   0
#define EDIT_FILE   1
#define EDIT_STDIN  2
#define EDIT_TAG    3
#define EDIT_QF     4
```

---

## Standard I/O: Editing from stdin

When MNV is invoked as part of a pipe:

```bash
echo "hello world" | mnv -
```

The `read_stdin()` function reads data from standard input into the first
buffer:

```c
static void read_stdin(void);
```

The `EDIT_STDIN` edit type is set in `command_line_scan()` when `-` appears as
a file argument.

---

## Example Workflows

### Quick edit from command line

```bash
mnv +42 src/main.c        # Open at line 42
mnv -c 'set nu' file.txt  # Open with line numbers
mnv -d file1.c file2.c    # Diff two files
mnv -R /var/log/syslog    # View log read-only
```

### Batch processing

```bash
mnv -es '+%s/foo/bar/g' '+wq' file.txt
echo ":%s/old/new/g" | mnv -s file.txt
```

### Remote editing (client-server)

```bash
mnv --servername MYSERVER file.c &
mnv --servername MYSERVER --remote file2.c
mnv --servername MYSERVER --remote-send ':qa!<CR>'
```

### Startup profiling

```bash
mnv --startuptime startup.log file.c
```

---

*This document describes MNV 10.0 as of build 287 (2026-04-03).*
