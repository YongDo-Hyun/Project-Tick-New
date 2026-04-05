# Account Management

## Overview

MeshMC supports Microsoft Account (MSA) authentication for Minecraft login. The authentication system implements the full MSA → Xbox Live → Minecraft authentication chain using Qt6's `QOAuth2AuthorizationCodeFlow` and the Katabasis OAuth2 library. Multiple accounts can be stored, managed, and switched between.

## Authentication Architecture

### Key Classes

| Class | File | Purpose |
|---|---|---|
| `MinecraftAccount` | `minecraft/auth/MinecraftAccount.{h,cpp}` | Account model |
| `AccountList` | `minecraft/auth/AccountList.{h,cpp}` | Account collection model |
| `AccountData` | `minecraft/auth/AccountData.{h,cpp}` | Token/profile data storage |
| `AccountTask` | `minecraft/auth/AccountTask.{h,cpp}` | Async auth task base |
| `AuthFlow` | `minecraft/auth/flows/AuthFlow.{h,cpp}` | Auth step pipeline |
| `MSAInteractive` | `minecraft/auth/flows/MSA.{h,cpp}` | Interactive MSA login |
| `MSASilent` | `minecraft/auth/flows/MSA.{h,cpp}` | Silent token refresh |
| `AuthStep` | `minecraft/auth/AuthStep.{h,cpp}` | Abstract auth pipeline step |
| `AuthSession` | `minecraft/auth/AuthSession.{h,cpp}` | Resolved session for launch |
| `AuthRequest` | `minecraft/auth/AuthRequest.{h,cpp}` | HTTP request helper |
| `Parsers` | `minecraft/auth/Parsers.{h,cpp}` | Response JSON parsers |
| `MSALoginDialog` | `ui/dialogs/MSALoginDialog.{h,cpp}` | Login UI dialog |

### Authentication Steps

| Step | File | Purpose |
|---|---|---|
| `MSAStep` | `steps/MSAStep.{h,cpp}` | OAuth2 browser-based login |
| `XboxUserStep` | `steps/XboxUserStep.{h,cpp}` | Exchange MSA token → Xbox User Token |
| `XboxAuthorizationStep` | `steps/XboxAuthorizationStep.{h,cpp}` | Exchange → XSTS Token |
| `XboxProfileStep` | `steps/XboxProfileStep.{h,cpp}` | Fetch Xbox profile |
| `MinecraftProfileStep` | `steps/MinecraftProfileStep.{h,cpp}` | Fetch Minecraft profile |
| `EntitlementsStep` | `steps/EntitlementsStep.{h,cpp}` | Verify game ownership |
| `GetSkinStep` | `steps/GetSkinStep.{h,cpp}` | Download player skin |
| `MeshMCLoginStep` | `steps/MeshMCLoginStep.{h,cpp}` | MeshMC-specific login |

## MSA Authentication Flow

### Interactive Login (First-Time)

The `MSAInteractive` flow is used when the user needs to sign in for the first time:

```
MSAStep(Login)
    │
    ├── QOAuth2AuthorizationCodeFlow configured with:
    │   ├── authorizationUrl: https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize
    │   ├── accessTokenUrl: https://login.microsoftonline.com/consumers/oauth2/v2.0/token
    │   ├── clientId: MeshMC_MICROSOFT_CLIENT_ID (from BuildConfig)
    │   └── scope: XboxLive.signin XboxLive.offline_access
    │
    ├── Opens browser for user authentication
    ├── QOAuthHttpServerReplyHandler listens on localhost
    ├── Receives authorization code callback
    └── Exchanges code for MSA tokens
    │
    ▼
XboxUserStep
    │
    ├── POST https://user.auth.xboxlive.com/user/authenticate
    ├── Body: { "Properties": { "AuthMethod": "RPS", "SiteName": "user.auth.xboxlive.com",
    │              "RpsTicket": "d=<MSA_ACCESS_TOKEN>" }, "RelyingParty": "http://auth.xboxlive.com",
    │              "TokenType": "JWT" }
    └── Returns: Xbox User Token + user hash
    │
    ▼
XboxAuthorizationStep
    │
    ├── POST https://xsts.auth.xboxlive.com/xsts/authorize
    ├── Body: { "Properties": { "SandboxId": "RETAIL",
    │              "UserTokens": ["<XBOX_USER_TOKEN>"] },
    │              "RelyingParty": "rp://api.minecraftservices.com/",
    │              "TokenType": "JWT" }
    └── Returns: XSTS Token + user hash
    │
    ▼
MinecraftProfileStep
    │
    ├── POST https://api.minecraftservices.com/authentication/login_with_xbox
    ├── Body: { "identityToken": "XBL3.0 x=<USERHASH>;<XSTS_TOKEN>" }
    ├── Returns: Minecraft access token
    │
    ├── GET https://api.minecraftservices.com/minecraft/profile
    └── Returns: player UUID, name, skin, capes
    │
    ▼
EntitlementsStep
    │
    ├── GET https://api.minecraftservices.com/entitlements/mcstore
    └── Verifies: ownsMinecraft, canPlayMinecraft
    │
    ▼
GetSkinStep
    │
    ├── Downloads skin texture from profile URL
    └── Stores skin data in AccountData
```

