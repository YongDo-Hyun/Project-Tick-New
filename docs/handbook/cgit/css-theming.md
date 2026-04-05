# cgit — CSS Theming

## Overview

cgit ships with a comprehensive CSS stylesheet (`cgit.css`) that controls
the visual appearance of all pages.  The stylesheet is designed with a light
color scheme and semantic CSS classes that map directly to cgit's HTML
structure.

Source file: `cgit.css` (~450 lines).

## Loading Stylesheets

CSS files are specified via the `css` configuration directive:

```ini
css=/cgit/cgit.css
```

Multiple stylesheets can be loaded by repeating the directive:

```ini
css=/cgit/cgit.css
css=/cgit/custom.css
```

Stylesheets are included in document order in the `<head>` section via
`cgit_print_docstart()` in `ui-shared.c`.

## Page Structure

The HTML layout uses this basic structure:

```html
<body>
  <div id='cgit'>
    <table id='header'>...</table>      <!-- site header with logo -->
    <table id='navigation'>...</table>  <!-- tab navigation -->
    <div id='content'>                  <!-- page content -->
      <!-- page-specific content -->
    </div>
    <div class='footer'>...</div>       <!-- footer -->
  </div>
</body>
```

## Base Styles

### Body and Layout

```css
body {
    font-family: sans-serif;
    font-size: 11px;
    color: #000;
    background: white;
    padding: 4px;
}

div#cgit {
    padding: 0;
    margin: 0;
    font-family: monospace;
    font-size: 12px;
}
```

### Header

```css
table#header {
    width: 100%;
    margin-bottom: 1em;
}

table#header td.logo {
    /* logo cell */
}

table#header td.main {
    font-size: 250%;
    font-weight: bold;
    vertical-align: bottom;
    padding-left: 10px;
}

table#header td.sub {
    color: #999;
    font-size: 75%;
    vertical-align: top;
    padding-left: 10px;
}
```

### Navigation Tabs

```css
table#navigation {
    width: 100%;
}

table#navigation a {
    padding: 2px 6px;
    color: #000;
    text-decoration: none;
}

table#navigation a:hover {
    color: #00f;
}
```

## Content Areas

### Repository List

```css
table.list {
    border-collapse: collapse;
    border: solid 1px #aaa;
    width: 100%;
}

table.list th {
    text-align: left;
    font-weight: bold;
    background: #ddd;
    border-bottom: solid 1px #aaa;
    padding: 2px 4px;
}

table.list td {
    padding: 2px 4px;
    border: none;
}

table.list tr:hover {
    background: #eee;
}

table.list td a {
    color: #00f;
    text-decoration: none;
}

table.list td a:hover {
    text-decoration: underline;
}
```

### Sections

```css
div.section-header {
    background: #eee;
    border: solid 1px #ddd;
    padding: 2px 4px;
    font-weight: bold;
    margin-top: 1em;
}
```

## Diff Styles

### Diffstat

```css
table.diffstat {
    border-collapse: collapse;
    border: solid 1px #aaa;
}

table.diffstat td {
    padding: 1px 4px;
    border: none;
}

table.diffstat td.mode {
    font-weight: bold;
    /* status indicator: A/M/D/R */
}

table.diffstat td.graph {
    width: 500px;
}

table.diffstat td.graph span.add {
    background: #5f5;
    /* green bar for additions */
}

table.diffstat td.graph span.rem {
    background: #f55;
    /* red bar for deletions */
}

table.diffstat .total {
    font-weight: bold;
    text-align: center;
}
```

### Unified Diff

```css
table.diff {
    width: 100%;
}

table.diff td div.head {
    font-weight: bold;
    margin-top: 1em;
    color: #000;
}

table.diff td div.hunk {
    color: #009;
    /* hunk header @@ ... @@ */
}

table.diff td div.add {
    color: green;
    background: #dfd;
}

table.diff td div.del {
    color: red;
    background: #fdd;
}
```

### Side-by-Side Diff

```css
table.ssdiff {
    width: 100%;
}

table.ssdiff td {
    font-family: monospace;
    font-size: 12px;
    padding: 1px 4px;
    vertical-align: top;
}

table.ssdiff td.lineno {
    text-align: right;
    width: 3em;
    background: #eee;
    color: #999;
}

table.ssdiff td.add {
    background: #dfd;
}

table.ssdiff td.del {
    background: #fdd;
}

table.ssdiff td.changed {
    background: #ffc;
}

table.ssdiff span.add {
    background: #afa;
    font-weight: bold;
}

table.ssdiff span.del {
    background: #faa;
    font-weight: bold;
}
```

