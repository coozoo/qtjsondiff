#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QObject>
#include <QSettings>
#include <QColor>
#include <QTabWidget>
#include <QKeySequence>
#include <QMap>

#define PREF_INST Preferences::Instance()

enum DiffColorType
{
    None,
    Identical,
    Moderate,
    Huge,
    NotPresent
};

struct ShortcutInfo {
    QString key;
    QString description;
    QKeySequence defaultSequence;
};

class Preferences : public QObject
{
    Q_OBJECT

public:
    static Preferences *Instance();
    ~Preferences();

    void load();
    void save();
    void restoreColorDefaults();
    void restoreShortcutDefaults();

    int activeTabIndex;
    int tabsPosition;
    int showJsonButtonPosition;
    bool restoreOnStart;

    QByteArray mainWindowGeometry;
    QByteArray mainWindowState;

    QString jsonContainerPath;
    QString differLeftPath;
    QString differRightPath;

    QColor identicalDiffColor;
    QColor moderateDiffColor;
    QColor hugeDiffColor;
    QColor notPresentDiffColor;
    int diffColorsAlpha;

    QColor diffColor(DiffColorType colorType);

    QColor syntaxKeywordColor;
    QColor syntaxValueColor;

    // Inline-editing toggles. Off by default — integrators that want the
    // original read-only widget contract see no behavior change. The
    // demo app's MainWindow reads these on startup and re-reads them
    // whenever editModeChanged fires (live update from the dialog).
    bool editableSingleTree;
    bool editableDiffView;

    QMap<QString, QKeySequence> shortcuts;
    static const QList<ShortcutInfo> shortcutInfos;

signals:
    void shortcutsUpdated();
    void editModeChanged();

private:
    explicit Preferences(QObject *parent = nullptr);
    Preferences(const Preferences &) = delete;
    Preferences &operator=(const Preferences &) = delete;

    static Preferences *m_instance;

    QSettings s;
};

#endif // PREFERENCES_H
