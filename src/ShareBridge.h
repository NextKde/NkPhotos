// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QDBusMessage>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class ShareBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit ShareBridge(QObject *parent = nullptr);
    ~ShareBridge() override;
    bool busy() const { return m_busy; }
    QString message() const { return m_message; }
    Q_INVOKABLE void openWith(const QUrl &source);
    Q_INVOKABLE void dismiss();
signals:
    void changed();
    void completed(bool success, const QString &message);
private slots:
    void portalResponse(uint response, const QVariantMap &results, const QDBusMessage &message);
private:
    void finish(bool success, const QString &message);
    void closeRequest();
    bool listen(const QString &path);
    bool m_busy = false;
    QString m_message;
    QString m_requestPath;
    QStringList m_listenedPaths;
    quint64 m_generation = 0;
    QTimer m_timeout;
};
