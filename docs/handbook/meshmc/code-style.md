# Code Style

## Overview

MeshMC enforces consistent code style using `.clang-format` and `.clang-tidy` configurations. The project uses C++23 with Qt6 patterns. Code formatting is checked automatically via lefthook git hooks.

## Clang-Format Configuration

The `.clang-format` file defines the formatting rules:

```yaml
BasedOnStyle: LLVM
ColumnLimit: 80
IndentWidth: 4
TabWidth: 4
UseTab: Always
ContinuationIndentWidth: 4
BreakBeforeBraces: Linux
PointerAlignment: Left
SortIncludes: false
AlignAfterOpenBracket: Align
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false
AlignOperands: Align
AllowShortBlocksOnASingleLine: Never
AllowShortCaseLabelsOnASingleLine: false
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
IndentCaseLabels: true
IndentPPDirectives: None
NamespaceIndentation: All
SpaceBeforeParens: ControlStatements
SpacesInParentheses: false
```

### Key Formatting Rules

| Rule | Value | Example |
|---|---|---|
| Indentation | Tabs (width 4) | `\tif (x)` |
| Column limit | 80 characters | — |
| Braces | Linux style | `if (x) {` on same line; function `{` on new line |
| Pointer alignment | Left | `int* ptr` not `int *ptr` |
| Include sorting | Disabled | Preserve manual grouping |
| Short blocks | Never on one line | Always use braces + newlines |
| Namespace indent | All | Contents indented inside namespaces |

### Running clang-format

```bash
# Format a single file
clang-format -i launcher/Application.cpp

# Format all source files
find launcher libraries -name '*.cpp' -o -name '*.h' | xargs clang-format -i

# Check without modifying (CI mode)
clang-format --dry-run --Werror launcher/Application.cpp
```

## Clang-Tidy Configuration

The `.clang-tidy` file enables static analysis checks:

```yaml
Checks: >
  -*,
  bugprone-*,
  clang-analyzer-*,
  performance-*,
  portability-*,
  readability-*,
  -readability-function-cognitive-complexity,
  -readability-magic-numbers,
  -readability-identifier-length,
  -readability-convert-member-functions-to-static,
  modernize-*,
  -modernize-use-trailing-return-type
HeaderFilterRegex: '^(launcher|libraries|updater|buildconfig)/'
FormatStyle: file
CheckOptions:
  - key: readability-function-size.LineThreshold
    value: '200'
  - key: readability-function-size.StatementThreshold
    value: '120'
```

### Enabled Check Categories

| Category | Scope |
|---|---|
| `bugprone-*` | Bug-prone patterns (narrowing conversions, incorrect moves, etc.) |
| `clang-analyzer-*` | Clang Static Analyzer checks (null deref, memory leaks, etc.) |
| `performance-*` | Performance issues (unnecessary copies, move semantics) |
| `portability-*` | Portability concerns across compilers/platforms |
| `readability-*` | Code readability (naming, braces, simplification) |
| `modernize-*` | C++ modernization (auto, range-for, nullptr, etc.) |

### Disabled Checks

| Check | Reason |
|---|---|
| `readability-function-cognitive-complexity` | Qt UI code often has inherently complex functions |
| `readability-magic-numbers` | Not enforced due to frequent use in UI layout code |
| `readability-identifier-length` | Short names like `i`, `dl`, `it` are acceptable |
| `readability-convert-member-functions-to-static` | Conflicts with Qt's signal/slot pattern |
| `modernize-use-trailing-return-type` | Traditional return types preferred |

### Function Size Limits

- Maximum **200 lines** per function
- Maximum **120 statements** per function

### Running clang-tidy

```bash
# Run on a single file (requires compile_commands.json)
clang-tidy launcher/Application.cpp

# Run with fixes applied
clang-tidy --fix launcher/Application.cpp

# Generate compile_commands.json
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ...
```

