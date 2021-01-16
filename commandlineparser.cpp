#include "commandlineparser.h"
#include <QApplication>
#include <QStyleFactory>
#include <QtDebug>
#include <QCommandLineParser>


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
    QCommandLineOption styleOption(QStringList() << "style",
                QCoreApplication::translate("main", QString(tr("overwrite application theme style <style>. \n Available styles: \n") + styles_Str).toUtf8()),
                                   QCoreApplication::translate("main", "style"));
    parser.addOption(styleOption);
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
