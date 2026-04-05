# Network Layer

## Overview

MeshMC's network layer provides a managed download system supporting parallel downloads, caching, integrity validation, and progress tracking. Built on Qt's `QNetworkAccessManager`, it centralizes all HTTP operations through the `NetJob` and `Download` classes.

## Architecture

### Key Classes

| Class | File | Purpose |
|---|---|---|
| `NetAction` | `net/NetAction.h` | Abstract network operation base |
| `Download` | `net/Download.{h,cpp}` | HTTP GET download |
| `NetJob` | `net/NetJob.{h,cpp}` | Parallel download manager |
| `Sink` | `net/Sink.h` | Abstract data receiver |
| `FileSink` | `net/FileSink.{h,cpp}` | Write to file |
| `ByteArraySink` | `net/ByteArraySink.{h,cpp}` | Write to memory |
| `MetaCacheSink` | `net/MetaCacheSink.{h,cpp}` | Write to file with caching |
| `Validator` | `net/Validator.h` | Abstract integrity checker |
| `ChecksumValidator` | `net/ChecksumValidator.{h,cpp}` | Hash-based validation |
| `HttpMetaCache` | `net/HttpMetaCache.{h,cpp}` | HTTP cache metadata |
| `MetaEntry` | `net/HttpMetaCache.h` | Cache entry (ETag, MD5) |
| `PasteUpload` | `net/PasteUpload.{h,cpp}` | Log paste upload |
| `Upload` | `net/Upload.{h,cpp}` | HTTP POST/PUT upload |

## NetAction

Base class for all network operations:

```cpp
class NetAction : public Task
{
    Q_OBJECT
public:
    using Ptr = shared_qobject_ptr<NetAction>;

    virtual void init() = 0;

public slots:
    void startAction(shared_qobject_ptr<QNetworkAccessManager> nam);

protected:
    virtual void executeTask() override = 0;

    // State
    QUrl m_url;
    QUrl m_redirectUrl;
    int m_redirectsRemaining = 6;
    shared_qobject_ptr<QNetworkAccessManager> m_network;
    std::unique_ptr<QNetworkReply> m_reply;
};
```

`NetAction` extends `Task`, inheriting progress tracking, state management, and signal notifications.

### JobStatus

Network operations report their status through the `Task` state machine:
- `NotStarted` → `Running` → `Succeeded` / `Failed` / `AbortedByUser`

## Download

The primary HTTP download class:

```cpp
class Download : public NetAction
{
    Q_OBJECT
public:
    // Factory methods
    static Download::Ptr makeCached(QUrl url, MetaEntryPtr entry, Options options = Option::NoOptions);
    static Download::Ptr makeByteArray(QUrl url, std::shared_ptr<QByteArray> output);
    static Download::Ptr makeFile(QUrl url, QString path);

    // Configuration
    Download* addValidator(Validator* v);
    Download* addHttpRedirectHandler(QUrl url);

private slots:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadError(QNetworkReply::NetworkError error);
    void sslErrors(const QList<QSslError>& errors);
    void downloadFinished();
    void downloadReadyRead();

protected:
    std::unique_ptr<Sink> m_sink;
    QList<Validator*> m_validators;
    Options m_options;
};
```

### Factory Methods

Three factory methods create downloads with appropriate sinks:

#### `makeCached(url, entry, options)`
Creates a download backed by `MetaCacheSink`:
- Checks `HttpMetaCache` for existing entry
- Sends `If-None-Match` with stored ETag
- On `304 Not Modified`, reuses cached file
- On `200 OK`, writes to file and updates cache
- Supports MD5 and SHA-1 validation

#### `makeByteArray(url, output)`
Creates a download backed by `ByteArraySink`:
- Data written to the provided `QByteArray`
- Used for small API responses, JSON data
- No disk I/O

#### `makeFile(url, path)`
Creates a download backed by `FileSink`:
- Writes directly to the specified file path
- No caching metadata
- Creates parent directories as needed

### Download Options

```cpp
enum class Option {
    NoOptions = 0,
    AcceptLocalFiles = 1,  // Allow file:// URLs
    MakeEternal = 2,       // Never expire from cache
};
```

### Redirect Handling

Downloads automatically follow HTTP redirects:
- Maximum of 6 redirects (configurable via `m_redirectsRemaining`)
- Both HTTP 301/302 and Qt's redirect attribute are handled
- Custom redirect handlers can be added for complex redirect chains

## NetJob

Manages parallel execution of multiple `NetAction` operations:

