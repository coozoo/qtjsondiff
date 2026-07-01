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
* [Comparison modes](#comparison-modes)


## Summary

Some kind of diff viewer for Json (based on a tree-like json container/viewer widget).

Actually, I've created this widget for myself. As a tester, I often need to compare JSONs from different sources or simply need a handy viewer that can work with really big JSONs.

Usually, online viewers simply crash and hang with such data. This viewer is still able to work with such big JSONs.

For example, I'm using this in my SignalR and cometD clients to visualize responses or simply to show and compare HTTP responses from other sources. And I've found this example app pretty handy as well.

Some features:

    - **Dual View Modes**: Switch between a formatted JSON text view and an interactive tree view.
    - **Data Loading**: Load JSON from a file, a URL, or by pasting directly into the tree view (Ctrl+V).
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

#include "preferences/preferences.h"
#include "preferences/preferencesdialog.h"

```
If you need just json tree view you can ignore `qjsondiff.h` and `jsondiffengine.h`.

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

If you want to run a comparison without the widget (for example in a CLI tool or a background job), you can use `JsonDiffEngine` directly:

```cpp
#include "jsondiffengine.h"
#include "qjsonmodel.h"

// take a snapshot of each model
DiffNode left  = JsonDiffEngine::snapshot(leftModel);
DiffNode right = JsonDiffEngine::snapshot(rightModel);

// run the compare (Mode::FullPath or Mode::ParentChildPair)
JsonDiffEngine::compare(left, right, JsonDiffEngine::Mode::FullPath);

// apply the result back onto the models so the tree views show colors
JsonDiffEngine::apply(left,  leftModel,  rightModel);
JsonDiffEngine::apply(right, rightModel, leftModel);
```

That's all you need for a basic compare. `QJsonDiff` does the same thing internally on a background thread with a progress dialog.


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

You can switch between modes at any time with the **Use Full Path** checkbox above the diff view.


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
