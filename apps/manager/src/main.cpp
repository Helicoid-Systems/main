#include "main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication::setOrganizationName("Helicoid Systems");
    QApplication::setOrganizationDomain("helicoid.systems");
    QApplication::setApplicationName("Helicoid Manager");
    QApplication::setApplicationVersion("0.1.0");

    QApplication application(argc, argv);
    application.setWindowIcon(QIcon(QStringLiteral(":/brand/windows-manager.ico")));
    MainWindow window;
    window.show();
    return application.exec();
}
