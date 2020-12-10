#include "commandlineparser.h"
#include <QtDebug>
#include <QCommandLineParser>


CommandLineParser::CommandLineParser(QObject *parent): QObject(parent)
{
}

CliAppMode CommandLineParser::parse()
{
    QCommandLineParser parser;
    parser.setApplicationDescription(tr("JSON files display and diff tool."));
    QCommandLineOption helpOption = parser.addHelpOption();
    QCommandLineOption versionOption = parser.addVersionOption();
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
    switch (mArgs.size()) {
        case 0: return Gui;
        case 1: return Tree;
        case 2: return Diff;
        default: {
            qInfo() << tr("Wrong count of arguments used. See help:");
            parser.showHelp(1);
        }
    }
}
