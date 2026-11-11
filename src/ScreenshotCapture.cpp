// SPDX-License-Identifier: GPL-3.0-only
#include "ScreenshotCapture.h"
#include "PhotoLibrary.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQuickWindow>
#include <QQuickItem>
#include <QImage>

ScreenshotCapture::ScreenshotCapture(QQuickWindow *window, PhotoLibrary *library,
                                     const QString &path, const QString &view, QObject *parent)
    : QObject(parent), m_window(window), m_library(library), m_path(path), m_view(view)
{
    m_elapsed.start();
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &ScreenshotCapture::advance);
    m_timer.start();
}

void ScreenshotCapture::advance()
{
    if (!m_window || m_elapsed.elapsed() > 30000) {
        qCritical("Screenshot could not finish within 30 seconds");
        m_timer.stop(); QCoreApplication::exit(4); return;
    }
    if (m_library->loading() || m_library->busy() || !m_window->isExposed()) {
        m_readyFrames = 0; return;
    }
    if (!m_prepared) {
        m_prepared = true;
        if (m_view != "viewer" && m_view != "info" && m_view != "video")
            QMetaObject::invokeMethod(m_window, "returnToGallery");
        if (m_view == "viewer" || m_view == "info" || m_view == "video") {
            if (m_view == "video") m_library->setMediaFilter("video");
            if (m_library->rowCount() == 0) {
                qCritical("Screenshot view requires matching media in this profile");
                m_timer.stop(); QCoreApplication::exit(4); return;
            }
            m_library->select(0);
            QMetaObject::invokeMethod(m_window, "openSelected");
            if (m_view == "info") m_window->setProperty("infoVisible", true);
        } else if (m_view == "albums" || m_view == "memories") {
            m_window->setProperty("browserPage", m_view);
        } else if (m_view == "years" || m_view == "months" || m_view == "days") {
            m_library->setDateView(m_view);
        } else if (m_view == "selection") {
            m_library->selectAll();
            m_window->setProperty("selectionMode", true);
        } else if (m_view == "trash" || m_view == "hidden" || m_view == "favorites" || m_view == "recent") {
            m_library->showCollection(m_view);
        }
        m_window->requestUpdate();
        return;
    }
    // Readiness comes from the actual window's image items, not a separate render.
    for (QObject *item : m_window->findChildren<QObject *>()) {
        if (!item->inherits("QQuickImage")) continue;
        auto *quick = qobject_cast<QQuickItem *>(item);
        if (quick && quick->isVisible() && item->property("status").toInt() == 2) {
            m_readyFrames = 0; return;
        }
    }
    if (++m_readyFrames < 8) return;
    const QImage image = m_window->grabWindow();
    if (image.isNull()) return;
    const QFileInfo destination(m_path);
    if (!QDir().mkpath(destination.absolutePath()) || !image.save(m_path)) {
        qCritical("Could not save screenshot");
        m_timer.stop(); QCoreApplication::exit(4); return;
    }
    qInfo("Screenshot saved to %s", qPrintable(m_path));
    m_timer.stop(); QCoreApplication::quit();
}
