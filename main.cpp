/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QTextStream cout(stdout);
    QApplication a(argc, argv);
    QTranslator translator;
    cout<<translator.load(a.applicationDirPath()+"/lang/"+a.applicationName()+"_"+QLocale::system().name()+".qm")<<endl;
    cout<<QLocale::system().name()<<endl;
    a.installTranslator(&translator);
    QString platform="";
    #if __GNUC__
        #if __x86_64__
            platform="-64bit";
        #endif
    #endif
    a.setProperty("appversion","0.32b" + platform + " (QTbuild:" + QString(QT_VERSION_STR) +")");
    a.setProperty("appname","QT JSON Diff");

#ifdef Q_OS_LINUX
    a.setWindowIcon(QIcon(":/images/diff.png"));
#endif

    MainWindow w;
    w.showMaximized();
    w.setWindowTitle(a.property("appname").toString() + " " + a.property("appversion").toString());
    return a.exec();
}
