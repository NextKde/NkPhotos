// SPDX-License-Identifier: GPL-3.0-only
#include "PhotoLibrary.h"
#include "EditorBridge.h"
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QDir>

class UiTests : public QObject {
    Q_OBJECT
private slots:
    void galleryViewerAndKeyboard() {
        QTemporaryDir scratch;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,scratch.path()+"/profile");
        QDir().mkpath(scratch.path()+"/images");
        for (int i=0;i<9;++i) {
            QImage img(720,480,QImage::Format_RGB32);
            img.fill(QColor::fromHsv((i*36+180)%360,60,230));
            QPainter p(&img);
            p.setPen(Qt::NoPen); p.setBrush(QColor(255,235,180)); p.drawEllipse(QPoint(520,110),45,45);
            QPainterPath hills; hills.moveTo(0,360); hills.cubicTo(220,120,380,500,720,230); hills.lineTo(720,480); hills.lineTo(0,480);
            p.fillPath(hills,QColor::fromHsv((i*36+190)%360,110,140));
            p.setPen(Qt::white); p.setFont(QFont("Sans",22)); p.drawText(30,430,QString("NextKde · %1").arg(i+1)); p.end();
            QVERIFY(img.save(scratch.path()+QString("/images/Photo %1.png").arg(i+1)));
        }
        QImageReader::setAllocationLimit(128);
        auto registry=std::make_shared<ImageRegistry>();
        PhotoLibrary library(registry); EditorBridge editor;
        QQmlApplicationEngine engine;
        QStringList errors;
        connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings) errors.append(warning.toString());
        });
        engine.addImageProvider("photos",new ImageProvider(registry));
        engine.rootContext()->setContextProperty("photoLibrary",&library);
        engine.rootContext()->setContextProperty("editorBridge",&editor);
        engine.loadFromModule("Nk.Photos","Main");
        QVERIFY2(!engine.rootObjects().isEmpty(),qPrintable(errors.join('\n')));
        auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        library.addFolder(QUrl::fromLocalFile(scratch.path()+"/images"));
        QTRY_VERIFY_WITH_TIMEOUT(!library.loading(),5000);
        QCOMPARE(library.rowCount(),9);
        QTest::qWait(1200);
        const QString artifacts=qEnvironmentVariable("NKPHOTOS_ARTIFACT_DIR");
        if (!artifacts.isEmpty()) { QDir().mkpath(artifacts); QVERIFY(window->grabWindow().save(artifacts+"/gallery.png")); }
        library.select(0);
        QVERIFY(QMetaObject::invokeMethod(window,"openSelected"));
        auto *picture=window->findChild<QQuickItem *>("fullImage");
        QVERIFY(picture);
        QTRY_COMPARE_WITH_TIMEOUT(picture->property("status").toInt(),1,5000);
        QCOMPARE(window->property("viewing").toBool(),true);
        QTest::keyClick(window,Qt::Key_Right);
        QCOMPARE(library.selectedIndex(),1);
        QTRY_COMPARE_WITH_TIMEOUT(picture->property("status").toInt(),1,5000);
        window->setProperty("infoVisible",true);
        QTest::qWait(300);
        if (!artifacts.isEmpty()) QVERIFY(window->grabWindow().save(artifacts+"/viewer.png"));
        QTest::keyClick(window,Qt::Key_Escape);
        QCOMPARE(window->property("viewing").toBool(),false);
        // Missing files are visible as unavailable, never replaced by another selection.
        const QUrl selected=library.selected().value("url").toUrl();
        QVERIFY(QFile::remove(selected.toLocalFile())); library.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!library.loading(),5000);
        QCOMPARE(library.rowCount(),8); QCOMPARE(library.selectedIndex(),-1);
        QVERIFY2(errors.isEmpty(),qPrintable(errors.join('\n')));
        window->close();
    }
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    app.setOrganizationName("NkPhotosTests"); app.setApplicationName("UiSynthetic");
    QQuickStyle::setStyle("Basic");
    UiTests test; return QTest::qExec(&test,argc,argv);
}
#include "UiTests.moc"
