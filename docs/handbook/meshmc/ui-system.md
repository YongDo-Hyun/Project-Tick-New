# UI System

## Overview

MeshMC's user interface is built on Qt6 Widgets. The UI follows a page-based navigation model, with a main window hosting instance management, and dialog-based workflows for settings, instance configuration, and account management.

## Architecture

### Key Classes

| Class | File | Purpose |
|---|---|---|
| `MainWindow` | `ui/MainWindow.{h,cpp}` | Primary application window |
| `InstanceWindow` | `ui/InstanceWindow.{h,cpp}` | Per-instance console window |
| `PageDialog` | `ui/dialogs/PageDialog.{h,cpp}` | Dialog for page navigation |
| `PageContainer` | `ui/pages/PageContainer.{h,cpp}` | Page management container |
| `BasePage` | `ui/pages/BasePage.h` | Abstract page interface |
| `InstanceView` | `ui/instanceview/InstanceView.{h,cpp}` | Grid/list instance view |
| `WizardDialog` | `ui/setupwizard/SetupWizard.{h,cpp}` | First-run setup wizard |

## MainWindow

The central window of the application:

```cpp
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

    // Instance management
    void setSelectedInstanceById(const QString& id);

    // Window state
    void checkInstancePathForProblems();

public slots:
    void instanceActivated(QModelIndex);
    void instanceChanged(const QModelIndex& current, const QModelIndex& previous);

    // Toolbar actions
    void on_actionAddInstance_triggered();
    void on_actionViewSelectedInstFolder_triggered();
    void on_actionViewSelectedMCFolder_triggered();
    void on_actionCopyInstance_triggered();
    void on_actionDeleteInstance_triggered();
    void on_actionExportInstance_triggered();
    void on_actionLaunchInstance_triggered();
    void on_actionLaunchInstanceOffline_triggered();
    void on_actionKillInstance_triggered();

    // Global actions
    void on_actionSettings_triggered();
    void on_actionManageAccounts_triggered();
    void on_actionAbout_triggered();
    void on_actionCAT_triggered();

    // Group management
    void on_actionRenameGroup_triggered();
    void on_actionDeleteGroup_triggered();

private:
    // UI components
    Ui::MainWindow* ui;
    InstanceView* view;
    QToolBar* instanceToolbar;
    QToolBar* newsToolbar;
    StatusLabel* m_statusLeft;
    StatusLabel* m_statusCenter;
    QMenu* accountMenu;
    QMenu* skinMenu;

    // Models
    InstanceProxyModel* proxymodel;
    GroupView* groupView;

    // State
    MinecraftAccountPtr m_selectedAccount;
    BaseInstance::Ptr m_selectedInstance;
};
```

### Toolbar Layout

The main window has two toolbars:

**Main Toolbar:**
| Action | Shortcut | Description |
|---|---|---|
| Add Instance | Ctrl+N | Opens NewInstanceDialog |
| Folders | — | Dropdown: instances, central mods, skins |
| Settings | — | Opens global settings dialog |
| Help | — | About, bug report, wiki, Discord |
| Update | — | Check for updates (when available) |

**Instance Toolbar** (shown when an instance is selected):
| Action | Description |
|---|---|
| Launch | Start the selected instance |
| Launch Offline | Start without authentication |
| Kill | Force-stop a running instance |
| Edit Instance | Open instance settings |
| Edit Mods | Open mod management page |
| View Folder | Open instance folder in file manager |
| Copy Instance | Duplicate the instance |
| Delete | Delete the instance |
| Export | Export instance as zip/mrpack |

### Account Selector

The account selector is a dropdown menu in the toolbar:
- Shows current default account name + skin icon
- Lists all accounts with switch option
- "Manage Accounts..." opens global account settings
- "No Default Account" option

### Instance View

`InstanceView` displays instances in a grid with grouping:

```cpp
class InstanceView : public QAbstractItemView
{
    Q_OBJECT
public:
    void setModel(QAbstractItemModel* model) override;

    // View modes
    void setIconSize(QSize size);

signals:
    void droppedURLs(QList<QUrl> urls);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};
```

