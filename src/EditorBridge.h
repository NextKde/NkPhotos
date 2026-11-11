// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QFutureWatcher>
#include <QUrl>

struct EditorResult { QUrl copy; QString error; };
class EditorBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit EditorBridge(QObject *parent = nullptr);
    ~EditorBridge() override;
    bool busy() const { return m_busy; }
    QString message() const { return m_message; }
    Q_INVOKABLE void editCopy(const QUrl &source, const QUrl &destinationFolder);
    Q_INVOKABLE void dismiss() { m_message.clear(); emit changed(); }
    static EditorResult createCopy(const QUrl &source, const QUrl &destinationFolder);
signals:
    void changed();
    void copyCreated(const QUrl &url);
private:
    static EditorResult launch(EditorResult result);
    bool m_busy = false;
    QString m_message;
    QFutureWatcher<EditorResult> m_job;
};
