#include "preferences.h"

#define DEF_IDENTICAL_DIFF_COLOR QColor(0, 100, 0, 150)
#define DEF_MODERATE_DIFF_COLOR QColor(Qt::yellow)
#define DEF_HUGE_DIFF_COLOR QColor(Qt::red)
#define DEF_NOT_PRESENT_DIFF_COLOR QColor(Qt::lightGray)

#define DEF_DIFF_ALPHA 75

#define DEF_SYNTAX_KW_COLOR QColor(Qt::blue)
#define DEF_SYNTAX_VAL_COLOR QColor(Qt::green)


Preferences * Preferences::m_instance = nullptr;


Preferences * Preferences::Instance()
{
    if (!m_instance)
        m_instance = new Preferences();
    return m_instance;
}

Preferences::Preferences()
{
    load();
}

void Preferences::load()
{
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
}

void Preferences::save()
{
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
}

void Preferences::restoreDefaults()
{
    identicalDiffColor = DEF_IDENTICAL_DIFF_COLOR;
    moderateDiffColor = DEF_MODERATE_DIFF_COLOR;
    hugeDiffColor = DEF_HUGE_DIFF_COLOR;
    notPresentDiffColor = DEF_NOT_PRESENT_DIFF_COLOR;

    diffColorsAlpha = DEF_DIFF_ALPHA;

    syntaxKeywordColor = DEF_SYNTAX_KW_COLOR;
    syntaxValueColor = DEF_SYNTAX_VAL_COLOR;
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