Features:
- Custom grid layout with grouped headings
- Drag-and-drop support for instance import (zip/mrpack files)
- Custom icon rendering with play-time overlay
- Context menu with all instance actions
- Group collapse/expand

## Page System

### BasePage Interface

All settings and configuration pages implement `BasePage`:

```cpp
class BasePage : public QWidget
{
public:
    virtual ~BasePage() {}

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual QIcon icon() const = 0;
    virtual QString helpPage() const { return QString(); }
    virtual bool shouldDisplay() const { return true; }

    virtual void opened() {}
    virtual void closed() {}
    virtual bool apply() { return true; }

    virtual bool isOpened() const { return m_isOpened; }

protected:
    bool m_isOpened = false;
};
```

### PageContainer

Manages page navigation with a tree-based sidebar:

```cpp
class PageContainer : public QWidget
{
    Q_OBJECT
public:
    PageContainer(BasePageProvider* pages, QString defaultId = QString(),
                  QWidget* parent = nullptr);

    void setPageProvider(BasePageProvider* pages);
    BasePage* getPage(const QString& id);
    const QList<BasePage*>& getPages() const;

    void selectPage(const QString& id);

private:
    QTreeView* m_pageList;
    QStackedWidget* m_pageStack;
    QList<BasePage*> m_pages;
};
```

### PageDialog

Wraps a `PageContainer` in a dialog:

```cpp
class PageDialog : public QDialog
{
    Q_OBJECT
public:
    PageDialog(BasePageProvider* pages, QString title = QString(),
               QWidget* parent = nullptr);

    void accept() override;

private:
    PageContainer* m_container;
};
```

Used for both global settings and instance settings.

## Global Settings Pages

### MeshMCPage
- Update channel selection (stable/beta)
- Auto-update toggle
- Analytics opt-in/out
- Instance sort mode

### MinecraftPage
- Default window dimensions (width × height)
- Launch maximized toggle
- Console visibility settings
- Performance settings

### JavaPage
- Java path (manual or auto-detect)
- Memory allocation (min/max heap)
- JVM arguments
- Java compatibility warnings toggle
- "Auto-detect" button triggers `JavaUtils::FindJavaPaths()`
- "Test" button verifies selected Java installation

### LanguagePage
- Language selector (populated from `translations/`)
- Live preview of selected language

### ProxyPage
- Proxy type: None / SOCKS5 / HTTP
- Proxy address, port
- Authentication (username/password)

### ExternalToolsPage
- Profiler paths (JProfiler, JVisualVM, MCEdit)
- Custom editor path

### AccountListPage
- Account list with status indicators
- Add/Remove/Set Default/Refresh buttons
- Skin preview panel

### PasteEEPage
- Paste service URL configuration

### CustomCommandsPage
- Pre-launch command
- Wrapper command
- Post-exit command

### AppearancePage
- Application theme selector
- Icon theme selector
- Cat style (cat/kitteh)

## Instance Pages

When editing an instance (`PageDialog` with instance pages):

### VersionPage
- Component list (Minecraft version, mod loaders)
- Add/Remove/Change version of components
- Component ordering (move up/down)

### ModFolderPage
- Mod list with enable/disable toggles
- Add from file / Add from CurseForge / Add from Modrinth
- Remove selected mods
- View mod details

### LogPage
- Live log output from running instance
- Search/filter log content
- Upload to paste service
- Copy to clipboard
- Auto-scroll toggle

### InstanceSettingsPage
- Java override (checkbox + path)
- Memory override (checkbox + min/max)
- JVM args override
- Window size override
- Console settings override
- Custom commands override

### WorldListPage
- List of worlds with metadata
- Backup/Restore/Delete operations
- Datapacks submenu

### ScreenshotsPage
- Grid view of instance screenshots
- Open in file manager
- Delete selected
- Upload to Imgur

### ResourcePackPage / TexturePackPage
- Resource/texture pack list
- Add/Remove
- Enable/disable

### NotesPage
- Free-text notes field per instance

