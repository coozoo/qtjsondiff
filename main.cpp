/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include "commandlineparser.h"
#include <QApplication>
#include <QTranslator>
#include <QStandardPaths>
#include <QFileInfo>
#include <QSettings>
#include <QtDebug>

const QString APP_VERSION = "0.7b";

void qtJsonDiffLogger(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    const char *timestamp = QDateTime::currentDateTime().toString(Qt::DateFormat::ISODate).toUtf8();
    const char *level = "n/a";

    switch (type)
        {
        case QtDebugMsg:
            level = "DEBUG";
            break;
        case QtInfoMsg:
            level = "INFO";
            break;
        case QtWarningMsg:
            level = "WARNING";
            break;
        case QtCriticalMsg:
            level = "CRITICAL";
            break;
        case QtFatalMsg:
            level = "FATAL";
            break;
        }

    fprintf(stderr, "%s [%s] %s (%s:%u, %s)\n",
            timestamp, level, localMsg.constData(), file, context.line, function);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(qtJsonDiffLogger);

    QApplication a(argc, argv);

    QLoggingCategory::setFilterRules("*.debug=true\nqt.*.debug=false");

    qInfo() << QLocale::system().name();
    QStringList translations;
    QDir dir(a.applicationDirPath());
    if (dir.cdUp() && dir.cd("share"))
        {
            translations.append(dir.absolutePath() + "/" + a.applicationName());
        }
    translations.append(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation));
    translations.append(QCoreApplication::applicationDirPath());
    translations.append(a.applicationDirPath() + "/.qm");
    translations.append(a.applicationDirPath() + "/lang");
    QString translationFilePath = "";
    qInfo() << "Search for translations";
    foreach (const QString &str, translations)
        {
            QFileInfo fileinfo(str + "/" + a.applicationName() + "_" + QLocale::system().name() + ".qm");
            qInfo() << fileinfo.filePath();
            if (fileinfo.exists() && fileinfo.isFile())
                {
                    translationFilePath = fileinfo.filePath();
                    qInfo() << "Translation found in: " + translationFilePath;
                    break;
                }
        }

    QTranslator translator;
    qInfo() << translator.load(translationFilePath);
    a.installTranslator(&translator);

    QString platform = "";
#if __GNUC__
#if __x86_64__
    platform = "-64bit";
#endif
#endif
    a.setProperty("appversion", APP_VERSION + platform + " (QTbuild:" + QString(QT_VERSION_STR) + ")");
    a.setProperty("appname", "QT JSON Diff");

#ifdef Q_OS_LINUX
    a.setWindowIcon(QIcon(":/images/qtjsondiff.png"));
#endif

    QString settingsDir = "";
#ifdef Q_OS_WIN
    settingsDir = qApp->applicationDirPath();
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    settingsDir = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation)[0] + "/" + qAppName();
#endif
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    QApplication::setOrganizationName("ini");
    QApplication::setApplicationName("qtjsondiff");
    QApplication::setApplicationVersion(APP_VERSION);

    MainWindow w;
    // CLI options
    CommandLineParser cliParser;
    CliAppMode appMode = cliParser.parse();
    if (appMode == Exit)
        {
            return 0;
        }
    if (appMode == Tree || appMode == Diff)
        {
            w.setDisplayMode(cliParser.files());
        }

    w.setWindowTitle(a.property("appname").toString() + " " + a.property("appversion").toString());
    w.show();

    return a.exec();
}
