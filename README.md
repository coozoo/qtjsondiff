[<img src="https://copr.fedorainfracloud.org/coprs/yura/QTjsonDiff/package/QTjsonDiff/status_image/last_build.png"></img>](https://copr.fedorainfracloud.org/coprs/yura/QTjsonDiff/) [<img src="https://github.com/coozoo/qtjsondiff/workflows/Release_Version/badge.svg"></img>](https://github.com/coozoo/qtjsondiff/releases/latest)

# QT JSON diff

## Contents

* [Summary](#summary)
* [Installation](#installation)
* [Build from sources](#build-from-sources)
   * [MAC OS](#mac-os)
   * [Linux](#linux)
   * [Windows](#windows)
* [Integrate it to your application](#integrate-it-to-your-application)
* [Command line](#command-line)
* [Comparison modes](#comparison-modes)


## Summary

Some kind of diff viewer for Json (based on a tree-like json container/viewer widget).

Actually, I've created this widget for myself. As a tester, I often need to compare JSONs from different sources or simply need a handy viewer that can work with really big JSONs.

Usually, online viewers simply crash and hang with such data. This viewer is still able to work with such big JSONs.

For example, I'm using this in my SignalR and cometD clients to visualize responses or simply to show and compare HTTP responses from other sources. And I've found this example app pretty handy as well.

Some features:

    - **Dual View Modes**: Switch between a formatted JSON text view and an interactive tree view.
    - **Data Loading**: Load JSON from a file, a URL, or by pasting directly into the tree view (Ctrl+V).
    - **Custom HTTP requests**: for URL sources, the gear button next to *Reload* opens a dialog to configure the HTTP method (GET/POST/PUT/PATCH/DELETE/HEAD), custom headers with autocomplete for common names, and the request body. Presets are stored as plain cURL text files (Save… / Load…), so you can paste `curl -X POST … -H '…' --data-raw '…'` straight from browser devtools "Copy as cURL". A **Reset** button restores the built-in default GET (with the three sensible default headers). When the config is non-default, the gear button gets an orange border so you can tell at a glance that reloading won't be a plain GET.
    - **Inline Editing** (opt-in): Edit keys, types and values directly within the tree view.
      Toggle from **Preferences → JSON Editing**.
      * **Single-tree tab**: right-click → *Add child* / *Delete row*.
      * **Diff tab**: each side gets a toolbar arrow that resolves the selected difference — for paired rows it overwrites the other side's value with this side's; for rows that exist only here, it inserts a copy on the other side. A *Delete here* button removes the selected row (the peer on the other side becomes *NotPresent* until you delete or push it too). Colors update live as you edit — no need to press *Compare* again. Renaming a key detaches the pair (both sides become *NotPresent*); changing a value or type marks the pair *Huge*.
    - **Threaded Compare with Progress + Cancel**: the *Compare* button runs on a worker thread; a modal progress dialog shows progress per node and a *Cancel* button aborts the run safely. The trees stay blocked from input until the compare finishes so the snapshot stays consistent.
    - **Search**: Full search functionality (forward, backward, case-sensitive) for both text and tree views.
    - **JSON Comparison**: Compare two JSON documents with highlighting for differences, synchronized scrolling, and synchronized item selection in tree view.
    - **Array Sorting**: Sort objects within arrays to simplify comparison when order is not important.
    - **Comparison Modes**:
      * **Full Path**: (Default, Fast) Compares items based on their absolute path.
      * **Parent+Child Pair**: (Slow) Finds the first occurrence of a parent-child pair anywhere in the JSON, useful for structurally different but content-similar JSONs.
    - **Smart Array** (optional): compare arrays and objects by content instead of by position. Reordered lists still pair correctly, and any item that exists on only one side shows up as a ghost row on the other side so matched pairs stay lined up row-for-row for sync-scroll and side-by-side reading. Fill in the **Match key** field (e.g. `id`) to prefer that field as the item's identity when comparing arrays of objects.
    - **Rich Clipboard Support**:
      * **Copy Row**: Copies the selected item's text ("Key Type Value").
      * **Copy Rows**: Copies the entire tree content for pasting into spreadsheets.
      * **Copy Path**: Copies the Qt-style path to the item (e.g., `root(Object)->widget(Object)`).
      * **Copy jq Path**: Copies the path in `jq` compatible syntax (e.g., `'.widget.image.alignment'`).
      * **Copy Plain/Pretty JSON**: Copies the full JSON document, either minified or indented.
      * **Copy Selected Plain/Pretty**: Copies just the selected object or array, either minified or indented.
    - **Tree Expansion**:
      * **Expand/Collapse Selected**: Expands or collapses the currently selected items.
      * **Expand/Collapse Recursively**: Expands or collapses the selected items and all their children.
    - **Configurable** (Preferences, persisted via `QSettings`):
      * Colors for *Identical*, *Moderate*, *Huge*, *NotPresent* + a global alpha; syntax highlighter colors; one-click restore-to-defaults.
      * Tab position (north/south/west/east), JSON-view button position.
      * Keyboard shortcuts for every clipboard action (editable in-table, restorable).
      * **JSON Editing** toggles for the single-tree tab and the diff view.
      * Optional restore-last-loaded-paths on startup.


JSON Tree View

<img src="https://user-images.githubusercontent.com/25594311/72466610-e78a6f00-37e1-11ea-91dd-cdbe86c317d6.png" width="60%"></img> 

JSON Compare View

<img src="https://user-images.githubusercontent.com/25594311/104792301-9b805a80-57a6-11eb-8cd5-eae3e7ceb78d.png" width="60%"></img> 

## Installation

Precompiled RPMs (**Fedora**, **RHEL**, **Centos**, **OpenSuse**) can be found in COPR:

```bash
sudo dnf copr enable yura/QTjsonDiff
sudo dnf install qtjsondiff

```

[<img src="https://copr.fedorainfracloud.org/coprs/yura/QTjsonDiff/package/QTjsonDiff/status_image/last_build.png"></img>](https://copr.fedorainfracloud.org/coprs/yura/QTjsonDiff/)


Precompiled deb

You need to add repo use commands below:

```bash
sudo add-apt-repository ppa:coozoo/qtjsondiff
sudo apt update
sudo apt-get install qtjsondiff
```

**Debian**, **Ubuntu** and **some other Linux distros** may try precompiled and packed packages by linuxdeployqt available on releases page to use them don't forget install fuse system and make downloaded file executable.

**Windows** zip archives available just extract and run exe file.

**Mac OS** zip and DMG as well available. 

You should dance and turn few times in order to launch it. Once app unpacked you need to allow it  for damn mac security and it's getting harder from day to day

```
# adjust application location accordingly to yours
xattr -dr com.apple.quarantine /Applications/qtjsondiff.app
codesign --force --deep --sign - /Applications/qtjsondiff.app
```

You can find all of above in release section.

[<img src="https://github.com/coozoo/qtjsondiff/workflows/Release_Version/badge.svg"></img>](https://github.com/coozoo/qtjsondiff/releases/latest)

If you prefer to compile it by yourself then see below. 


## Build from sources

You should have preinstalled Qt6 (it can be build with latest Qt5).
Open in QTcreator the QTjsonDiff.pro file and compile it (you will get something).

### MAC OS

Suppose you have installed and configured:
  - xCode+command line tools
  - Qt6 (```brew install qt```)

```bash
$ cd ~
$ mkdir proj
$ cd proj
$ git clone https://github.com/coozoo/qtjsondiff
$ cd qtjsondiff
$ chmod 777 MAC_build_RELEASE.sh
$ ./MAC_build_RELEASE.sh
```

It will build and copy libs into app. You will find ready app inside this directory.

### Linux

You should have Qt6 if no then install it accordingly to your distro.

You can build it with QTcreator or execute commands:
```bash
$ git clone https://github.com/coozoo/qtjsondiff
$ cd qtjsondiff
$ qmake6 # or it can be simply qmake
$ make
$ make install #if you wish to do that
```


### Windows

Qt6 should be installed.

git clone https://github.com/coozoo/qtjsondiff

Open QTCreator and navigate to project dir. Open QTjsonDiff.pro and compile it.

If you want to load jsons from https source then you need openssl. Under windows QT provides their own openssl you can install it using QT installer from start menu and add this component.

## Integrate it to your application

Actually this application it is example so you can simply to view MainWindow class and you will get idea.

You need to include those files:
```cpp
#include "qjsonitem.h"
#include "qjsonmodel.h"
#include "qjsoncontainer.h"
#include "qjsondiff.h"
#include "jsondiffengine.h"          // only if you want to drive the compare engine directly
#include "httprequestconfig.h"       // pulled in transitively by qjsoncontainer.h;
                                     // include it explicitly if you build/read
                                     // HttpRequestConfig values in your own code

#include "preferences/preferences.h"
#include "preferences/preferencesdialog.h"

```
If you need just json tree view you can ignore `qjsondiff.h` and `jsondiffengine.h`.

### Files you must build/link

`QJsonContainer` itself opens the request-config dialog when the user clicks the gear button, so **the whole HTTP-request trio compiles and links unconditionally** with the widget:

- `httprequestconfig.{h,cpp}` — data class + cURL parser
- `httprequestconfigwidget.{h,cpp}` — reusable form + cURL tab
- `httprequestconfigdialog.{h,cpp}` — thin QDialog wrapper used by the toolbar button

Add all three `.cpp` files to your project's `SOURCES` (or their equivalent in your build system) and their headers to `HEADERS`. Skipping any of them will break the container's link step. There's no "no-network" build flavor — if you want to strip the feature, call `configureRequest_toolButton->hide()` on the container after construction.

Declare pointers:
```cpp
    QJsonContainer *messageJsonCont; //json treeview
    QJsonDiff *differ;               //json diff like treeview
```

You need to create some UI elements where you can put those objects.

Create objects and define their properties:
```cpp

    // create json treeview and set tab as parent
    messageJsonCont=new QJsonContainer(ui->jsonview_tab);
    // you can hide address line and browse button
    //messageJsonCont->setBrowseVisible(false);
    // you can let users edit keys, values and types inline (and
    // unlock the Add child / Delete row context-menu items)
    //messageJsonCont->setEditable(true);
    // you can set json text
    messageJsonCont->loadJson(QString("{\"empty\":\"empty\"}"));

    // create json diff like tree view set some tab as parrent
    differ=new QJsonDiff(ui->compare_tab);
    // opt-in to inline diff editing: both trees become editable,
    // per-side toolbar arrows resolve Huge differences and push
    // NotPresent rows to the other side, and Delete here removes
    // the selected row. Colors refresh live as the user edits.
    //differ->setDiffEditable(true);
    // set some text to left diff view
    QJsonDocument data22=QJsonDocument::fromJson("{\"empty\":\"empty\"}");
    differ->loadJsonLeft(data22);
    // set some text to right diff view
    QJsonDocument data11=QJsonDocument::fromJson("{\"empty\":\"empty\"}");
    differ->loadJsonRight(data11);
```

That's all pretty simple. Both edit-mode switches are off by default
so the original read-only widget contract is preserved.

If you'd like to bind your own shortcuts to the edit actions instead
of going through the toolbar / context menu, every action is exposed
as a public slot:

```cpp
// Diff widget
differ->pushLeftSelectionToRight();
differ->pushRightSelectionToLeft();
differ->deleteLeftSelection();
differ->deleteRightSelection();

// Single tree
messageJsonCont->addChildOf(parentIndex);   // returns the new row
messageJsonCont->removeAt(targetIndex);
```

All of them no-op when their side's edit mode is off, so you can
wire them up unconditionally.

### Configuring HTTP requests programmatically

When a `QJsonContainer` is loading a URL rather than a file, you can drive method, headers, and body via `HttpRequestConfig` — the same class the built-in gear-button dialog edits. Custom values apply only when the address is a URL; file paths ignore them.

```cpp
#include "httprequestconfig.h"

HttpRequestConfig cfg;
cfg.method = HttpRequestConfig::Post;
cfg.headers.append(qMakePair(QStringLiteral("Authorization"),
                             QStringLiteral("Bearer XYZ")));
cfg.headers.append(qMakePair(QStringLiteral("Content-Type"),
                             QStringLiteral("application/json")));
cfg.body = "{\"greeting\":\"hi\"}";

messageJsonCont->setRequestConfig(cfg);
// or, to go back to a plain GET with the three sensible defaults:
messageJsonCont->setRequestConfig(HttpRequestConfig::defaults());
```

The container also emits `requestConfigChanged(HttpRequestConfig)` when the user changes the config through the gear button on the toolbar (`configureRequest_toolButton`), so you can persist or forward it however you like.

The text format used for the built-in **Save…** / **Load…** buttons on the request dialog *and* for the CLI's `--config*` flags is a normal cURL command:

```bash
curl -X POST 'https://api.example.com/foo' \
  -H 'Authorization: Bearer XXX' \
  -H 'Content-Type: application/json' \
  --data-raw '{"key":"value"}'
```

You can paste this straight from browser devtools' *Copy as cURL*. Round-trip both ways in code with `HttpRequestConfig::fromCurlText(text, &ok)` and `.toCurlText()`. Presets are just files on disk — there's no separate preferences store to manage.

If you want to run a comparison without the widget (for example in a CLI tool or a background job), you can use `JsonDiffEngine` directly:

```cpp
#include "jsondiffengine.h"
#include "qjsonmodel.h"

// take a snapshot of each model
DiffNode left  = JsonDiffEngine::snapshot(leftModel);
DiffNode right = JsonDiffEngine::snapshot(rightModel);

// simple positional compare (Mode::FullPath or Mode::ParentChildPair)
JsonDiffEngine::compare(left, right, JsonDiffEngine::Mode::FullPath);

// or turn Smart Array on — match items by content instead of by
// index, so reordered arrays pair correctly and missing items
// surface as ghost rows on the opposite side. The third argument
// is the match-key (empty = compare items by their whole content)
// and the fourth is the Smart Array switch.
JsonDiffEngine::compare(left, right, JsonDiffEngine::Mode::FullPath,
                        /*matchKey=*/"id", /*arrayOverlay=*/true);

// apply the result back onto the models so the tree views show colors
// and (under Smart Array) ghost rows for the missing items.
JsonDiffEngine::apply(left,  leftModel,  rightModel);
JsonDiffEngine::apply(right, rightModel, leftModel);
```

That's all you need for a basic compare. `QJsonDiff` does the same thing internally on a background thread with a progress dialog. The toolbar controls above the diff view are `modeCombo` (QComboBox — Full Path / Parent+Child), `arrayOverlay_checkbox` (QCheckBox — Smart Array), and `matchKey_lineEdit` (QLineEdit); all three are public members if you want to drive them from your own code.


## Command line

```
qtjsondiff [--style <style>]
           [--config <curl-text> | --config-file <path>]
           [--config-left <curl-text> | --config-left-file <path>]
           [--config-right <curl-text> | --config-right-file <path>]
           [--no-http-defaults]
           [file/URL [file/URL]]
```

- Zero positionals → the plain GUI opens.
- One positional → single-tree view.
- Two positionals → diff view.

If the *Restore last-loaded paths on startup* option is enabled, containers the CLI doesn't address still restore their previous session's content — so `qtjsondiff URL` loads `URL` on the single-tree tab, and switching to the diff tab still shows the last diff you were on.

Each `--config*` flag pair configures how the URL for one container is requested (method, headers, body). You pass it in **one of two ways**:

- **Inline as text** — the value is a raw cURL command string: `--config "curl -X POST -H 'X-Foo: 1' --data-raw '{}'"`.
- **From a file** — the `-file` variant reads the same cURL text from disk: `--config-file ~/api-configs/prod.curl`.

Inline and `-file` for the same side are mutually exclusive. Each is only valid when its matching positional is given (`--config-left` needs two positionals, `--config` needs one, etc.).

**Default headers are appended automatically.** Any of `Accept-Language: en-US,en;q=0.8`, `Cache-Control: no-cache`, `Accept-Encoding: gzip, deflate` that the cURL you typed doesn't set is added by qtjsondiff — this preserves things like gzip decompression even for short CLI configs. If any of those clash with what you passed, yours wins. Add `--no-http-defaults` to send exactly and only the headers you typed.

The cURL text is exactly what you'd type in a shell, or what browser devtools produces under *Copy as cURL*. Examples:

```bash
# GET with custom headers — hit the GitHub API:
qtjsondiff 'https://api.github.com/users/hadley/orgs' \
           --config "curl -X GET -H 'X-Client: qtjsondiff'"

# POST with a JSON body — httpbin echoes the request back:
qtjsondiff 'https://httpbin.org/post' \
           --config "curl -X POST \
             -H 'Content-Type: application/json' \
             --data-raw '{\"user\":\"hadley\",\"action\":\"login\"}'"

# Diff two POSTs with different bodies against the same endpoint:
qtjsondiff \
  --config-left  "curl -X POST -H 'Content-Type: application/json' --data-raw '{\"user\":\"alice\",\"role\":\"admin\"}'" \
  --config-right "curl -X POST -H 'Content-Type: application/json' --data-raw '{\"user\":\"bob\",\"role\":\"user\"}'" \
  'https://httpbin.org/post' \
  'https://httpbin.org/post'

# File-loaded — reusable preset per side of the diff:
qtjsondiff --config-left-file  ~/api-configs/prod.curl \
           --config-right-file ~/api-configs/staging.curl \
           https://prod.example.com/api/foo \
           https://staging.example.com/api/foo
```

### Shell quoting reminder

Inside the outer `"…"` around a `--config*` value:

- `\"` is a literal `"` (use it inside JSON bodies).
- `'…'` passes through as-is — the natural quoting for `-H` and `--data-raw` values.
- Multi-line cURL with `\`-continuations is fine.

Copy-paste from browser devtools' *Copy as cURL* almost always Just Works — just wrap the whole cURL blob in the outer `"…"` and escape any bare `"` inside.

## Comparison modes

There are two ways the diff view can decide whether two items in your JSONs are "the same item":

### Full Path (default, fast)

Items are matched **only if they sit at the same place in the tree** — same chain of keys from the root, and same JSON type.

For example, given:

```json
left:  { "user": { "id": 42 } }
right: { "user": { "id": "42" } }
```

both `id` keys live at `user.id`, but the left one is a number and the right one is a string. Full Path treats them as **two different items** (one missing on each side) because the type is part of the path.

This is much faster for large or very different JSONs. Pick this one unless you have a reason not to.

### Parent+Child pair (slow, structure-flexible)

Items are matched if they share the **same parent key and the same own key**, no matter where they sit in the tree. Type is checked separately — a mismatched type counts as "found, but different" rather than "missing".

Using the same example above, Parent+Child pair would say "`user.id` exists on both sides, but its value/type differs" — it pairs them up and highlights the difference.

Useful when two JSONs describe the same data but are structured differently, or when you care about *values* more than *exact tree shape*. The trade-off is speed: every left-side item walks the entire right side looking for a match, so very large or very different JSONs can take a long time.

You can switch between modes at any time with the **compare-mode combobox** above the diff view.

### Smart Array

Positional modes (Full Path, Parent+Child) match array items by their index. A single missing item makes everything after it look different, even when the items are byte-for-byte identical. When **Smart Array** is on, arrays and objects are compared by content: items with the same content pair up regardless of position, and any item that exists on only one side shows up as a ghost row on the other side so matched pairs stay aligned row-for-row.

- **Arrays**: each item's content is used as its identity — two items pair up if their content signatures match. If you fill in the **Match key** field (`id`, or a comma-separated list like `id,eventId,marketId`), objects that carry any of those fields pair by that field's value first, which is useful when items are objects with a natural identifier.
- **Objects**: children pair by key at their own level; source order is preserved.
- **Missing items become ghost rows** on the opposite side, so row *N* on the left lines up with row *N* on the right for every matched pair. Sync-scroll and visual side-by-side reading work again on shifted arrays.

Example — the whole point of the feature:

```json
left:  { "arr": [ "A", "B", "C", "D" ] }
right: { "arr": [ "A", "B", "X", "C", "D" ] }
```

With Smart Array off, items are paired by index: `A` with `A`, `B` with `B`, `C` with `X`, `D` with `C`, and the right's `D` is left over — so three of the five items are flagged as different even though every letter appears on both sides. Turn **Smart Array** on and the pairing becomes A↔A, B↔B, C↔C, D↔D; the extra `X` is shown as a real item on the right and a ghost row on the left at the same visual position.

Smart Array is off by default (preserves prior positional behavior). Toggle it whenever the JSON is array-heavy or the arrays are known to be reordered.


### Supported Build Tags

You can trigger or control specific build jobs by including one or more of these tags in your commit message or pull request title/body (don't use backticks or it will break the build):

| Tag                 | Effect                                                       |
|---------------------|--------------------------------------------------------------|
| `[build mac dmg]`   | Builds a macOS DMG package                                   |
| `[build mac zip]`   | Builds a macOS ZIP package                                   |
| `[build mac]`       | Builds a macOS DMG package (alias for `[build mac dmg]`)     |
| `[build win]`       | Builds the Windows release                                   |
| `[build linux]`     | Builds the Linux release                                     |
| `[skip ci]`         | Skips all CI jobs for this commit or PR                      |

**Note:**  
If no build tags are present, the system will build **all platforms** by default.  
Use `[skip ci]` to intentionally skip the build and workflow runs.


## Special thanks to this projects:
    
    https://github.com/dridk/QJsonModel - I've used this model as basement

    https://github.com/probonopd/linuxdeployqt - nice tool to deploy application on linux
