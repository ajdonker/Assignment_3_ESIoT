#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QLoggingCategory>
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QLoggingCategory::setFilterRules("qt.qml.debug=true\n");
    QQmlApplicationEngine engine;

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("DashboardSystem", "Main");
    qDebug() << "C++ MAIN STARTED";
    return app.exec();
}
