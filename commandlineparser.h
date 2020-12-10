#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include <QObject>

/**
 * @brief The CliAppMode enum
 *
 * Logical return value from the CommanLineParser::parse()
 *
 */
enum CliAppMode
{
    // Sign that app should exit without its GUI
    Exit,
    // Display standard app, no CLI specified files will be loaded
    Gui,
    // Single file given, show json tree
    Tree,
    // 2 files given, show diff pane
    Diff
};


/**
 * @brief Custom CLI arguments handling for QTjsonDiff
 *
 * Currently it just supports help, version and
 * 1-2 file path(s) to setup the gui.
 */
class CommandLineParser: public QObject
{
    Q_OBJECT
public:
    CommandLineParser(QObject *parent = 0);

    CliAppMode parse();
    //! Files given from CLI
    QStringList files() { return mArgs; }

private:
    QStringList mArgs;
};

#endif // COMMANDLINEPARSER_H
