#include "commandlineparser.h"
#include <QApplication>
#include <QStyleFactory>
#include <QtDebug>
#include <QCommandLineParser>
#include <QFile>


CommandLineParser::CommandLineParser(QObject *parent): QObject(parent)
{
}

CliAppMode CommandLineParser::parse()
{
    QStringList styles_list=QStyleFactory::keys();
    QString styles_Str="";
    for (QStringList::iterator it = styles_list.begin();it != styles_list.end(); ++it)
    {
            styles_Str.append(*it +"\n");
    }
    QCommandLineParser parser;
    parser.setApplicationDescription(tr("JSON files display and diff tool."));
    QCommandLineOption helpOption = parser.addHelpOption();
    QCommandLineOption versionOption = parser.addVersionOption();
    //it will not overwrite default qt argument. I will just list them
    // Description: translate the English template once via tr(), then
    // append the runtime list of style names. Don't nest translate() -
    // lupdate can only extract string LITERALS, not expressions, so the
    // outer translate("main", QString(...).toUtf8()) form was being
    // silently dropped (and the inner tr() ran twice for no benefit).
    QCommandLineOption styleOption(QStringList() << "style",
                                   tr("overwrite application theme style <style>. \nAvailable styles: \n") + styles_Str,
                                   tr("style"));
    parser.addOption(styleOption);

    // HTTP request configuration. Two variants per side:
    //   --config       <curl-text>  inline cURL command as a string
    //   --config-file  <path>       path to a file with the same
    // They're mutually exclusive per side - using both is an error.
    // Each is valid only when the matching positional file/URL is
    // given, per the "extends the positional" contract.
    QCommandLineOption configOption(
        QStringList() << "config",
        tr("Inline cURL command describing method/headers/body for "
           "the single-tree URL. E.g. --config \"curl -X POST -H "
           "'X-Foo: 1' --data-raw '{}'\". Valid only when a "
           "positional file/URL is given."),
        tr("curl-text"));
    QCommandLineOption configFileOption(
        QStringList() << "config-file",
        tr("Same as --config but reads the cURL command from a file."),
        tr("path"));
    QCommandLineOption leftConfigOption(
        QStringList() << "config-left",
        tr("Inline cURL command for the LEFT diff container. Valid "
           "only when the left positional file/URL is given."),
        tr("curl-text"));
    QCommandLineOption leftConfigFileOption(
        QStringList() << "config-left-file",
        tr("Same as --config-left but from a file."),
        tr("path"));
    QCommandLineOption rightConfigOption(
        QStringList() << "config-right",
        tr("Inline cURL command for the RIGHT diff container. Valid "
           "only when the right positional file/URL is given."),
        tr("curl-text"));
    QCommandLineOption rightConfigFileOption(
        QStringList() << "config-right-file",
        tr("Same as --config-right but from a file."),
        tr("path"));
    parser.addOption(configOption);
    parser.addOption(configFileOption);
    parser.addOption(leftConfigOption);
    parser.addOption(leftConfigFileOption);
    parser.addOption(rightConfigOption);
    parser.addOption(rightConfigFileOption);

    QCommandLineOption noHttpDefaultsOption(
        QStringList() << "no-http-defaults",
        tr("With --config* set, don't append the built-in default "
           "headers (Accept-Language, Cache-Control, Accept-Encoding) "
           "to the request. Send exactly and only the headers you "
           "typed."));
    parser.addOption(noHttpDefaultsOption);

    parser.addPositionalArgument("files",
                                 tr("File(s) to open. One file for tree view, " \
                                    "two files for the diff. No file opens plain gui."),
                                 "[file, [file]]");

    if (!parser.parse(qApp->arguments())) {
        qCritical() << parser.errorText();
        parser.showHelp(1);
    }

    if (parser.isSet(helpOption)) {
        parser.showHelp(0);
    }
    else if (parser.isSet(versionOption)) {
        parser.showVersion();
        return Exit;
    }

    mArgs = parser.positionalArguments();

    // Resolve each side: pick inline if given, otherwise read the
    // file variant, otherwise leave empty. Both set at once is an
    // error - the user might otherwise wonder which one won.
    auto resolveConfig = [&](const QString &inlineFlag,
                             const QCommandLineOption &inlineOpt,
                             const QString &fileFlag,
                             const QCommandLineOption &fileOpt) -> QString
    {
        const bool hasInline = parser.isSet(inlineOpt);
        const bool hasFile   = parser.isSet(fileOpt);
        if (hasInline && hasFile) {
            qCritical() << tr("--%1 and --%2 are mutually exclusive; "
                              "pass only one.").arg(inlineFlag, fileFlag);
            parser.showHelp(1);
        }
        if (hasInline) return parser.value(inlineOpt);
        if (hasFile) {
            const QString path = parser.value(fileOpt);
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qCritical() << tr("--%1: could not open %2: %3")
                                   .arg(fileFlag, path, f.errorString());
                parser.showHelp(1);
            }
            return QString::fromUtf8(f.readAll());
        }
        return QString();
    };

    mNoHttpDefaults = parser.isSet(noHttpDefaultsOption);
    mConfigCurl      = resolveConfig("config",       configOption,
                                     "config-file",  configFileOption);
    mLeftConfigCurl  = resolveConfig("config-left",  leftConfigOption,
                                     "config-left-file",
                                                     leftConfigFileOption);
    mRightConfigCurl = resolveConfig("config-right", rightConfigOption,
                                     "config-right-file",
                                                     rightConfigFileOption);

    // Validate: config* is only meaningful when its positional is
    // also given (per the user's "valid only after this one exists"
    // rule). Reject up front so bad invocations don't silently drop
    // the config the user passed in.
    auto rejectStrayConfig = [&](const QString &flag) {
        qCritical() << tr("--%1 (or its -file variant) requires the "
                          "matching positional file/URL to be given "
                          "as well.").arg(flag);
        parser.showHelp(1);
    };
    switch (mArgs.size()) {
        case 0:
            if (!mConfigCurl.isEmpty())      rejectStrayConfig("config");
            if (!mLeftConfigCurl.isEmpty())  rejectStrayConfig("config-left");
            if (!mRightConfigCurl.isEmpty()) rejectStrayConfig("config-right");
            return Gui;
        case 1:
            if (!mLeftConfigCurl.isEmpty())  rejectStrayConfig("config-left");
            if (!mRightConfigCurl.isEmpty()) rejectStrayConfig("config-right");
            return Tree;
        case 2:
            if (!mConfigCurl.isEmpty()) {
                qCritical() << tr("--config is for tree mode; use --config-left "
                                  "and/or --config-right in diff mode.");
                parser.showHelp(1);
            }
            return Diff;
        default: {
            qInfo() << tr("Wrong count of arguments used. See help:");
            parser.showHelp(1);
        }
    }
}
