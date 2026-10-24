#include "app/PmxAppBridge.h"
#include "viewport/PmxCamera.h"
#include "viewport/PmxViewportItem.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("pmx-editor"));
    app.setOrganizationName(QStringLiteral("MMD"));

    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    qmlRegisterType<pmx::PmxViewportItem>("PmxViewport", 1, 0, "PmxViewportItem");
    qmlRegisterType<pmx::PmxCamera>("PmxViewport", 1, 0, "PmxCamera");
    qmlRegisterType<pmx::PmxAppBridge>("PmxEditorApp", 1, 0, "PmxAppBridge");

    QQmlApplicationEngine engine;
    pmx::PmxAppBridge bridge;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    engine.loadFromModule("PmxEditorApp", "Main");
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