### Silent Refresh

The `MSASilent` flow is used for automatic token refresh after the initial login:

```
MSAStep(Refresh)
    │
    ├── Uses stored refresh token
    ├── QOAuth2AuthorizationCodeFlow::refreshAccessToken()
    └── Exchanges refresh token for new MSA tokens
    │
    ▼
(Same steps as Interactive: XboxUser → Xbox Authorization → MinecraftProfile → etc.)
```

## MSAStep Implementation

`MSAStep` uses Qt6's `QOAuth2AuthorizationCodeFlow`:

```cpp
class MSAStep : public AuthStep
{
    Q_OBJECT
public:
    enum Action { Refresh, Login };

    explicit MSAStep(AccountData* data, Action action);
    virtual ~MSAStep() noexcept;

    void perform() override;
    void rehydrate() override;
    QString describe() override;

private slots:
    void onGranted();
    void onRequestFailed(QAbstractOAuth::Error error);
    void onOpenBrowser(const QUrl& url);
};
```

For `Login` action:
- Creates `QOAuth2AuthorizationCodeFlow`
- Starts a `QOAuthHttpServerReplyHandler` on a random local port
- Opens the user's browser to Microsoft's login page
- Receives the callback and exchanges the code for tokens

For `Refresh` action:
- Uses the stored refresh token
- Calls `refreshAccessToken()` on the flow
- No browser interaction needed

## AuthFlow Pipeline

`AuthFlow` orchestrates the step sequence:

```cpp
class AuthFlow : public AccountTask
{
    Q_OBJECT
public:
    explicit AuthFlow(AccountData* data, QObject* parent = 0);

    void executeTask() override;

private slots:
    void stepFinished(AccountTaskState resultingState, QString message);

protected:
    void succeed();
    void nextStep();

    QList<AuthStep::Ptr> m_steps;
    AuthStep::Ptr m_currentStep;
};
```

Steps are executed sequentially. Each step calls `stepFinished()` when complete, which either advances to the next step or handles errors.

## AccountData

`AccountData` stores all authentication state for a single account:

```cpp
struct AccountData {
    // Serialization
    QJsonObject saveState() const;
    bool resumeStateFromV3(QJsonObject data);

    // Display
    QString accountDisplayString() const;   // Gamertag
    QString accessToken() const;             // Minecraft access token
    QString profileId() const;               // Minecraft UUID
    QString profileName() const;             // Minecraft username
    QString lastError() const;

    // Account type
    AccountType type = AccountType::MSA;

    // Token storage
    Katabasis::Token msaToken;              // Microsoft Account token
    Katabasis::Token userToken;             // Xbox User Token
    Katabasis::Token xboxApiToken;          // XSTS Token
    Katabasis::Token mojangservicesToken;   // Mojang services token
    Katabasis::Token yggdrasilToken;        // Minecraft access token

    // Profile data
    MinecraftProfile minecraftProfile;      // UUID, name, skin, capes
    MinecraftEntitlement minecraftEntitlement;  // Game ownership

    // Internal
    Katabasis::Validity validity_;
    QString internalId;                     // Runtime-only unique ID
};
```

### Token Stack

Each token is stored as a `Katabasis::Token`:
- `token` — the actual token string
- `refresh_token` — token for refresh operations
- `validity` — `Katabasis::Validity::None`, `Assumed`, or `Certain`
- `extra` — additional token metadata

### MinecraftProfile

```cpp
struct MinecraftProfile {
    QString id;                   // UUID
    QString name;                 // Player name
    Skin skin;                    // Active skin
    QString currentCape;          // Active cape ID
    QMap<QString, Cape> capes;    // Available capes
    Katabasis::Validity validity;
};

struct Skin {
    QString id;
    QString url;
    QString variant;    // "classic" or "slim"
    QByteArray data;    // Texture data
};
```

## MinecraftAccount

`MinecraftAccount` is the QObject wrapper around `AccountData`:

```cpp
class MinecraftAccount : public QObject, public Usable
{
    Q_OBJECT
public:
    static MinecraftAccountPtr createBlankMSA();
    static MinecraftAccountPtr loadFromJsonV3(const QJsonObject& json);
    QJsonObject saveToJson() const;

    // Auth operations
    shared_qobject_ptr<AccountTask> loginMSA();
    shared_qobject_ptr<AccountTask> refresh();
    shared_qobject_ptr<AccountTask> currentTask();

    // Queries
    QString internalId() const;
    QString accountDisplayString() const;
    QString accessToken() const;
    QString profileId() const;
    QString profileName() const;
    bool isActive() const;

signals:
    void changed();
    void activityChanged(bool active);

public:
    AccountData data;
};
```

### Account States

```cpp
enum class AccountState {
    Unchecked,   // Not yet validated
    Offline,     // Tokens exist but not verified
    Working,     // Auth operation in progress
    Online,      // Valid tokens, verified
    Errored,     // Auth operation failed
    Expired,     // Tokens expired
    Gone         // Account removed from Microsoft
};
```