```cpp
class NetJob : public Task
{
    Q_OBJECT
public:
    using Ptr = shared_qobject_ptr<NetJob>;

    explicit NetJob(QString job_name, shared_qobject_ptr<QNetworkAccessManager> network);

    bool addNetAction(NetAction::Ptr action);

    // From Task
    void executeTask() override;
    bool abort() override;

    int size() const;

    // Progress aggregation
    auto totalProgress() const -> qint64;
    auto totalSize() const -> qint64;

signals:
    void failed(QString reason);

private slots:
    void partProgress(int index, qint64 bytesReceived, qint64 bytesTotal);
    void partSucceeded(int index);
    void partFailed(int index);
    void partAborted(int index);

private:
    struct Part {
        NetAction::Ptr download;
        qint64 current_progress = 0;
        qint64 total_progress = 1;
        int failures = 0;
    };

    QList<Part> m_parts;
    QQueue<int> m_todo;
    QSet<int> m_doing;
    QSet<int> m_done;
    QSet<int> m_failed;

    shared_qobject_ptr<QNetworkAccessManager> m_network;
    int m_maxConcurrent = 6;
};
```

### Execution Model

1. All `NetAction` items are queued in `m_todo`
2. Up to `m_maxConcurrent` (default 6) downloads run simultaneously
3. As downloads complete, new ones are started from the queue
4. Progress is aggregated across all downloads
5. On failure, items are retried up to 3 times before the job fails
6. The entire job succeeds only when all parts succeed

### Progress Tracking

`NetJob` aggregates progress from all parts:

```cpp
void NetJob::partProgress(int index, qint64 bytesReceived, qint64 bytesTotal)
{
    m_parts[index].current_progress = bytesReceived;
    m_parts[index].total_progress = bytesTotal;

    qint64 current = 0, total = 0;
    for (auto& part : m_parts) {
        current += part.current_progress;
        total += part.total_progress;
    }
    setProgress(current, total);
}
```

This emits `Task::progress(qint64, qint64)` which the UI connects to for progress bars.

## Sink System

Sinks receive downloaded data and write it to the appropriate destination:

### Sink (Abstract Base)

```cpp
class Sink
{
public:
    virtual ~Sink() = default;

    virtual auto init(QNetworkRequest& request) -> Task::State = 0;
    virtual auto write(const QByteArray& data) -> Task::State = 0;
    virtual auto abort() -> Task::State = 0;
    virtual auto finalize(QNetworkReply& reply) -> Task::State = 0;

    void addValidator(Validator* v);

protected:
    bool finalizeAllValidators(QNetworkReply& reply);
    QList<Validator*> m_validators;
};
```

### FileSink

```cpp
class FileSink : public Sink
{
public:
    explicit FileSink(const QString& filename);

    auto init(QNetworkRequest& request) -> Task::State override;
    auto write(const QByteArray& data) -> Task::State override;
    auto abort() -> Task::State override;
    auto finalize(QNetworkReply& reply) -> Task::State override;

private:
    QString m_filename;
    std::unique_ptr<QSaveFile> m_output;  // Atomic write
};
```

Uses `QSaveFile` for atomic writes — the file is written to a temporary location and renamed on `finalize()`, preventing partial files on failure.

### ByteArraySink

```cpp
class ByteArraySink : public Sink
{
public:
    explicit ByteArraySink(std::shared_ptr<QByteArray> output);

    auto init(QNetworkRequest& request) -> Task::State override;
    auto write(const QByteArray& data) -> Task::State override;
    auto abort() -> Task::State override;
    auto finalize(QNetworkReply& reply) -> Task::State override;

private:
    std::shared_ptr<QByteArray> m_output;
};
```

### MetaCacheSink

```cpp
class MetaCacheSink : public FileSink
{
public:
    MetaCacheSink(MetaEntryPtr entry, const QString& filename);

    auto init(QNetworkRequest& request) -> Task::State override;
    auto finalize(QNetworkReply& reply) -> Task::State override;

    bool hasLocalData();

private:
    MetaEntryPtr m_entry;
};
```

`MetaCacheSink` extends `FileSink` with:
- Setting `If-None-Match` header from cached ETag
- On `304 Not Modified`, short-circuits to success
- On `200 OK`, updates `MetaEntry` with new ETag, MD5, timestamp

## Validation System

### Validator (Abstract)

```cpp
class Validator
{
public:
    virtual ~Validator() = default;
    virtual bool init(QNetworkRequest& request) = 0;
    virtual bool write(const QByteArray& data) = 0;
    virtual bool validate(QNetworkReply& reply) = 0;
};
```

### ChecksumValidator

```cpp
class ChecksumValidator : public Validator
{
public:
    ChecksumValidator(QCryptographicHash::Algorithm algorithm,
                      QString expected = QString());

    bool init(QNetworkRequest& request) override;
    bool write(const QByteArray& data) override;
    bool validate(QNetworkReply& reply) override;

private:
    QCryptographicHash m_hash;
    QString m_expected;
};
```

Usage:
```cpp
auto dl = Download::makeFile(url, path);
dl->addValidator(new ChecksumValidator(QCryptographicHash::Sha1, expectedSha1));
```

The validator incrementally hashes data as it arrives via `write()`, then compares the final hash in `validate()`. Supported algorithms: MD5, SHA-1, SHA-256.

