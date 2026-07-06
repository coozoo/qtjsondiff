#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include <QObject>
#include <QStringList>

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
 * Supports help, version, 1-2 file path(s), and optional
 * per-position HTTP request configs. Each --config* flag names a
 * text file whose contents is a cURL command; the request is
 * applied to the matching QJsonContainer before its URL is loaded.
 * A --config-left / --config-right flag is only valid if the
 * corresponding positional file was given.
 */
class CommandLineParser: public QObject
{
    Q_OBJECT
public:
    CommandLineParser(QObject *parent = 0);

    CliAppMode parse();
    //! Files given from CLI
    QStringList files() { return mArgs; }

    //! Resolved cURL text for each container position. Empty when
    //! neither the inline (--config) nor the file (--config-file)
    //! variant was set for that side. The parser has already read
    //! the file (if a --config-file* flag was used) so MainWindow
    //! doesn't need to touch the disk.
    QString configCurl()      const { return mConfigCurl; }
    QString leftConfigCurl()  const { return mLeftConfigCurl; }
    QString rightConfigCurl() const { return mRightConfigCurl; }

    //! When false (the default), missing built-in default headers
    //! (Accept-Language / Cache-Control / Accept-Encoding) are
    //! appended to whatever the --config* text provides. Set with
    //! --no-http-defaults to send exactly and only what you typed.
    bool noHttpDefaults() const { return mNoHttpDefaults; }

private:
    QStringList mArgs;
    QString     mConfigCurl;
    QString     mLeftConfigCurl;
    QString     mRightConfigCurl;
    bool        mNoHttpDefaults = false;
};

#endif // COMMANDLINEPARSER_H
