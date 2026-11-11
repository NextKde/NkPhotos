// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>

class PhotoLibrary;
class QQuickWindow;

// An explicit application screenshot command, never enabled during normal use.
class ScreenshotCapture : public QObject {
    Q_OBJECT
public:
    ScreenshotCapture(QQuickWindow *window, PhotoLibrary *library, const QString &path,
                      const QString &view, QObject *parent = nullptr);
private:
    void advance();
    QPointer<QQuickWindow> m_window;
    PhotoLibrary *m_library;
    QString m_path, m_view;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    int m_readyFrames = 0;
    bool m_prepared = false;
};
