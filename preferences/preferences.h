#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QSettings>
#include <QColor>

#define P Preferences::Instance()

enum DiffColorType
{
    None,
    Identical,
    Moderate,
    Huge,
    NotPresent
};

class Preferences
{
public:
    static Preferences *Instance();
    ~Preferences();

    void load();
    void save();
    void restoreDefaults();

    int activeTabIndex;
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

private:
    explicit Preferences();
    Preferences(const Preferences &) = delete;
    Preferences &operator=(const Preferences &) = delete;

    static Preferences *m_instance;

    QSettings s;
};

#endif // PREFERENCES_H
