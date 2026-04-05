# Notification System

## Table of Contents

- [Introduction](#introduction)
- [Notification Architecture](#notification-architecture)
- [Email Notification Implementation](#email-notification-implementation)
  - [Trigger Conditions](#trigger-conditions)
  - [Prerequisite Check](#prerequisite-check)
  - [Email Body Construction](#email-body-construction)
  - [Subject Line Format](#subject-line-format)
  - [Recipient Configuration](#recipient-configuration)
- [The MIRROR_NOTIFY Variable](#the-mirror_notify-variable)
  - [Enabling Notifications](#enabling-notifications)
  - [Disabling Notifications](#disabling-notifications)
  - [Multiple Recipients](#multiple-recipients)
- [Email Body Format](#email-body-format)
  - [Complete Email Example](#complete-email-example)
  - [Field-by-Field Breakdown](#field-by-field-breakdown)
- [Mail Command Integration](#mail-command-integration)
  - [The mail Command](#the-mail-command)
  - [Installing mail on Different Systems](#installing-mail-on-different-systems)
  - [Mail Transfer Agent Configuration](#mail-transfer-agent-configuration)
  - [Testing Email Delivery](#testing-email-delivery)
- [Failure Scenarios and Edge Cases](#failure-scenarios-and-edge-cases)
- [Extending the Notification System](#extending-the-notification-system)
  - [Adding Webhook Notifications](#adding-webhook-notifications)
  - [Adding Slack Integration](#adding-slack-integration)
  - [Adding Discord Integration](#adding-discord-integration)
  - [Adding Matrix Integration](#adding-matrix-integration)
  - [Adding SMS Notifications](#adding-sms-notifications)
- [Notification Suppression](#notification-suppression)
- [Monitoring and Alerting Integration](#monitoring-and-alerting-integration)

---

## Introduction

The Project-Tick `post-receive` hook (`hooks/post-receive`) includes an optional email notification system that alerts administrators when mirror push operations fail. The system is triggered only on failure and only when explicitly configured via the `MIRROR_NOTIFY` environment variable.

The notification system follows two guiding principles:
1. **Opt-in** — Notifications are disabled by default; no email is sent unless `MIRROR_NOTIFY` is set
2. **Graceful degradation** — If the `mail` command is not available, the notification is silently skipped

---

## Notification Architecture

The notification flow is:

```
Mirror push loop completes
        │
        ▼
Any FAILED_REMOTES?  ──No──►  Skip notification
        │
       Yes
        │
        ▼
MIRROR_NOTIFY set?  ──No──►  Skip notification
        │
       Yes
        │
        ▼
mail command available?  ──No──►  Skip notification
        │
       Yes
        │
        ▼
Construct email body from:
  - FAILED_REMOTES[]
  - $(pwd)             ← repository path
  - REFS[]             ← updated refs
  - $MIRROR_LOG        ← log file path
        │
        ▼
Send via: mail -s "[git-mirror] Push failure in <reponame>" "$MIRROR_NOTIFY"
```

Three gates must all pass before an email is sent:
1. At least one remote must have failed
2. `MIRROR_NOTIFY` must be set to a non-empty value
3. The `mail` command must be present on the system

---

## Email Notification Implementation

### Trigger Conditions

```bash
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_NOTIFY:-}" ]]; then
```

This compound condition checks:

| Expression | Test |
|------------|------|
| `${#FAILED_REMOTES[@]} -gt 0` | The `FAILED_REMOTES` array has at least one element |
| `-n "${MIRROR_NOTIFY:-}"` | The `MIRROR_NOTIFY` variable is non-empty |

The `${MIRROR_NOTIFY:-}` expansion with `:-` is critical under `set -u` — it prevents an "unbound variable" error if `MIRROR_NOTIFY` was never set. The `:-` substitutes an empty string for an unset variable, and then `-n` tests whether that string is non-empty.

The `&&` short-circuit operator means the `MIRROR_NOTIFY` check is only evaluated if there are failures. This is functionally irrelevant (both must pass), but reads naturally: "if there are failures AND notifications are configured."

### Prerequisite Check

```bash
if command -v mail &>/dev/null; then
```

Before attempting to send email, the script checks if the `mail` command exists:

| Component | Purpose |
|-----------|---------|
| `command -v mail` | Looks up `mail` in PATH; prints its path if found, exits non-zero if not |
| `&>/dev/null` | Suppresses both stdout and stderr (we only care about the exit code) |

This is the POSIX-compliant way to check for command availability, preferred over:
- `which mail` — not POSIX, may behave differently across systems
- `type mail` — bash-specific, prints extra output
- `hash mail` — bash-specific

If `mail` is not found, the entire notification block is skipped silently — no error, no warning. This allows deploying the hook on systems without `mail` configured.

### Email Body Construction

```bash
{
    echo "Mirror push failed for the following remotes:"
    printf '  - %s\n' "${FAILED_REMOTES[@]}"
    echo ""
    echo "Repository: $(pwd)"
    echo "Refs updated:"
    printf '  %s\n' "${REFS[@]}"
    echo ""
    echo "Check log: $MIRROR_LOG"
} | mail -s "[git-mirror] Push failure in $(basename "$(pwd)")" "$MIRROR_NOTIFY"
```

The `{ ... }` command group constructs the email body as a multi-line string. This group acts as a single compound command whose combined stdout is piped to `mail`.

**`printf '  - %s\n' "${FAILED_REMOTES[@]}"`** — Iterates over each element of the `FAILED_REMOTES` array, printing each as a bulleted list item. Using `printf` with an array is a bash idiom: the format string is applied to each argument in turn.

For example, if `FAILED_REMOTES=(sourceforge codeberg)`, the output is:
```
  - sourceforge
  - codeberg
```

**`$(pwd)`** — Expands to the current working directory. In a bare repository hook, this is the bare repository path (e.g., `/srv/git/project-tick.git`).

**`printf '  %s\n' "${REFS[@]}"`** — Lists all refs that were updated in this push, providing context about what triggered the mirror.

**`$MIRROR_LOG`** — Points the reader to the log file for detailed push output and error messages.

### Subject Line Format

```bash
mail -s "[git-mirror] Push failure in $(basename "$(pwd)")" "$MIRROR_NOTIFY"
```

The subject line follows the pattern:

```
[git-mirror] Push failure in <repository-directory-name>
```

**`$(basename "$(pwd)")`** — Extracts just the directory name from the full path:
- Input: `/srv/git/project-tick.git`
- Output: `project-tick.git`

The `[git-mirror]` prefix allows email filters to route or prioritize these notifications:

```
# Example email filter rule
Subject contains "[git-mirror]" → Move to "Git Alerts" folder
```

### Recipient Configuration

The recipient is specified by the `MIRROR_NOTIFY` environment variable, passed as the final argument to `mail`:

```bash
mail -s "subject" "$MIRROR_NOTIFY"
```

The variable is quoted (`"$MIRROR_NOTIFY"`) to handle email addresses that might contain special characters (though standard email addresses typically don't).

---

## The MIRROR_NOTIFY Variable

### Enabling Notifications

Set the variable in the environment of the process running the git daemon:

```bash
# In systemd service file
Environment=MIRROR_NOTIFY=admin@project-tick.org

# In shell profile
export MIRROR_NOTIFY=admin@project-tick.org

# In a wrapper script
MIRROR_NOTIFY=admin@project-tick.org exec hooks/post-receive
```

### Disabling Notifications

Notifications are disabled by default. To explicitly disable:

```bash
# Unset the variable
unset MIRROR_NOTIFY

# Or set to empty
export MIRROR_NOTIFY=""
```

### Multiple Recipients

The `mail` command typically supports multiple recipients as a comma-separated list:

```bash
export MIRROR_NOTIFY="admin@project-tick.org,ops@project-tick.org"
```

Or as space-separated arguments (behavior depends on the MTA):

```bash
export MIRROR_NOTIFY="admin@project-tick.org ops@project-tick.org"
```

For reliable multi-recipient support, modify the script to loop over recipients:

```bash
for addr in $MIRROR_NOTIFY; do
    { ... } | mail -s "subject" "$addr"
done
```

---

## Email Body Format

### Complete Email Example

```
From: git@server.project-tick.org
To: admin@project-tick.org
Subject: [git-mirror] Push failure in project-tick.git

Mirror push failed for the following remotes:
  - sourceforge
  - codeberg

Repository: /srv/git/project-tick.git
Refs updated:
  refs/heads/main
  refs/tags/v2.1.0

Check log: /var/log/git-mirror.log
```

### Field-by-Field Breakdown

| Field | Source | Example |
|-------|--------|---------|
| Failed remotes list | `"${FAILED_REMOTES[@]}"` | `sourceforge`, `codeberg` |
| Repository path | `$(pwd)` | `/srv/git/project-tick.git` |
| Updated refs | `"${REFS[@]}"` | `refs/heads/main`, `refs/tags/v2.1.0` |
| Log file path | `$MIRROR_LOG` | `/var/log/git-mirror.log` |
| Subject repo name | `$(basename "$(pwd)")` | `project-tick.git` |

The email body provides enough context for an administrator to:
1. Identify which mirrors are out of sync (failed remotes)
2. Locate the repository to investigate (repository path)
3. Understand what changed (updated refs)
4. Access detailed error output (log file path)

---

## Mail Command Integration

### The mail Command

The hook uses the `mail` command (also known as `mailx`), a standard Unix mail user agent. It reads the message body from stdin and sends it to the specified recipient via the system's mail transfer agent (MTA).

```bash
echo "body" | mail -s "subject" recipient@example.com
```

### Installing mail on Different Systems

| System | Package | Command |
|--------|---------|---------|
| Debian/Ubuntu | `sudo apt install mailutils` | `mail` |
| RHEL/CentOS | `sudo yum install mailx` | `mail` |
| Fedora | `sudo dnf install mailx` | `mail` |
| Arch Linux | `sudo pacman -S s-nail` | `mail` |
| Alpine | `apk add mailx` | `mail` |
| NixOS | `nix-env -iA nixpkgs.mailutils` | `mail` |
| macOS | Pre-installed (or `brew install mailutils`) | `mail` |

### Mail Transfer Agent Configuration

The `mail` command hands off the message to a local MTA. Common MTAs include:

| MTA | Package | Use Case |
|-----|---------|----------|
| Postfix | `postfix` | Full-featured, most common |
| Exim | `exim4` | Flexible, Debian default |
| msmtp | `msmtp` | Lightweight relay to external SMTP |
| ssmtp | `ssmtp` | Minimal relay (deprecated) |
| OpenSMTPD | `opensmtpd` | Simple, secure |

For a server that only needs to send outbound email (no receiving), `msmtp` is the simplest option:

```bash
# /etc/msmtprc
account default
host smtp.example.com
port 587
auth on
user notifications@project-tick.org
password APP_PASSWORD
tls on
from git-mirror@project-tick.org
```

### Testing Email Delivery

```bash
# Test basic mail delivery
echo "Test message from git-mirror" | mail -s "Test" admin@project-tick.org

# Check mail queue
mailq

# Check mail log
sudo tail /var/log/mail.log
```

---

## Failure Scenarios and Edge Cases

| Scenario | Behavior | User Impact |
|----------|----------|-------------|
| `MIRROR_NOTIFY` not set | Notification block skipped entirely | None |
| `MIRROR_NOTIFY` set to empty string | `-n` test fails; notification skipped | None |
| `mail` command not found | `command -v mail` fails; notification skipped | None |
| MTA not configured | `mail` command may succeed but message is undeliverable | Email queued or bounced locally |
| MTA fails to send | `mail` exits non-zero; under `set -e`... | See note below |
| Invalid email address | MTA accepts the message but it bounces later | Bounce email to local mailbox |
| All remotes succeed | `${#FAILED_REMOTES[@]} -gt 0` is false; notification skipped | None — no false alerts |
| REFS array is empty | `printf` prints nothing for refs section | Email sent with empty refs list |

**Note on `set -e` and `mail` failure**: The `mail` command is inside an `if` block (the `if command -v mail` block), which shields it from `set -e`. However, if `mail` itself fails, the pipeline `{ ... } | mail ...` would fail. Under `pipefail`, this could cause the `if` block's body to fail. In practice, `mail` commands rarely fail immediately — they queue messages locally even if delivery fails.

---

## Extending the Notification System

### Adding Webhook Notifications

To send notifications via a generic webhook (e.g., for monitoring tools):

```bash
# Add after the mail block:
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_WEBHOOK:-}" ]]; then
    if command -v curl &>/dev/null; then
        curl -sf -X POST \
            -H "Content-Type: application/json" \
            -d "{
                \"event\": \"mirror_failure\",
                \"repository\": \"$(basename "$(pwd)")\",
                \"failed_remotes\": [$(printf '\"%s\",' "${FAILED_REMOTES[@]}" | sed 's/,$//')]
            }" \
            "$MIRROR_WEBHOOK" 2>/dev/null || true
    fi
fi
```

Configure with: `export MIRROR_WEBHOOK="https://monitoring.example.com/hooks/git-mirror"`

### Adding Slack Integration

```bash
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_SLACK_WEBHOOK:-}" ]]; then
    if command -v curl &>/dev/null; then
        REMOTE_LIST=$(printf '• %s\\n' "${FAILED_REMOTES[@]}")
        REF_LIST=$(printf '• %s\\n' "${REFS[@]}")
        curl -sf -X POST \
            -H "Content-Type: application/json" \
            -d "{
                \"text\": \":x: *Mirror push failed* in \`$(basename "$(pwd)")\`\",
                \"blocks\": [
                    {
                        \"type\": \"section\",
                        \"text\": {
                            \"type\": \"mrkdwn\",
                            \"text\": \":x: *Mirror push failed* in \`$(basename "$(pwd)")\`\\n\\n*Failed remotes:*\\n${REMOTE_LIST}\\n\\n*Refs updated:*\\n${REF_LIST}\"
                        }
                    }
                ]
            }" \
            "$MIRROR_SLACK_WEBHOOK" 2>/dev/null || true
    fi
fi
```

Configure with: `export MIRROR_SLACK_WEBHOOK="https://hooks.slack.com/services/T.../B.../xxx"`

### Adding Discord Integration

```bash
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_DISCORD_WEBHOOK:-}" ]]; then
    if command -v curl &>/dev/null; then
        REMOTE_LIST=$(printf '- %s\n' "${FAILED_REMOTES[@]}")
        curl -sf -X POST \
            -H "Content-Type: application/json" \
            -d "{
                \"content\": \"**Mirror push failed** in \`$(basename "$(pwd)")\`\\n\\nFailed remotes:\\n${REMOTE_LIST}\\n\\nCheck log: ${MIRROR_LOG}\"
            }" \
            "$MIRROR_DISCORD_WEBHOOK" 2>/dev/null || true
    fi
fi
```

Configure with: `export MIRROR_DISCORD_WEBHOOK="https://discord.com/api/webhooks/xxx/yyy"`

### Adding Matrix Integration

```bash
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_MATRIX_WEBHOOK:-}" ]]; then
    if command -v curl &>/dev/null; then
        REMOTE_LIST=$(printf '- %s\n' "${FAILED_REMOTES[@]}")
        MSG="Mirror push failed in $(basename "$(pwd)")\n\nFailed remotes:\n${REMOTE_LIST}"
        curl -sf -X POST \
            -H "Content-Type: application/json" \
            -d "{
                \"msgtype\": \"m.text\",
                \"body\": \"${MSG}\"
            }" \
            "$MIRROR_MATRIX_WEBHOOK" 2>/dev/null || true
    fi
fi
```

### Adding SMS Notifications

Using Twilio as an example:

```bash
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_TWILIO_SID:-}" ]]; then
    if command -v curl &>/dev/null; then
        REMOTE_LIST=$(printf '%s, ' "${FAILED_REMOTES[@]}" | sed 's/, $//')
        curl -sf -X POST \
            "https://api.twilio.com/2010-04-01/Accounts/${MIRROR_TWILIO_SID}/Messages.json" \
            -u "${MIRROR_TWILIO_SID}:${MIRROR_TWILIO_TOKEN}" \
            -d "From=${MIRROR_TWILIO_FROM}" \
            -d "To=${MIRROR_TWILIO_TO}" \
            -d "Body=Mirror push failed in $(basename "$(pwd)"). Failed: ${REMOTE_LIST}" \
            2>/dev/null || true
    fi
fi
```

---

## Notification Suppression

To temporarily suppress notifications without removing the `MIRROR_NOTIFY` configuration:

```bash
# Method 1: Unset for a single invocation
unset MIRROR_NOTIFY
echo "..." | hooks/post-receive

# Method 2: Override with empty string
MIRROR_NOTIFY="" hooks/post-receive

# Method 3: Remove notification config from systemd
# Edit the service file and remove the MIRROR_NOTIFY line
sudo systemctl edit git-daemon
```

The hook's design ensures notifications are never sent unless explicitly enabled, so the default state is already "suppressed."

---

## Monitoring and Alerting Integration

For production deployments, the notification system can be integrated with monitoring platforms:

### Prometheus + Alertmanager

Expose mirror status as a Prometheus metric by writing to a textfile collector:

```bash
# Add to the end of the hook:
METRICS_DIR="/var/lib/prometheus/node-exporter"
if [[ -d "$METRICS_DIR" ]]; then
    cat > "$METRICS_DIR/git_mirror.prom" <<EOF
# HELP git_mirror_last_run_timestamp_seconds Unix timestamp of the last mirror run
# TYPE git_mirror_last_run_timestamp_seconds gauge
git_mirror_last_run_timestamp_seconds $(date +%s)
# HELP git_mirror_failed_remotes_total Number of remotes that failed in the last run  
# TYPE git_mirror_failed_remotes_total gauge
git_mirror_failed_remotes_total ${#FAILED_REMOTES[@]}
# HELP git_mirror_succeeded_remotes_total Number of remotes that succeeded
# TYPE git_mirror_succeeded_remotes_total gauge
git_mirror_succeeded_remotes_total ${#SUCCEEDED_REMOTES[@]}
EOF
fi
```

### Healthcheck Pings

Integrate with uptime monitoring services:

```bash
# Ping a healthcheck endpoint on success
if [[ ${#FAILED_REMOTES[@]} -eq 0 && -n "${MIRROR_HEALTHCHECK_URL:-}" ]]; then
    curl -sf "$MIRROR_HEALTHCHECK_URL" 2>/dev/null || true
fi

# Signal failure
if [[ ${#FAILED_REMOTES[@]} -gt 0 && -n "${MIRROR_HEALTHCHECK_URL:-}" ]]; then
    curl -sf "${MIRROR_HEALTHCHECK_URL}/fail" 2>/dev/null || true
fi
```

Configure with: `export MIRROR_HEALTHCHECK_URL="https://hc-ping.com/uuid-here"`

This allows dead-man's-switch monitoring — if no push occurs within the expected interval, the monitoring service alerts.
