# qtjsondiff

## What this is

Qt6 (also builds on Qt5) widget-first project. Two reusable widgets —
`QJsonContainer` (single JSON tree view) and `QJsonDiff` (side-by-side
diff of two trees) — packaged inside a demo app (`MainWindow`) that
doubles as a useful standalone tool.

**The widgets are the product, the app is the showcase.** Anything new
that lives in the app must stay configurable on the widget so external
integrators can opt in. See `README.md` → "Integrate it to your
application" — that integration recipe is part of the public contract.

## Architecture

```
QJsonTreeItem      qjsonitem.{h,cpp}      Tree node. Keeps key/type/value,
                                          children, diff color, sibling
                                          relation index. No Qt model logic.

QJsonModel         qjsonmodel.{h,cpp}     QAbstractItemModel over a
                                          QJsonTreeItem root. load(),
                                          loadJson(), setData(), flags(),
                                          getJsonDocument(), setEditable().

JsonItemDelegate   jsonitemdelegate.{h,cpp}  QComboBox editor for the
                                            "Type" column (col 1).

QJsonContainer     qjsoncontainer.{h,cpp} QWidget: tree + toolbar +
                                          plaintext view + search +
                                          browse/URL + context menu +
                                          paste. Wraps QJsonModel.

QJsonDiff          qjsondiff.{h,cpp}      QWidget: hosts two
                                          QJsonContainers, "Compare"
                                          button, sync-scroll, "Use Full
                                          Path" mode toggle. Owns the
                                          comparison algorithm.

MainWindow         mainwindow.{h,cpp,ui}  Demo shell, tab widget, menus,
                                          preferences dialog.
```

`DiffColorType` lives in `preferences/preferences.h`:
`None / Identical / Moderate / Huge / NotPresent`. Per-item color is
stored on `QJsonTreeItem`; the actual `QColor` comes from
`PREF_INST->diffColor(type)` so themes are user-configurable.

`PREF_INST` is a `Preferences` singleton (`preferences/preferences.h`)
backed by `QSettings`. Holds colors, shortcuts, last-loaded paths,
window geometry.

## Edit mode

Edit mode was added on branch `edit`. It is off by default to preserve
the widget's prior read-only behaviour.

- `QJsonModel::setEditable(bool)` is the master switch.
  `MainWindow` calls `messageJsonCont->setEditable(true)` on the
  single-tree tab. The diff tab containers stay read-only.
- `flags()` gates per-cell editability:
  - Column 0 (key): editable for non-root, non-array children.
  - Column 1 (type): editable for any non-root item.
  - Column 2 (value): editable for scalars only (not Object/Array/Null).
- `setData(col 1)` performs structural type conversion, including four
  Object↔Array conversion shapes:
  - `[["k","v"]]` ↔ `{"k":"v"}`
  - `["a","b"]` ↔ `{"0":"a","1":"b"}`
  - sequential keys → array, mixed keys → key-value pair arrays
- `setData(col 0/2)` are simple key/value writes.
- The combo-box for the type column is provided by `JsonItemDelegate`.

Integrators opt in with one call: `container->setEditable(true);`.
Don't add edit-only public API that breaks the read-only contract.

## Build — always shadow build (Qt Creator pattern)

**Never build in the source tree.** Use shadow builds so artefacts
land under `build/<KitName>-<Config>/`. Mirrors the Qt Creator
default and keeps the source dir clean.

```bash
mkdir -p build/Desktopqt6-Debug
cd build/Desktopqt6-Debug
qmake6 ../../qtjsondiff.pro CONFIG+=debug   # or `qmake` if Qt5
make -j$(nproc)
./qtjsondiff
```

Release build: replace `Desktopqt6-Debug` with `Desktopqt6-Release`
and drop `CONFIG+=debug`.

Translations (`translations/*.ts`) are compiled via `lrelease` as part
of the build (`CONFIG += lrelease`). Release builds suppress
`qDebug()` via `QT_NO_DEBUG_OUTPUT`.

`build/` and the usual qmake droppings (`*.o`, `moc_*.cpp`, `Makefile*`,
…) are in `.gitignore`.

## Tests — same shadow-build convention

From the shadow build dir:

```bash
cd build/Desktopqt6-Debug
make check         # builds tests under tests-shadow/ and runs them
```

