// SPDX-License-Identifier: GPL-3.0-only
#include "EditorBridge.h"
#include <QtConcurrentRun>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QUuid>
#include <QProcess>
#include <QStandardPaths>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>

EditorBridge::EditorBridge(QObject *parent) : QObject(parent)
{
    connect(&m_job, &QFutureWatcher<EditorResult>::finished, this, [this] {
        const auto result = m_job.result();
        m_busy = false;
        m_message = result.error.isEmpty()
            ? tr("An editing copy was sent to NkShot. Save there, then return here. The original is unchanged.")
            : result.error;
        if (!result.copy.isEmpty()) emit copyCreated(result.copy);
        emit changed();
    });
}
EditorBridge::~EditorBridge() { m_job.waitForFinished(); }
void EditorBridge::editCopy(const QUrl &source, const QUrl &destinationFolder)
{
    if (m_busy) return;
    m_busy = true; m_message.clear(); emit changed();
    m_job.setFuture(QtConcurrent::run([source,destinationFolder] { return launch(createCopy(source,destinationFolder)); }));
}
EditorResult EditorBridge::createCopy(const QUrl &source, const QUrl &destinationFolder)
{
    QFileInfo input(source.toLocalFile());
    QFileInfo destination(destinationFolder.toLocalFile());
    if (!source.isLocalFile() || !destinationFolder.isLocalFile() || !input.isFile() || input.isSymLink()
        || !destination.isDir() || !input.isReadable() || input.size() > 64 * 1024 * 1024)
        return {{},tr("Cannot prepare an editing copy. Choose an accessible image and destination folder.")};
    const QString file = input.completeBaseName() + "-edited-" + QUuid::createUuid().toString(QUuid::Id128).left(8)
        + (input.suffix().isEmpty() ? QString() : "." + input.suffix());
    const QString target = QDir(destination.absoluteFilePath()).filePath(file);
    // QFile::copy refuses existing destinations, including collisions. No overwrites.
    if (!QFile::copy(input.absoluteFilePath(), target))
        return {{},tr("Could not save the editing copy. Choose another folder and try again.")};
    QFileInfo after(input.absoluteFilePath());
    if (after.size() != input.size() || after.lastModified() != input.lastModified()) {
        return {QUrl::fromLocalFile(target),tr("The source changed while copying. The copy was kept but was not sent to NkShot.")};
    }
    return {QUrl::fromLocalFile(target),{}};
}
EditorResult EditorBridge::launch(EditorResult result)
{
    if (!result.error.isEmpty()) return result;
    auto fail = [&](const QString &message) { result.error = message; return result; };
    // Native launches preserve the exact argv and never execute a shell.
    if (!qEnvironmentVariableIsSet("FLATPAK_ID")) {
        const QString executable = QStandardPaths::findExecutable("nkshot");
        if (!executable.isEmpty()) {
            if (QProcess::startDetached(executable,{"--edit-existing",result.copy.toLocalFile()})) return result;
            return fail(tr("NkShot could not be started. Your editing copy is available in Opened images."));
        }
    }
    // Both sandboxed and host editors receive a document-portal path, never a
    // path private to this application's sandbox. The original is never exported.
    QFile file(result.copy.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) return fail(tr("The editing copy is no longer accessible."));
    QDBusInterface documents("org.freedesktop.portal.Documents", "/org/freedesktop/portal/documents",
                             "org.freedesktop.portal.Documents",QDBusConnection::sessionBus());
    documents.setTimeout(5000);
    QDBusUnixFileDescriptor fd(file.handle());
    const QDBusReply<QString> added = documents.call("Add",QVariant::fromValue(fd),true,true);
    if (!added.isValid() || added.value().isEmpty())
        return fail(tr("The desktop could not share the editing copy. It remains available in Opened images."));
    const QDBusReply<void> grant = documents.call("GrantPermissions",added.value(),"org.nextkde.nkshot",QStringList{"read","write"});
    const QDBusReply<QByteArray> mount = documents.call("GetMountPoint");
    if (!grant.isValid() || !mount.isValid())
        return fail(tr("Could not grant NkShot access to the editing copy. The copy has been kept."));
    QByteArray mountBytes = mount.value();
    if (mountBytes.endsWith('\0')) mountBytes.chop(1);
    const QString shared = QDir(QString::fromUtf8(mountBytes)).filePath(added.value() + '/' + QFileInfo(file).fileName());
    QDBusInterface editor("org.nextkde.nkshot","/MainApplication","org.kde.KDBusService",QDBusConnection::sessionBus());
    editor.setTimeout(10000);
    const QDBusReply<int> opened = editor.call("CommandLine",QStringList{"nkshot","--edit-existing",shared},QString(),QVariantMap{});
    if (!opened.isValid() || opened.value() != 0)
        return fail(tr("NkShot is unavailable or could not open the copy. Install or start NkShot, then open the saved copy there."));
    return result;
}