## C++ Standard

MeshMC uses **C++23**:

```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### C++23 Features Used

- `std::expected` — error handling without exceptions
- `std::format` — string formatting (where supported)
- `std::ranges` — range operations
- Structured bindings
- `if constexpr`
- `std::optional`, `std::variant`
- Designated initializers
- Three-way comparison (`<=>`)

## Qt Patterns

### Q_OBJECT Macro

All QObject subclasses must include the `Q_OBJECT` macro:

```cpp
class MyClass : public QObject
{
    Q_OBJECT
public:
    explicit MyClass(QObject* parent = nullptr);
    // ...
};
```

### Signals and Slots

Use the new-style signal/slot syntax:

```cpp
// Preferred: compile-time checked
connect(sender, &Sender::signalName, receiver, &Receiver::slotName);

// Lambda connections
connect(sender, &Sender::signalName, this, [this]() {
    // Handle signal
});
```

Avoid the old `SIGNAL()`/`SLOT()` macro syntax.

### shared_qobject_ptr

MeshMC uses `shared_qobject_ptr<T>` for shared ownership of QObjects:

```cpp
// Instead of raw pointers or QSharedPointer
shared_qobject_ptr<NetJob> job(new NetJob("Download", network));
```

This is a custom smart pointer that integrates with Qt's parent-child ownership.

### Memory Management

- Use Qt's parent-child ownership for UI objects
- Use `shared_qobject_ptr<T>` for task objects shared across modules
- Use `std::shared_ptr<T>` for non-QObject types
- Use `std::unique_ptr<T>` for exclusive ownership
- Avoid raw `new`/`delete` outside Qt's parent-child system

## Naming Conventions

### Classes

```cpp
class MinecraftInstance;     // PascalCase
class BaseVersionList;       // PascalCase
class NetJob;               // PascalCase
```

### Member Variables

```cpp
class MyClass {
    int m_count;             // m_ prefix for member variables
    QString m_name;          // m_ prefix
    QList<Item> m_items;     // m_ prefix

    static int s_instance;   // s_ prefix for static members
};
```

### Methods

```cpp
void executeTask();          // camelCase
QString profileName() const; // camelCase, const for getters
void setProfileName(const QString& name); // set prefix for setters
```

### Signals and Slots

```cpp
signals:
    void taskStarted();          // camelCase, past tense for events
    void progressChanged(int);   // camelCase
    void downloadFinished();     // camelCase

public slots:
    void onButtonClicked();      // on prefix for UI slots (optional)
    void handleError(QString);   // handle prefix (optional)
```

### Type Aliases

```cpp
using Ptr = std::shared_ptr<MyClass>;          // Ptr alias convention
using WeakPtr = std::weak_ptr<MyClass>;
```

### Enums

```cpp
enum class AccountState {   // PascalCase, scoped (enum class)
    Unchecked,              // PascalCase values
    Online,
    Offline,
    Errored,
};
```

### Files

```
Application.h / Application.cpp     // PascalCase, matching class name
MinecraftInstance.h                  // PascalCase
ui-shared.h / ui-shared.c           // kebab-case for C files (cgit-inherited)
```

## Header Guards

Use `#pragma once`:

```cpp
#pragma once

#include <QString>
// ...
```

## Include Order

Includes are grouped (but not auto-sorted):

```cpp
// 1. Corresponding header
#include "MyClass.h"

// 2. Project headers
#include "Application.h"
#include "settings/SettingsObject.h"

// 3. Qt headers
#include <QObject>
#include <QString>

// 4. Standard library
#include <memory>
#include <vector>
```

## Lefthook Integration

Git hooks are managed via `lefthook.yml`:

```yaml
pre-commit:
  commands:
    clang-format:
      glob: "*.{cpp,h}"
      run: clang-format --dry-run --Werror {staged_files}
```

Install hooks after bootstrapping:
```bash
lefthook install
```
