#include "preferences.h"
#include <QDebug>

#define DEF_IDENTICAL_DIFF_COLOR QColor(0, 100, 0, 150)
#define DEF_MODERATE_DIFF_COLOR QColor(Qt::yellow)
#define DEF_HUGE_DIFF_COLOR QColor(Qt::red)
#define DEF_NOT_PRESENT_DIFF_COLOR QColor(Qt::lightGray)

#define DEF_DIFF_ALPHA 75

#define DEF_SYNTAX_KW_COLOR QColor(Qt::blue)
#define DEF_SYNTAX_VAL_COLOR QColor(Qt::green)


const QList<ShortcutInfo> Preferences::shortcutInfos = {
    {"copy_row",                 "Copy Row",                       QKeySequence("Ctrl+Alt+1")},
    {"copy_all_rows",            "Copy all Rows",                  QKeySequence("Ctrl+Alt+2")},
    {"copy_path",                "Copy Path",                      QKeySequence("Ctrl+Alt+3")},
    {"copy_jq_path",             "Copy jq Path",                   QKeySequence("Ctrl+Alt+4")},
    {"copy_json_plain_text",     "Copy Plain JSON",                QKeySequence("Ctrl+Alt+5")},
    {"copy_json_pretty_text",    "Copy Pretty JSON",               QKeySequence("Ctrl+Alt+6")},
    {"copy_selected_plain",      "Copy Plain selected JSON value", QKeySequence("Ctrl+Alt+7")},
    {"copy_selected_pretty",     "Copy Pretty selected JSON",      QKeySequence("Ctrl+Alt+8")}
};

Preferences * Preferences::m_instance = nullptr;


Preferences * Preferences::Instance()
{
    if (!m_instance)
        m_instance = new Preferences();
    return m_instance;
}

Preferences::Preferences(QObject *parent) : QObject(parent)
{
    load();
}

Preferences::~Preferences() = default;

void Preferences::load()
{
    qDebug() << "--- Preferences::load() START ---";
    activeTabIndex = s.value("active_tab_index", 0).toInt();
    tabsPosition = s.value("tabs_position",QTabWidget::East).toInt();
    showJsonButtonPosition= s.value("show_json_button_position",-2).toInt();
    restoreOnStart = s.value("Saved_Paths/restore_on_start", false).toBool();

    mainWindowGeometry = s.value("MainWindow/geometry").toByteArray();
    mainWindowState = s.value("MainWindow/windowState").toByteArray();

    jsonContainerPath = s.value("Saved_Paths/json_container_path").toString();
    differLeftPath = s.value("Saved_Paths/differ_left_path").toString();
    differRightPath = s.value("Saved_Paths/differ_right_path").toString();

    identicalDiffColor = s.value("identical_diff_color",DEF_IDENTICAL_DIFF_COLOR).value<QColor>();
    moderateDiffColor = s.value("moderate_diff_color", DEF_MODERATE_DIFF_COLOR).value<QColor>();
    hugeDiffColor = s.value("huge_diff_color", DEF_HUGE_DIFF_COLOR).value<QColor>();
    notPresentDiffColor = s.value("not_present_diff_color", DEF_NOT_PRESENT_DIFF_COLOR).value<QColor>();
    diffColorsAlpha = s.value("diff_colors_alpha", DEF_DIFF_ALPHA).toInt();

    syntaxKeywordColor = s.value("syntax_keyword_color", DEF_SYNTAX_KW_COLOR).value<QColor>();
    syntaxValueColor = s.value("syntax_value_color", DEF_SYNTAX_VAL_COLOR).value<QColor>();

    editableSingleTree = s.value("Edit/single_tree_editable", false).toBool();
    editableDiffView   = s.value("Edit/diff_view_editable",   false).toBool();

    appStyle      = s.value("Style/app_style").toString();
    useStyledTree = s.value("Style/use_styled_tree", false).toBool();

    matchKey     = s.value("Compare/match_key",     QString()).toString();
    arrayOverlay = s.value("Compare/array_overlay", false).toBool();
    // 0 = FullPath (default), 1 = ParentChildPair.
    compareMode  = s.value("Compare/mode", 0).toInt();

    bool needToSaveDefaults = !s.contains("Shortcuts/copy_row");

    for (const auto &info : shortcutInfos) {
        QVariant loadedShortcut = s.value("Shortcuts/" + info.key, QVariant::fromValue(info.defaultSequence));
        shortcuts[info.key] = loadedShortcut.value<QKeySequence>();
        qDebug() << "Loaded shortcut for" << info.key << ":" << shortcuts[info.key].toString();
    }

    if (needToSaveDefaults) {
        qDebug() << "Shortcuts not found in settings, saving defaults.";
        save();
    }
    qDebug() << "--- Preferences::load() END ---";
}