## HttpMetaCache

Persistent HTTP caching metadata:

```cpp
class HttpMetaCache : public QObject
{
    Q_OBJECT
public:
    HttpMetaCache(const QString& path = QString());
    ~HttpMetaCache();

    MetaEntryPtr resolveEntry(QString base, QString relative_path, QString expected_etag = QString());
    bool updateEntry(MetaEntryPtr stale_entry);
    bool evictEntry(MetaEntryPtr entry);
    void addBase(QString base, QString base_root);

    void Load();
    void Save();

private:
    // Base URL → local root directory mapping
    QMap<QString, QString> m_entries;

    // base/path → MetaEntry
    QMap<QString, QMap<QString, MetaEntryPtr>> m_entry_cache;

    QString m_index_file;
};
```

### MetaEntry

```cpp
class MetaEntry
{
public:
    QString basePath;
    QString relativePath;

    QString md5sum;
    QString etag;

    qint64 local_changed_timestamp = 0;
    qint64 remote_changed_timestamp = 0;

    bool stale = true;
    bool makeEternal = false;

    QString getFullPath();
};
```

### Cache Flow

```
1. resolveEntry("mojang", "versions/1.21.json")
       │
       ├── Lookup in m_entry_cache["mojang"]["versions/1.21.json"]
       │       ├── Found + !stale → return existing entry
       │       └── Found + stale  → return for re-download
       └── Not found → create new stale entry
       │
2. MetaCacheSink.init()
       ├── Set If-None-Match: <etag> on request
       │
3. HTTP Response
       ├── 304 Not Modified → mark entry not stale, return
       └── 200 OK → write file, update etag/md5/timestamps
       │
4. updateEntry() → save to cache index
```

### Cache Persistence

The cache index is stored as JSON in `metacache/metacache.json`:

```json
{
    "formatVersion": 2,
    "entries": {
        "mojang": {
            "versions/1.21.json": {
                "md5sum": "abc123...",
                "etag": "\"xyz789\"",
                "local_changed_timestamp": 1700000000,
                "remote_changed_timestamp": 1699000000
            }
        }
    }
}
```

## Shared Network Manager

`Application` creates a single `QNetworkAccessManager` shared across all operations:

```cpp
// Application.h
shared_qobject_ptr<QNetworkAccessManager> network();

// Application.cpp
m_network = new QNetworkAccessManager();
```

All `NetJob` instances receive this shared manager. Proxy settings from the settings system are applied to the manager at startup and when changed.

### Proxy Configuration

```cpp
// From global settings
switch (proxyType) {
    case "None":
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        break;
    case "SOCKS5":
        proxy.setType(QNetworkProxy::Socks5Proxy);
        // ...
        break;
    case "HTTP":
        proxy.setType(QNetworkProxy::HttpProxy);
        // ...
        break;
}
```

## Upload System

### PasteUpload

Uploads log content to paste services:

```cpp
class PasteUpload : public Task
{
    Q_OBJECT
public:
    PasteUpload(QWidget* window, QString text, QString url);

    QString pasteLink();

protected:
    void executeTask() override;

private slots:
    void downloadFinished();
    void downloadError(QNetworkReply::NetworkError error);

private:
    QByteArray m_text;
    QString m_pasteLink;
    QString m_pasteURL;
    shared_qobject_ptr<QNetworkReply> m_reply;
};
```

Used by the log page to share instance logs. The paste URL is configurable in settings.

## Common Download Patterns

### Downloading Minecraft Version Manifest

```cpp
auto job = new NetJob("Version List", APPLICATION->network());
auto entry = APPLICATION->metacache()->resolveEntry("mojang", "version_manifest_v2.json");
auto dl = Download::makeCached(
    QUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"),
    entry
);
job->addNetAction(dl);
connect(job, &NetJob::succeeded, this, &VersionList::loadListFromFile);
job->start();
```

### Downloading Game Libraries

```cpp
auto job = new NetJob("Libraries", APPLICATION->network());
for (auto& lib : libraries) {
    auto dl = Download::makeFile(lib.url(), lib.storagePath());
    dl->addValidator(new ChecksumValidator(QCryptographicHash::Sha1, lib.sha1()));
    job->addNetAction(dl);
}
connect(job, &NetJob::progress, this, &LaunchStep::setProgress);
connect(job, &NetJob::succeeded, this, &LaunchStep::emitSucceeded);
connect(job, &NetJob::failed, this, &LaunchStep::emitFailed);
job->start();
```

### Downloading to Memory

```cpp
auto output = std::make_shared<QByteArray>();
auto dl = Download::makeByteArray(
    QUrl("https://api.example.com/data.json"),
    output
);
auto job = new NetJob("API Request", APPLICATION->network());
job->addNetAction(dl);
connect(job, &NetJob::succeeded, [output]() {
    auto doc = QJsonDocument::fromJson(*output);
    // Process response
});
job->start();
```
