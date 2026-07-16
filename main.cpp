/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include "commandlineparser.h"
#include "preferences/preferences.h"
#include <QApplication>
#include <QStyleFactory>
#include <QTranslator>
#include <QStandardPaths>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <QtDebug>

const QString APP_VERSION = "0.97";

void qtJsonDiffLogger(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    // Keep the QByteArray alive in a local - the `const char *` form
    // would dangle as soon as the full-expression ended, and fprintf()
    // below would strlen() freed memory.
    QByteArray timestamp = QDateTime::currentDateTime().toString(Qt::DateFormat::ISODate).toUtf8();
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
            timestamp.constData(), level, localMsg.constData(), file, context.line, function);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(qtJsonDiffLogger);

    QApplication a(argc, argv);

    const QString actualAppName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
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
            QFileInfo fileinfo(str + "/" + actualAppName + "_" + QLocale::system().name() + ".qm");
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

    // Apply persisted Qt style before any widgets get constructed so
    // every widget polishes against the right QStyle. Empty pref means
    // "leave Qt's default alone." setStyle silently no-ops on an
    // unknown key (e.g. a style the user uninstalled).
    if (!PREF_INST->appStyle.isEmpty())
        QApplication::setStyle(PREF_INST->appStyle);

    MainWindow w;
    // CLI options
    CommandLineParser cliParser;
    CliAppMode appMode = cliParser.parse();
    if (appMode == Exit)
        {
            return 0;
        }
    // Restore last-loaded paths for all three containers first, so
    // switching tabs still shows the previous session's content on
    // whichever side the CLI didn't override. setDisplayMode then
    // replaces just the CLI-targeted container(s).
    w.loadLastPathsIfEnabled();
    if (appMode == Tree || appMode == Diff)
        {
            w.setDisplayMode(cliParser.files(),
                             cliParser.configCurl(),
                             cliParser.leftConfigCurl(),
                             cliParser.rightConfigCurl(),
                             cliParser.noHttpDefaults());
        }

    w.setWindowTitle(a.property("appname").toString() + " " + a.property("appversion").toString());
    w.show();

    // Test/leak-check hook: if the env var is set, schedule a clean
    // quit after N ms. Lets the app be driven under valgrind without
    // needing manual GUI input. Set QTJSONDIFF_AUTOQUIT_MS=5000 (etc.)
    if (qEnvironmentVariableIsSet("QTJSONDIFF_AUTOQUIT_MS"))
    {
        bool ok = false;
        int ms = qEnvironmentVariable("QTJSONDIFF_AUTOQUIT_MS").toInt(&ok);
        if (ok && ms > 0)
        {
            QTimer::singleShot(ms, &a, &QCoreApplication::quit);
        }
    }

    return a.exec();
}
