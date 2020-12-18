#include "preferences.h"

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
    restoreOnStart = s.value("Saved_Paths/restore_on_start", false).toBool();

    mainWindowGeometry = s.value("MainWindow/geometry").toByteArray();
    mainWindowState = s.value("MainWindow/windowState").toByteArray();

    jsonContainerPath = s.value("Saved_Paths/json_container_path").toString();
    differLeftPath = s.value("Saved_Paths/differ_left_path").toString();
    differRightPath = s.value("Saved_Paths/differ_right_path").toString();

    identicalDiffColor = s.value("identical_diff_color", QColor(0, 100, 0, 150)).value<QColor>();
    moderateDiffColor = s.value("moderate_diff_color", QColor(Qt::yellow)).value<QColor>();
    hugeDiffColor = s.value("huge_diff_color", QColor(Qt::red)).value<QColor>();
    notPresentDiffColor = s.value("not_present_diff_color", QColor(Qt::lightGray)).value<QColor>();
    diffColorsAlpha = s.value("diff_colors_alpha", 75).toInt();

    syntaxKeywordColor = s.value("syntax_keyword_color", QColor(Qt::blue)).value<QColor>();
    syntaxValueColor = s.value("syntax_value_color", QColor(Qt::green)).value<QColor>();

}

void Preferences::save()
{
    s.setValue("active_tab_index", activeTabIndex);
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
        c.setAlpha(diffColorsAlpha);
    }

    return c;
}
