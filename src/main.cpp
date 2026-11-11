// SPDX-License-Identifier: GPL-3.0-only
#include "PhotoLibrary.h"
#include "EditorBridge.h"
#include "ShareBridge.h"
#include "ScreenshotCapture.h"
#include "Instance.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QCryptographicHash>
#include <QQuickWindow>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QQuickStyle>
#include <QImageReader>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>
#include <QIcon>

int main(int argc, char **argv)
{
    QGuiApplication app(argc,argv);
    app.setOrganizationName("NextKde"); app.setOrganizationDomain("nextks.org");
    app.setApplicationName("org.nextks.photos"); app.setApplicationVersion("0.1.0");
    app.setApplicationDisplayName("NkPhotos"); app.setDesktopFileName("org.nextks.photos");
    QQuickStyle::setStyle("Basic");
    QImageReader::setAllocationLimit(128);
    QCommandLineParser parser;
    parser.setApplicationDescription("NextKde photo browser and image viewer");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"profile", "Use a separate settings/profile directory (development)","directory"});
    parser.addOption({"language", "Interface language: system, en or zh_CN","language","system"});
    parser.addOption({"screenshot", "Save this application window and exit (requires --profile)", "path"});
    parser.addOption({"screenshot-view", "View to capture: gallery, viewer, info, video, albums, memories, years, months, days, selection, trash, hidden, favorites, recent", "view", "gallery"});
    parser.addOption({"appearance", "Appearance: system, light or dark", "appearance", "system"});
    parser.addOption({"window-size", "Initial window size, for example 1080x720", "size"});
    parser.addPositionalArgument("images","Images or a folder to open","[images…]");
    parser.process(app);
    if (parser.isSet("profile")) {
        const QString profile = parser.value("profile");
        if (!QDir::isAbsolutePath(profile)) { qCritical("Profile directory must be absolute"); return 2; }
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,profile);
    }
    const QString appearance = parser.value("appearance");
    if (appearance != "system" && appearance != "light" && appearance != "dark") {
        qCritical("Unsupported appearance"); return 2;
    }
    const QString screenshot = parser.value("screenshot");
    const QString screenshotView = parser.value("screenshot-view");
    if (parser.isSet("screenshot")) {
        const QStringList views{"gallery", "viewer", "info", "video", "albums", "memories", "years", "months", "days", "selection", "trash", "hidden", "favorites", "recent"};
        if (!parser.isSet("profile") || !QDir::isAbsolutePath(screenshot) || !views.contains(screenshotView)
            || QFileInfo::exists(screenshot)) {
            qCritical("Screenshot requires an isolated --profile, a new absolute output path, and a supported view"); return 2;
        }
    }
    const auto dimensions = parser.value("window-size").split('x');
    if (parser.isSet("window-size") && (dimensions.size() != 2
        || dimensions[0].toInt() < 740 || dimensions[1].toInt() < 480
        || dimensions[0].toInt() > 3840 || dimensions[1].toInt() > 2160)) {
        qCritical("Window size must be between 740x480 and 3840x2160"); return 2;
    }
    QTranslator translator;
    const QString language = parser.value("language");
    if (language != "system" && language != "en" && language != "zh_CN") { qCritical("Unsupported language"); return 2; }
    if (language == "zh_CN" || (language == "system" && QLocale::system().language() == QLocale::Chinese)) {
        if (translator.load(":/i18n/nkphotos_zh_CN.qm")) app.installTranslator(&translator);
    }
    const QString settingsDir = QFileInfo(QSettings().fileName()).absolutePath();
    QDir().mkpath(settingsDir);
    QStringList inputUrls;
    for (const auto &argument : parser.positionalArguments())
        inputUrls.append(QUrl::fromUserInput(argument,QDir::currentPath(),QUrl::AssumeLocalFile).toString());
    const QString service = "org.nextks.photos.Profile" + QString::fromLatin1(
        QCryptographicHash::hash(settingsDir.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
    const auto bus = QDBusConnection::sessionBus();
    // Keep preferences single-writer. Opening another instance never overwrites
    // another process's folder/favorite list.
    QLockFile lock(settingsDir + "/photos.lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        if (!screenshot.isEmpty()) { qCritical("Screenshot profile is already in use"); return 3; }
        QDBusInterface running(service,"/Application","org.nextks.photos.Application",bus);
        running.setTimeout(3000);
        const QDBusReply<bool> reply = running.call("Open",inputUrls);
        if (reply.isValid() && reply.value()) return 0;
        qCritical("NkPhotos is already using this profile and could not accept the files"); return 3;
    }
    if (parser.isSet("appearance")) QSettings().setValue("Window/appearance", appearance);
    if (parser.isSet("window-size")) {
        QSettings settings;
        settings.setValue("Window/windowWidth", dimensions[0].toInt());
        settings.setValue("Window/windowHeight", dimensions[1].toInt());
    }
    auto registry = std::make_shared<ImageRegistry>();
    PhotoLibrary library(registry);
    EditorBridge editor;
    ShareBridge share;
    QQmlApplicationEngine engine;
    Instance instance;
    if (bus.isConnected()) {
        if (!QDBusConnection::sessionBus().registerService(service)
            || !QDBusConnection::sessionBus().registerObject("/Application",&instance,QDBusConnection::ExportAllSlots)) {
            qCritical("Could not register application activation"); return 3;
        }
    }
    QObject::connect(&instance,&Instance::requested,&library,[&](const QStringList &urls) {
        QList<QUrl> items;
        for (const auto &url : urls) if (QUrl(url).isLocalFile()) items.append(QUrl(url));
        if (!items.isEmpty()) library.openFiles(items);
        if (!engine.rootObjects().isEmpty()) {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            if (window) { window->show(); window->raise(); window->requestActivate(); }
        }
    });
    engine.addImageProvider("photos",new ImageProvider(registry));
    engine.rootContext()->setContextProperty("photoLibrary",&library);
    engine.rootContext()->setContextProperty("editorBridge",&editor);
    engine.rootContext()->setContextProperty("shareBridge",&share);
    QObject::connect(&editor,&EditorBridge::copyCreated,&library,[&](const QUrl &url) { library.openFiles({url}); });
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[] { QCoreApplication::exit(1); },Qt::QueuedConnection);
    engine.loadFromModule("Nk.Photos","Main");
    QList<QUrl> images;
    for (const auto &url : inputUrls) images.append(QUrl(url));
    if (!images.isEmpty()) QTimer::singleShot(0,&library,[&library,images] { library.openFiles(images); });
    if (!screenshot.isEmpty() && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window) { qCritical("No application window to capture"); return 4; }
        new ScreenshotCapture(window, &library, screenshot, screenshotView, &engine);
    }
    return app.exec();
}
