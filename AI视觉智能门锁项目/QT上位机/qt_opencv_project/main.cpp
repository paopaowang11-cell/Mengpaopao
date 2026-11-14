#include "dialog.h"
#include <QApplication>
#include <QCoreApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("NOVA");
    QCoreApplication::setApplicationName("SmartLockQt");
    Dialog w;
    w.show();

    return a.exec();
}