## AccountList

`AccountList` manages the collection of Microsoft accounts:

```cpp
class AccountList : public QAbstractListModel
{
    Q_OBJECT
public:
    enum VListColumns {
        NameColumn = 0,
        ProfileNameColumn,
        TypeColumn,
        StatusColumn,
        NUM_COLUMNS
    };

    const MinecraftAccountPtr at(int i) const;
    int count() const;

    void addAccount(const MinecraftAccountPtr account);
    void removeAccount(QModelIndex index);
    int findAccountByProfileId(const QString& profileId) const;
    MinecraftAccountPtr getAccountByProfileName(const QString& profileName) const;
    QStringList profileNames() const;

    void requestRefresh(QString accountId);
    void queueRefresh(QString accountId);

    void setListFilePath(QString path, bool autosave = false);
    bool loadList();
    bool saveList();

    MinecraftAccountPtr defaultAccount() const;
    void setDefaultAccount(MinecraftAccountPtr profileId);
    bool anyAccountIsValid();
};
```

### Persistence (`accounts.json`)

Accounts are serialized to `accounts.json` in the data directory:

```json
{
    "formatVersion": 3,
    "accounts": [
        {
            "type": "MSA",
            "msa": { ... token data ... },
            "utoken": { ... xbox user token ... },
            "xrp-main": { ... xsts token ... },
            "ygg": { ... minecraft access token ... },
            "profile": {
                "id": "player-uuid",
                "name": "PlayerName",
                "skin": { ... }
            },
            "entitlement": {
                "ownsMinecraft": true,
                "canPlayMinecraft": true
            }
        }
    ],
    "activeAccount": "player-uuid"
}
```

### Token Refresh Queue

`AccountList` maintains a refresh queue:
- `requestRefresh()` pushes an account to the front (high priority)
- `queueRefresh()` adds to the back (low priority)
- Refresh operations run sequentially to avoid rate limiting

## AuthSession

`AuthSession` is the resolved auth session passed to the launch system:

```cpp
struct AuthSession {
    QString player_name;        // Minecraft username
    QString uuid;               // Player UUID
    QString access_token;       // Minecraft access token
    QString session;            // Legacy session token (deprecated)
    QString user_type;          // "msa"

    // Used for censor filter
    QMap<QString, QString> sensitiveStrings();
};
```

## MSALoginDialog

The login dialog (`ui/dialogs/MSALoginDialog.h`) provides the user interface for Microsoft authentication:

```cpp
class MSALoginDialog : public QDialog {
    Q_OBJECT
};
```

The dialog:
1. Starts the `MSAInteractive` auth flow
2. Displays a message asking the user to sign in via their browser
3. Shows the authentication URL and a "Copy to Clipboard" button
4. Shows progress as auth steps complete
5. Closes on success or shows error on failure

## Account Management UI

### AccountListPage (`ui/pages/global/AccountListPage.h`)

The global settings page for account management:
- Lists all stored Microsoft accounts
- Shows account status (Online, Offline, Expired, etc.)
- "Add Microsoft Account" button → opens `MSALoginDialog`
- "Remove" button → deletes selected account
- "Set Default" button → sets the default account for launches
- "Refresh" button → triggers token refresh for selected account
- Displays skin preview for the selected account

### ProfileSelectDialog (`ui/dialogs/ProfileSelectDialog.h`)

Prompt dialog shown when launching without a default account:
- Lists available accounts
- User selects which account to use
- Returns the selected `MinecraftAccountPtr`

### ProfileSetupDialog (`ui/dialogs/ProfileSetupDialog.h`)

Dialog for initial profile setup when a Microsoft account doesn't yet have a Minecraft profile:
- Lets the user set their Minecraft username
- Submits the profile creation request to Mojang's API

## Skin Management

### SkinUploadDialog (`ui/dialogs/SkinUploadDialog.h`)

Upload dialog for changing the player's skin:
- Select a skin file (PNG)
- Choose variant (classic or slim)
- Upload to Mojang's API using the access token

### SkinUtils (`launcher/SkinUtils.h`)

Utility functions for skin image processing:
- Download skin textures
- Parse skin images for display

## Security Considerations

### Token Storage

Tokens are stored in `accounts.json` in the user's data directory:
- File is readable only by the user (standard OS file permissions)
- Tokens include MSA refresh tokens, Xbox tokens, and Minecraft access tokens
- **No encryption at rest** — the file relies on OS-level file permissions

### Censor Filter

The launch system automatically censors sensitive tokens from game log output:

```cpp
QMap<QString, QString> MinecraftInstance::createCensorFilterFromSession(AuthSessionPtr session);
```

This replaces token strings in log output with placeholder text to prevent accidental exposure.

### Client ID

The Microsoft OAuth2 client ID is embedded at build time:

```cmake
set(MeshMC_MICROSOFT_CLIENT_ID "3035382c-8f73-493a-b579-d182905c2864")
```

This is a public client ID registered with Azure Active Directory for the MeshMC application.