## Blob/Tree View

```css
table.blob {
    border-collapse: collapse;
    width: 100%;
}

table.blob td {
    font-family: monospace;
    font-size: 12px;
    padding: 0 4px;
    vertical-align: top;
}

table.blob td.linenumbers {
    text-align: right;
    color: #999;
    background: #eee;
    width: 3em;
    border-right: solid 1px #ddd;
}

table.blob td.lines {
    white-space: pre;
}
```

### Tree Listing

```css
table.list td.ls-mode {
    font-family: monospace;
    width: 10em;
}

table.list td.ls-size {
    text-align: right;
    width: 5em;
}
```

## Commit View

```css
table.commit-info {
    border-collapse: collapse;
    border: solid 1px #aaa;
    margin-bottom: 1em;
}

table.commit-info th {
    text-align: left;
    font-weight: bold;
    padding: 2px 6px;
    vertical-align: top;
}

table.commit-info td {
    padding: 2px 6px;
}

div.commit-subject {
    font-weight: bold;
    font-size: 125%;
    margin: 1em 0 0.5em;
}

div.commit-msg {
    white-space: pre;
    font-family: monospace;
}

div.notes-header {
    font-weight: bold;
    padding-top: 1em;
}

div.notes {
    white-space: pre;
    font-family: monospace;
    border-left: solid 3px #dd5;
    padding: 0.5em;
    background: #ffe;
}
```

## Log View

```css
div.commit-graph {
    font-family: monospace;
    white-space: pre;
    color: #000;
}

/* Column colors for commit graph */
.column1  { color: #a00; }
.column2  { color: #0a0; }
.column3  { color: #00a; }
.column4  { color: #aa0; }
.column5  { color: #0aa; }
.column6  { color: #a0a; }
```

## Stats View

```css
table.stats {
    border-collapse: collapse;
    border: solid 1px #aaa;
}

table.stats th {
    text-align: left;
    padding: 2px 6px;
    background: #ddd;
}

table.stats td {
    padding: 2px 6px;
}

div.stats-graph {
    /* bar chart container */
}
```

## Form Elements

```css
div.cgit-panel {
    float: right;
    margin: 0 0 0.5em 0.5em;
    padding: 4px;
    border: solid 1px #aaa;
    background: #eee;
}

div.cgit-panel b {
    display: block;
    margin-bottom: 2px;
}

div.cgit-panel select,
div.cgit-panel input {
    font-size: 11px;
}
```

## Customization Strategies

### Method 1: Override Stylesheet

Create a custom CSS file that overrides specific rules:

```css
/* /cgit/custom.css */
body {
    background: #1a1a2e;
    color: #e0e0e0;
}

div#cgit {
    background: #16213e;
}

table.list th {
    background: #0f3460;
    color: #e0e0e0;
}
```

```ini
css=/cgit/cgit.css
css=/cgit/custom.css
```

### Method 2: Replace Stylesheet

Replace the default stylesheet entirely:

```ini
css=/cgit/mytheme.css
```

### Method 3: head-include

Inject inline styles via the `head-include` directive:

```ini
head-include=/etc/cgit/extra-head.html
```

```html
<!-- /etc/cgit/extra-head.html -->
<style>
  body { background: #f0f0f0; }
</style>
```

## CSS Classes Reference

### Layout Classes

| Class/ID | Element | Description |
|----------|---------|-------------|
| `#cgit` | div | Main container |
| `#header` | table | Site header |
| `#navigation` | table | Tab navigation |
| `#content` | div | Page content area |
| `.footer` | div | Page footer |

### Content Classes

| Class | Element | Description |
|-------|---------|-------------|
| `.list` | table | Data listing (repos, files, refs) |
| `.blob` | table | File content display |
| `.diff` | table | Unified diff |
| `.ssdiff` | table | Side-by-side diff |
| `.diffstat` | table | Diff statistics |
| `.commit-info` | table | Commit metadata |
| `.stats` | table | Statistics data |
| `.cgit-panel` | div | Control panel |

### Diff Classes

| Class | Element | Description |
|-------|---------|-------------|
| `.add` | div/span | Added lines/chars |
| `.del` | div/span | Deleted lines/chars |
| `.hunk` | div | Hunk header |
| `.ctx` | div | Context lines |
| `.head` | div | File header |
| `.changed` | td | Modified line (ssdiff) |
| `.lineno` | td | Line number column |

### Status Classes

| Class | Description |
|-------|-------------|
| `.upd` | Modified file |
| `.add` | Added file |
| `.del` | Deleted file |
| `.mode` | File mode indicator |
| `.graph` | Graph bar container |