`make check` is defined in `qtjsondiff.pro` and:
1. Creates `$OUT_PWD/tests-shadow/` (a subdir of the current build dir).
2. Runs `qmake6 <src>/tests/tests.pro` in there.
3. Builds and runs both test binaries under `QT_QPA_PLATFORM=offscreen`.

To build tests by hand into their own shadow dir instead:

```bash
mkdir -p build/Desktopqt6-Debug-tests
cd build/Desktopqt6-Debug-tests
qmake6 ../../tests/tests.pro CONFIG+=debug
make -j$(nproc)
QT_QPA_PLATFORM=offscreen ./conversions/tst_json_conversions
QT_QPA_PLATFORM=offscreen ./compare/tst_compare
```

Tests use Qt Test (`QT += testlib`). Compare tests need
`QT_QPA_PLATFORM=offscreen` because they instantiate `QJsonDiff` (a
QWidget). When adding a test source under `tests/<name>/`, list every
`../../` source it pulls in from the project root inside
`tests/<name>/<name>.pro` — there is no shared static lib.

## Conventions

- C++17, `CONFIG += c++17`.
- Allman braces, K&R-ish (`CONTRIBUTING.md`).
- Header guards `*_H`.
- Minimal comments: only the non-obvious *why*; no narration of
  *what*.
- Logging: `qDebug()` only.
- Signals/slots: prefer the function-pointer `connect()` form for new
  code (existing code mixes `SIGNAL`/`SLOT` macros — leave them).

## Qt 5 / Qt 6 compatibility

The project targets Qt 6 but should still build on the latest Qt 5
(README explicitly mentions Qt 5 builds). **Do not use Qt 6-only APIs
in new code unless guarded by version ifdefs.** Common pitfalls:

- `QString::SkipEmptyParts` (Qt 5) vs `Qt::SkipEmptyParts` (Qt 6) —
  prefer `Qt::SkipEmptyParts` and guard if you need to touch this in
  code that must build on Qt 5.
- `QCheckBox::checkStateChanged` is Qt 6.7+; use the deprecated-but-
  still-present `stateChanged(int)` (the existing code already does).
- `QMouseEvent::pos()`/`globalPos()` semantics changed across the
  boundary — prefer `position()` / `globalPosition().toPoint()` only
  if you also `#ifdef` for Qt 5.
- `QStringView`, `QStringTokenizer` etc. are fine on both.
- Function-pointer `connect()` works the same on both since 5.0.

Pattern for version-guarded code:

```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 path
#else
    // Qt 5 path
#endif
```

If a change requires Qt 6-only behaviour and a fallback isn't
practical, raise it before introducing the dependency.

## Things to be careful about

- **Public API is additive only.** Don't rename signals/slots, don't
  remove members of `QJsonContainer` / `QJsonDiff` / `QJsonModel`.
  Integrators call these directly from their own `MainWindow`-like
  code (per the README recipe).
- **README integration section is the contract.** If you have to
  change the public API, update `README.md` in the same commit.
- **Don't make edit features mandatory.** Keep `setEditable(false)`
  the default; the diff view stays read-only.
- **Compare algorithm is being refactored** into a separate,
  threadable class with progress reporting. New tests in
  `tests/test_compare.cpp` are deliberately black-box (assert on
  `colorType()` / `idxRelation()` only) so they survive that
  refactor. Don't tighten them to internal details.

## CI

`.github/workflows/build_all.yml` reacts to commit-message tags:

| Tag             | Effect                                  |
|-----------------|-----------------------------------------|
| `[build mac]`   | macOS DMG package                       |
| `[build win]`   | Windows release                         |
| `[build linux]` | Linux release                           |
| `[skip ci]`     | Skip all CI                             |

No tag → all platforms build.

## File map (quick)

- `qjsonitem.{h,cpp}` — tree node
- `qjsonmodel.{h,cpp}` — QAbstractItemModel
- `qjsoncontainer.{h,cpp}` — single-tree widget
- `qjsondiff.{h,cpp}` — diff widget
- `jsonitemdelegate.{h,cpp}` — type combo editor
- `jsonsyntaxhighlighter.{h,cpp}` — plaintext view highlighter
- `commandlineparser.{h,cpp}` — CLI entry
- `mainwindow.{h,cpp,ui}` — demo shell
- `preferences/` — singleton, dialog, shortcut delegate, defaults
- `tests/` — Qt Test binaries
- `images/`, `qss/`, `translations/` — assets
