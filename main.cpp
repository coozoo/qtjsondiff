/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QStandardPaths>
#include <QFileInfo>

const QString APP_VERSION="0.35b";

int main(int argc, char *argv[])
{
    QTextStream cout(stdout);
    QApplication a(argc, argv);
    cout << QLocale::system().name() << Qt::endl;
    QStringList translations;
    QDir dir(a.applicationDirPath());
    if (dir.cdUp() && dir.cd("share"))
        {
            translations.append(dir.absolutePath() + "/" + a.applicationName());
        }
    translations.append(QStandardPaths::standardLocations(QStandardPaths::DataLocation));
    translations.append(QCoreApplication::applicationDirPath());
    translations.append(a.applicationDirPath() + "/.qm");
    translations.append(a.applicationDirPath() + "/lang");
    QString translationFilePath = "";
    cout<<"Search for translations"<<Qt::endl;
    foreach (const QString &str, translations)
        {
            QFileInfo fileinfo(str + "/" + a.applicationName() + "_" + QLocale::system().name() + ".qm");
            cout << fileinfo.filePath() << Qt::endl;
            if (fileinfo.exists() && fileinfo.isFile())
                {
                    translationFilePath=fileinfo.filePath();
                    cout<<"Translation found in: "+translationFilePath<<Qt::endl;
                    break;
                }
        }

    QTranslator translator;
    cout << translator.load(translationFilePath) << Qt::endl;
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
    a.setWindowIcon(QIcon(":/images/diff.png"));
#endif

    MainWindow w;
    w.showMaximized();
    w.setWindowTitle(a.property("appname").toString() + " " + a.property("appversion").toString());
    return a.exec();
}
