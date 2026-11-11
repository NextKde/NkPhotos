// SPDX-License-Identifier: GPL-3.0-only
#include "ShareBridge.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace {
constexpr auto portalService = "org.freedesktop.portal.Desktop";
constexpr auto portalPath = "/org/freedesktop/portal/desktop";
constexpr auto requestInterface = "org.freedesktop.portal.Request";
}

ShareBridge::ShareBridge(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(180000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        closeRequest();
        finish(false, tr("The application chooser timed out. Try Open With again."));
    });
}

ShareBridge::~ShareBridge()
{
    closeRequest();
}

void ShareBridge::dismiss()
{
    m_message.clear();
    emit changed();
}

bool ShareBridge::listen(const QString &path)
{
    if (m_listenedPaths.contains(path)) return true;
    if (!QDBusConnection::sessionBus().connect(QString::fromLatin1(portalService), path,
            QString::fromLatin1(requestInterface), QStringLiteral("Response"), this,
            SLOT(portalResponse(uint,QVariantMap,QDBusMessage)))) return false;
    m_listenedPaths.append(path);
    return true;
}

void ShareBridge::openWith(const QUrl &source)
{
    if (m_busy) return;
    ++m_generation;
    const quint64 generation = m_generation;
    m_busy = true;
    m_message.clear();
    emit changed();

    const QFileInfo info(source.toLocalFile());
    if (!source.isLocalFile() || !info.isFile() || !info.isReadable()) {
        finish(false, tr("Choose one accessible local file to open with another application."));
        return;
    }
    QFile file(source.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        finish(false, tr("The selected file is no longer accessible: %1").arg(file.errorString()));
        return;
    }
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !(bus.connectionCapabilities() & QDBusConnection::UnixFileDescriptorPassing)) {
        finish(false, tr("The desktop application chooser is unavailable. Export the file and open it in your chosen application."));
        return;
    }

    const QString token = QStringLiteral("nkphotos_") + QUuid::createUuid().toString(QUuid::Id128);
    QString sender = bus.baseService();
    if (sender.startsWith(QLatin1Char(':'))) sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    m_requestPath = QStringLiteral("/org/freedesktop/portal/desktop/request/") + sender + QLatin1Char('/') + token;
    // Subscribe before sending: the chooser may reply before OpenFile returns.
    if (!listen(m_requestPath)) {
        finish(false, tr("Could not listen for the desktop application chooser response."));
        return;
    }
    const QDBusUnixFileDescriptor descriptor(file.handle());
    if (!descriptor.isValid()) {
        finish(false, tr("Could not grant access to the selected file."));
        return;
    }
    auto request = QDBusMessage::createMethodCall(QString::fromLatin1(portalService),
        QString::fromLatin1(portalPath), QStringLiteral("org.freedesktop.portal.OpenURI"), QStringLiteral("OpenFile"));
    const QVariantMap options{{QStringLiteral("handle_token"), token},
                              {QStringLiteral("ask"), true},
                              {QStringLiteral("writable"), false}};
    // The portal owns application selection and document access; no shell,
    // network URI, or implicit default-app fallback bypasses the user's choice.
    request << QString() << QVariant::fromValue(descriptor) << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(request, 15000), this);
    m_timeout.start();
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QDBusObjectPath> reply = *call;
        call->deleteLater();
        if (!m_busy || generation != m_generation) return;
        if (reply.isError()) {
            finish(false, tr("The desktop could not open the application chooser: %1").arg(reply.error().message()));
            return;
        }
        const QString path = reply.value().path();
        if (path.isEmpty() || path == QStringLiteral("/")) {
            finish(false, tr("The desktop returned an invalid application chooser request."));
            return;
        }
        m_requestPath = path;
        if (!listen(path)) {
            closeRequest();
            finish(false, tr("Could not receive the application chooser result."));
        }
    });
}

void ShareBridge::portalResponse(uint response, const QVariantMap &, const QDBusMessage &message)
{
    if (!m_busy || !m_listenedPaths.contains(message.path())) return;
    if (response == 0)
        finish(true, tr("The file was handed to the application you chose. Complete any sharing there."));
    else if (response == 1)
        finish(false, tr("Open With was canceled."));
    else
        finish(false, tr("The selected application could not open the file. Try another application or export the file."));
}

void ShareBridge::closeRequest()
{
    if (!m_busy || m_requestPath.isEmpty()) return;
    const auto request = QDBusMessage::createMethodCall(QString::fromLatin1(portalService),
        m_requestPath, QString::fromLatin1(requestInterface), QStringLiteral("Close"));
    QDBusConnection::sessionBus().asyncCall(request, 1000);
}

void ShareBridge::finish(bool success, const QString &message)
{
    m_timeout.stop();
    for (const auto &path : m_listenedPaths)
        QDBusConnection::sessionBus().disconnect(QString::fromLatin1(portalService), path,
            QString::fromLatin1(requestInterface), QStringLiteral("Response"), this,
            SLOT(portalResponse(uint,QVariantMap,QDBusMessage)));
    m_listenedPaths.clear();
    m_requestPath.clear();
    m_busy = false;
    m_message = message;
    emit changed();
    emit completed(success, message);
}
