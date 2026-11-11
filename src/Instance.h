// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QStringList>
class Instance : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nextks.photos.Application")
public:
    using QObject::QObject;
public slots:
    bool Open(const QStringList &urls) {
        if (urls.size() > 256) return false;
        qsizetype size = 0;
        for (const auto &url : urls) size += url.size();
        if (size > 65536) return false;
        emit requested(urls);
        return true;
    }
signals:
    void requested(const QStringList &urls);
};
