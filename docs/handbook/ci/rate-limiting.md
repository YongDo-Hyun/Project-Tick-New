# Rate Limiting

## Overview

The CI system interacts heavily with the GitHub REST API for PR validation, commit
analysis, review management, and branch comparison. To prevent exhausting the
GitHub API rate limit (5,000 requests/hour for authenticated tokens), all API calls
are routed through `ci/github-script/withRateLimit.js`, which uses the
[Bottleneck](https://github.com/SGrondin/bottleneck) library for request throttling.

---

## Architecture

### Request Flow

```
┌──────────────────────────┐
│  CI Script               │
│  (lint-commits.js,       │
│   prepare.js, etc.)      │
└────────────┬─────────────┘
             │ github.rest.*
             ▼
┌──────────────────────────┐
│  withRateLimit wrapper   │
│  ┌──────────────────┐   │
│  │ allLimits         │   │  ← Bottleneck (maxConcurrent: 1, reservoir: dynamic)
│  │ (all requests)    │   │
│  └──────────────────┘   │
│  ┌──────────────────┐   │
│  │ writeLimits       │   │  ← Bottleneck (minTime: 1000ms) chained to allLimits
│  │ (POST/PUT/PATCH/  │   │
│  │  DELETE only)     │   │
│  └──────────────────┘   │
└────────────┬─────────────┘
             │
             ▼
┌──────────────────────────┐
│  GitHub REST API         │
│  api.github.com          │
└──────────────────────────┘
```

---

## Implementation

### Module Signature

```javascript
module.exports = async ({ github, core, maxConcurrent = 1 }, callback) => {
```

| Parameter       | Type     | Default | Description                          |
|----------------|----------|---------|--------------------------------------|
| `github`        | Object  | —       | Octokit instance from `@actions/github` |
| `core`          | Object  | —       | `@actions/core` for logging          |
| `maxConcurrent` | Number  | `1`     | Maximum concurrent API requests      |
| `callback`      | Function| —       | The script logic to execute          |

### Bottleneck Configuration

Two Bottleneck limiters are configured:

#### allLimits — Controls all requests

```javascript
const allLimits = new Bottleneck({
  maxConcurrent,
  reservoir: 0,  // Updated dynamically
})
```

- `maxConcurrent: 1` — Only one API request at a time (prevents burst usage)
- `reservoir: 0` — Starts empty; updated by `updateReservoir()` before first use

#### writeLimits — Additional throttle for mutative requests

```javascript
const writeLimits = new Bottleneck({ minTime: 1000 }).chain(allLimits)
```

- `minTime: 1000` — At least 1 second between write requests
- `.chain(allLimits)` — Write requests also go through the global limiter

---

## Request Classification

The Octokit `request` hook intercepts every API call and routes it through
the appropriate limiter:

```javascript
github.hook.wrap('request', async (request, options) => {
  // Bypass: different host (e.g., github.com for raw downloads)
  if (options.url.startsWith('https://github.com')) return request(options)

  // Bypass: rate limit endpoint (doesn't count against quota)
  if (options.url === '/rate_limit') return request(options)

  // Bypass: search endpoints (separate rate limit pool)
  if (options.url.startsWith('/search/')) return request(options)

  stats.requests++

  if (['POST', 'PUT', 'PATCH', 'DELETE'].includes(options.method))
    return writeLimits.schedule(request.bind(null, options))
  else
    return allLimits.schedule(request.bind(null, options))
})
```

### Bypass Rules

| URL Pattern                    | Reason                                      |
|-------------------------------|---------------------------------------------|
| `https://github.com/*`        | Raw file downloads, not API calls           |
| `/rate_limit`                 | Meta-endpoint, doesn't count against quota  |
| `/search/*`                   | Separate rate limit pool (30 requests/min)  |

### Request Routing

| HTTP Method           | Limiter          | Throttle Rule                    |
|----------------------|------------------|----------------------------------|
| `GET`                | `allLimits`      | Concurrency-limited + reservoir  |
| `POST`               | `writeLimits`    | 1 second minimum + concurrency   |
| `PUT`                | `writeLimits`    | 1 second minimum + concurrency   |
| `PATCH`              | `writeLimits`    | 1 second minimum + concurrency   |
| `DELETE`             | `writeLimits`    | 1 second minimum + concurrency   |

---

## Reservoir Management

### Dynamic Reservoir Updates

The reservoir tracks how many API requests the script is allowed to make:

```javascript
async function updateReservoir() {
  let response
  try {
    response = await github.rest.rateLimit.get()
  } catch (err) {
    core.error(`Failed updating reservoir:\n${err}`)
    return
  }
  const reservoir = Math.max(0, response.data.resources.core.remaining - 1000)
  core.info(`Updating reservoir to: ${reservoir}`)
  allLimits.updateSettings({ reservoir })
}
```

### Reserve Buffer

The script always keeps **1,000 spare requests** for other concurrent jobs:

```javascript
const reservoir = Math.max(0, response.data.resources.core.remaining - 1000)
```

If the rate limit shows 3,500 remaining requests, the reservoir is set to 2,500.
If remaining is below 1,000, the reservoir is set to 0 (all requests will queue).

### Why 1,000?

Other GitHub Actions jobs running in parallel (status checks, deployment workflows,
external integrations) typically use fewer than 100 requests each. A 1,000-request
buffer provides ample headroom:

- Normal job: ~50–100 API calls
- 10 concurrent jobs: ~500–1,000 API calls
- Buffer: 1,000 requests — covers typical parallel workload

### Update Schedule

```javascript
await updateReservoir()  // Initial update before any work
const reservoirUpdater = setInterval(updateReservoir, 60 * 1000)  // Every 60 seconds
```

The reservoir is refreshed every minute to account for:
- Other jobs consuming requests in parallel
- Rate limit window resets (GitHub resets the limit every hour)

### Cleanup

```javascript
try {
  await callback(stats)
} finally {
  clearInterval(reservoirUpdater)
  core.notice(
    `Processed ${stats.prs} PRs, ${stats.issues} Issues, ` +
      `made ${stats.requests + stats.artifacts} API requests ` +
      `and downloaded ${stats.artifacts} artifacts.`,
  )
}
```

The interval is cleared in a `finally` block to prevent resource leaks even if
the callback throws an error.

---

## Statistics Tracking

The wrapper tracks four metrics:

```javascript
const stats = {
  issues: 0,
  prs: 0,
  requests: 0,
  artifacts: 0,
}
```

| Metric       | Incremented By                        | Purpose                          |
|-------------|---------------------------------------|----------------------------------|
| `requests`   | Every throttled API call              | Total API usage                  |
| `prs`        | Callback logic (PR processing)        | PRs analyzed                     |
| `issues`     | Callback logic (issue processing)     | Issues analyzed                  |
| `artifacts`  | Callback logic (artifact downloads)   | Artifacts downloaded             |

At the end of execution, a summary is logged:

```
Notice: Processed 1 PRs, 0 Issues, made 15 API requests and downloaded 0 artifacts.
```

---

## Error Handling

### Rate Limit API Failure

If the rate limit endpoint itself fails (network error, GitHub outage):

```javascript
try {
  response = await github.rest.rateLimit.get()
} catch (err) {
  core.error(`Failed updating reservoir:\n${err}`)
  return  // Keep retrying on next interval
}
```

The error is logged but does not crash the script. The reservoir retains its
previous value, and the next 60-second interval will try again.

### Exhausted Reservoir

When the reservoir reaches 0:
- All new requests queue in Bottleneck
- Requests wait until the next `updateReservoir()` call adds capacity
- If GitHub's rate limit has not reset, requests continue to queue
- The script may time out if the rate limit window hasn't reset

---

## GitHub API Rate Limits Reference

| Resource     | Limit                    | Reset Period |
|-------------|--------------------------|--------------|
| Core REST API| 5,000 requests/hour     | Rolling hour |
| Search API   | 30 requests/minute      | Rolling minute|
| GraphQL API  | 5,000 points/hour       | Rolling hour |

The `withRateLimit.js` module only manages the **Core REST API** limit. Search
requests bypass the throttle because they have a separate, lower limit that is
rarely a concern for CI scripts.

---

## Usage in CI Scripts

### Wrapping a Script

```javascript
const withRateLimit = require('./withRateLimit.js')

module.exports = async ({ github, core }) => {
  await withRateLimit({ github, core }, async (stats) => {
    // All github.rest.* calls here are automatically throttled

    const pr = await github.rest.pulls.get({
      owner: 'YongDo-Hyun',
      repo: 'Project-Tick',
      pull_number: 123,
    })
    stats.prs++

    // ... more API calls
  })
}
```

### Adjusting Concurrency

For scripts that can safely parallelize reads:

```javascript
await withRateLimit({ github, core, maxConcurrent: 5 }, async (stats) => {
  // Up to 5 concurrent GET requests
  // Write requests still have 1-second minimum spacing
})
```

---

## Best Practices

1. **Minimize API calls** — Use pagination efficiently, avoid redundant requests
2. **Prefer git over API** — For commit data, `get-pr-commit-details.js` uses git directly
   to bypass the 250-commit API limit and reduce API usage
3. **Use the `stats` object** — Track what the script does for observability
4. **Don't bypass the wrapper** — All API calls should go through the throttled Octokit instance
5. **Handle network errors** — The wrapper handles rate limit API failures, but callback
   scripts should handle their own API errors gracefully
