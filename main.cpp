/* Author: Yuriy Kuzin
 */
#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QString platform="";
    Q_UNUSED(platform);
    #if __GNUC__
        #if __x86_64__
            platform="-64bit";
        #endif
    #endif
    a.setProperty("appversion","0.1b" + platform);
    a.setProperty("appname","QT JSON Diff");
    MainWindow w;
    w.show();
    w.setWindowTitle(a.property("appname").toString() + " " + a.property("appversion").toString());
    return a.exec();
}