void Preferences::save()
{
    qDebug() << "--- Preferences::save() ---";
    s.setValue("active_tab_index", activeTabIndex);
    s.setValue("tabs_position",tabsPosition);
    s.setValue("show_json_button_position",showJsonButtonPosition);
    s.setValue("Saved_Paths/restore_on_start", restoreOnStart);

    s.setValue("MainWindow/geometry", mainWindowGeometry);
    s.setValue("MainWindow/windowState", mainWindowState);

    s.setValue("Saved_Paths/json_container_path", jsonContainerPath);
    s.setValue("Saved_Paths/differ_left_path", differLeftPath);
    s.setValue("Saved_Paths/differ_right_path", differRightPath);

    s.setValue("identical_diff_color", identicalDiffColor);
    s.setValue("moderate_diff_color", moderateDiffColor);
    s.setValue("huge_diff_color", hugeDiffColor);
    s.setValue("not_present_diff_color", notPresentDiffColor);
    s.setValue("diff_colors_alpha", diffColorsAlpha);

    s.setValue("syntax_keyword_color", syntaxKeywordColor);
    s.setValue("syntax_value_color", syntaxValueColor);

    s.setValue("Edit/single_tree_editable", editableSingleTree);
    s.setValue("Edit/diff_view_editable",   editableDiffView);

    s.setValue("Style/app_style",        appStyle);
    s.setValue("Style/use_styled_tree",  useStyledTree);

    s.setValue("Compare/match_key",     matchKey);
    s.setValue("Compare/mode",          compareMode);
    s.setValue("Compare/array_overlay", arrayOverlay);

    for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it) {
        s.setValue("Shortcuts/" + it.key(), it.value());
        qDebug() << "Saving shortcut for" << it.key() << ":" << it.value().toString();
    }

    emit shortcutsUpdated();
}

void Preferences::restoreColorDefaults()
{
    qDebug() << "--- Preferences::restoreColorDefaults() ---";
    identicalDiffColor = DEF_IDENTICAL_DIFF_COLOR;
    moderateDiffColor = DEF_MODERATE_DIFF_COLOR;
    hugeDiffColor = DEF_HUGE_DIFF_COLOR;
    notPresentDiffColor = DEF_NOT_PRESENT_DIFF_COLOR;

    diffColorsAlpha = DEF_DIFF_ALPHA;

    syntaxKeywordColor = DEF_SYNTAX_KW_COLOR;
    syntaxValueColor = DEF_SYNTAX_VAL_COLOR;
    save();
}

void Preferences::restoreShortcutDefaults()
{
    qDebug() << "--- Preferences::restoreShortcutDefaults() ---";
    for (const auto &info : shortcutInfos) {
        shortcuts[info.key] = info.defaultSequence;
    }
    save();
}

QColor Preferences::diffColor(DiffColorType colorType)
{
    QColor c;

    switch (colorType) {
    case Identical:
        c = identicalDiffColor;
        break;
    case Moderate:
        c = moderateDiffColor;
        break;
    case Huge:
        c = hugeDiffColor;
        break;
    case NotPresent:
        c = notPresentDiffColor;
        break;
    default:
        break;
    }

    if (c.isValid() && c.alpha() != diffColorsAlpha) {
        c.setAlpha((100 - diffColorsAlpha) * 255 / 100);
    }

    return c;
}


