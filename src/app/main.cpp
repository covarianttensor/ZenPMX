#include "app/PmxAppBridge.h"
#include "viewport/PmxCamera.h"
#include "viewport/PmxViewportItem.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <QQuickWindow>
#include <QTimer>

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

    QString openPath;
    QString screenshotPath;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--screenshot") && i + 1 < argc) {
            screenshotPath = QString::fromLocal8Bit(argv[++i]);
        } else if (!arg.startsWith(QLatin1Char('-'))) {
            openPath = arg;
        }
    }

    if (!openPath.isEmpty()) {
        bridge.openFile(openPath);
    }

    if (!screenshotPath.isEmpty()) {
        auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (win) {
            QTimer::singleShot(800, [win, screenshotPath, &app]() {
                QImage img = win->grabWindow();
                img.save(screenshotPath);
                app.quit();
            });
        }
    }

    return app.exec();
}
