#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QQuickStyle>
#include "models/LibraryModel.h"
#include "ui/PdfQuickItem.h"

int main(int argc, char* argv[]) {
    // High DPI Scaling
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/SereneReader/app_icon.ico"));
    app.setOrganizationName("Serene Labs");
    app.setApplicationName("Serene Reader Native");

    QQuickStyle::setStyle("Basic");

    // Register Custom QML Types
    qmlRegisterType<PdfQuickItem>("SereneReader", 1, 0, "PdfReaderItem");

    QQmlApplicationEngine engine;

    // Initialize Models
    LibraryModel libraryModel;
    engine.rootContext()->setContextProperty("libraryModel", &libraryModel);

    const QUrl url(u"qrc:/SereneReader/qml/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