### ServersPage
- Server list for the instance
- Add/Edit/Remove servers

## Dialogs

### NewInstanceDialog

Multi-tab dialog for creating new instances:

| Tab | Source |
|---|---|
| Vanilla | Select Minecraft version |
| Import | Import from zip/mrpack URL or file |
| CurseForge | Browse CurseForge modpacks |
| Modrinth | Browse Modrinth modpacks |
| ATLauncher | ATLauncher pack listing |
| FTB | FTB pack listing |
| Technic | Technic Platform packs |

Each tab provides search, filtering, and version selection. On confirmation, the appropriate import/creation task is started.

### CopyInstanceDialog

Options for duplicating an instance:
- New name
- New group assignment
- Copy saves (toggle)
- Keep play time (toggle)

### ExportInstanceDialog

Export format selection and file exclusion:
- Format: MeshMC zip / Modrinth mrpack / CurseForge manifest
- File tree with checkboxes for selective export
- Exclusion filter patterns

### MSALoginDialog

Microsoft account login flow (see Account Management):
- Displays login URL with copy button
- Shows authentication progress
- Error display on failure

### ProfileSelectDialog

Account selection when no default is set:
- Account list with radio selection
- "Use selected" / Cancel

### ProfileSetupDialog

First-time Minecraft profile setup:
- Username entry
- Validates username availability
- Creates profile via Mojang API

### SkinUploadDialog

Skin upload interface:
- File picker for skin PNG
- Variant selector (classic/slim)
- Preview

### AboutDialog

Application information:
- Version, build info, Qt version
- License (GPL-3.0-or-later)
- Credits and contributors

### UpdateDialog

Update notification:
- Version comparison
- Changelog display
- Update / Skip buttons

## Setup Wizard

`SetupWizard` runs on first launch or when required:

```cpp
class SetupWizard : public QWizard
{
    Q_OBJECT
public:
    SetupWizard(QWidget* parent = nullptr);

    // Pages added conditionally
    void addLanguagePage();
    void addJavaPage();
    void addAnalyticsPage();
    void addPasteEEPage();
};
```

Pages are added based on what needs configuration:
- **LanguagePage** — if no language is set
- **JavaPage** — if no valid Java is detected
- **AnalyticsPage** — if analytics consent is not recorded
- **PasteEEPage** — if paste service is not configured

## Widget Components

### StatusLabel

Custom label widget for the status bar:
- Elides long text
- Supports click-to-copy
- Used for status area in MainWindow

### InstanceDelegate

Custom item delegate for `InstanceView`:
- Renders instance icon, name, and status
- Shows play time overlay
- Running state indicator

### ProgressWidget

Shared progress display widget:
- Progress bar with percentage
- Status text label
- Cancel button
- Used by download dialogs, import tasks, etc.

### IconPickerDialog

Instance icon selection:
- Built-in icon library
- Custom icon upload
- Icon theme integration

## Event Handling

### Instance Double-Click

```
MainWindow::instanceActivated(QModelIndex)
    │
    ├── If instance is running → open InstanceWindow
    └── If instance is stopped → launch instance
```

### Instance Launch

```
MainWindow::on_actionLaunchInstance_triggered()
    │
    ├── Resolve account (default or prompt with ProfileSelectDialog)
    ├── Create LaunchController
    │   ├── Set instance, account, main window
    │   └── connect succeeded/failed signals
    └── LaunchController::start()
```

### Drag-and-Drop Import

```
InstanceView::dropEvent(QDropEvent)
    │
    ├── Extract URLs from QMimeData
    ├── Filter for .zip, .mrpack files
    └── For each URL → open NewInstanceDialog with import tab pre-selected
```

## Window Management

`Application` manages window lifecycle:

```cpp
// Track open windows
QList<QWidget*> m_openWindows;

// Show instance window
void Application::showInstanceWindow(InstancePtr instance);

// Main window
MainWindow* Application::showMainWindow();
```

Multiple windows can be open simultaneously (main window + instance console windows). The application exits when all windows are closed.
