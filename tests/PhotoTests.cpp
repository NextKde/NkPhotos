// SPDX-License-Identifier: GPL-3.0-only
#include "PhotoLibrary.h"
#include "EditorBridge.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QImage>
#include <QFileInfo>
#include <QDir>

class PhotoTests : public QObject {
    Q_OBJECT
private:
    QTemporaryDir profile;
    static QString image(const QString &directory, const QString &name, QColor color = Qt::blue) {
        const QString path = QDir(directory).filePath(name);
        QImage image(240,160,QImage::Format_ARGB32); image.fill(color);
        return image.save(path,"PNG") ? path : QString();
    }
    static ImageSource source(const QString &path) {
        QFileInfo info(path); return {path,info.size(),info.lastModified().toMSecsSinceEpoch()};
    }
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("NkPhotosTests");
        QCoreApplication::setApplicationName("Synthetic");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,profile.path());
        QImageReader::setAllocationLimit(128);
    }
    void init() { QSettings().clear(); }
    void scannerUsesLiteralPathsAndDoesNotRecurse() {
        QTemporaryDir dir;
        const auto original = image(dir.path(),"中文 % #\nphoto.png");
        QVERIFY(!original.isEmpty());
        QVERIFY(QDir(dir.path()).mkdir("nested"));
        QVERIFY(!image(dir.path()+"/nested","hidden.png").isEmpty());
        QVERIFY(QFile::link(original,dir.path()+"/symlink.png"));
        const auto result = PhotoLibrary::scan({dir.path()}, {}, {"png"},std::make_shared<std::atomic_bool>(false));
        QCOMPARE(result.entries.size(),1);
        QCOMPARE(result.entries.first().url.toLocalFile(),original);
        const auto decoded = ImageProvider::decode(result.entries.first().source,{80,80});
        QVERIFY(!decoded.isNull());
        QVERIFY(decoded.width() <= 80 && decoded.height() <= 80);
    }
    void cancellationAndUnavailableAreExplicit() {
        QTemporaryDir dir;
        image(dir.path(),"sample.png");
        auto cancelled = PhotoLibrary::scan({dir.path()}, {}, {"png"},std::make_shared<std::atomic_bool>(true));
        QVERIFY(cancelled.entries.isEmpty());
        auto missing = PhotoLibrary::scan({dir.path()+"/absent"}, {}, {"png"},std::make_shared<std::atomic_bool>(false));
        QCOMPARE(missing.unavailable.size(),1);
    }
    void changedAndCorruptFilesAreNotDecodedAsOldVersion() {
        QTemporaryDir dir;
        const auto path = image(dir.path(),"photo.png");
        const auto old = source(path);
        QFile file(path); QVERIFY(file.open(QIODevice::Append)); file.write("changed"); file.close();
        QVERIFY(ImageProvider::decode(old,{80,80}).isNull());
        QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate)); file.write("not an image"); file.close();
        QVERIFY(ImageProvider::decode(source(path),{80,80}).isNull());
    }
    void providerOnlyLoadsRegisteredFiles() {
        QTemporaryDir dir; const auto path = image(dir.path(),"sample.png");
        auto registry = std::make_shared<ImageRegistry>();
        ImageProvider provider(registry); QSize size;
        const auto entry = source(path); const auto key = ImageProvider::token(entry);
        QVERIFY(provider.requestImage(key+"/thumb",&size,{50,50}).isNull());
        registry->sources.insert(key,entry);
        QVERIFY(!provider.requestImage(key+"/thumb",&size,{50,50}).isNull());
        registry->sources.clear();
        QVERIFY(provider.requestImage(key+"/thumb",&size,{50,50}).isNull());
    }
    void foldersFavoritesAndRemovalPreserveOriginals() {
        QTemporaryDir dir; const auto path = image(dir.path(),"photo.png");
        auto registry = std::make_shared<ImageRegistry>();
        {
            PhotoLibrary model(registry);
            model.addFolder(QUrl::fromLocalFile(dir.path()));
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(),5000);
            QCOMPARE(model.rowCount(),1);
            model.select(0); model.toggleFavorite();
            QVERIFY(model.selected().value("favorite").toBool());
            model.removeFolder(QUrl::fromLocalFile(dir.path()));
            QTRY_VERIFY_WITH_TIMEOUT(!model.loading(),5000);
            QVERIFY(QFileInfo::exists(path));
            QCOMPARE(model.folders().size(),0);
        }
        PhotoLibrary restored(registry);
        restored.showCollection("favorites");
        QTRY_VERIFY_WITH_TIMEOUT(!restored.loading(),5000);
        QCOMPARE(restored.rowCount(),1);
    }
    void rapidCollectionChangesDiscardStaleScans() {
        QTemporaryDir first,second;
        for(int i=0;i<50;++i) image(first.path(),QString::number(i)+".png");
        image(second.path(),"second.png");
        PhotoLibrary model(std::make_shared<ImageRegistry>());
        model.addFolder(QUrl::fromLocalFile(first.path()));
        model.addFolder(QUrl::fromLocalFile(second.path()));
        model.setSearch("second");
        QTRY_VERIFY_WITH_TIMEOUT(!model.loading(),5000);
        QCOMPARE(model.rowCount(),1);
        QCOMPARE(model.data(model.index(0),PhotoLibrary::NameRole).toString(),QString("second.png"));
    }
    void editingCopyDoesNotOverwriteOriginalOrSibling() {
        QTemporaryDir dir;
        const QString path = image(dir.path(),"source.png");
        QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly)); const auto bytes = original.readAll(); original.close();
        const auto first = EditorBridge::createCopy(QUrl::fromLocalFile(path),QUrl::fromLocalFile(dir.path()));
        const auto second = EditorBridge::createCopy(QUrl::fromLocalFile(path),QUrl::fromLocalFile(dir.path()));
        QVERIFY(first.error.isEmpty()); QVERIFY(second.error.isEmpty());
        QVERIFY(first.copy != second.copy); QVERIFY(first.copy.toLocalFile() != path);
        QFile copy(first.copy.toLocalFile()); QVERIFY(copy.open(QIODevice::ReadOnly)); QCOMPARE(copy.readAll(),bytes);
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(),bytes);
    }
};
QTEST_GUILESS_MAIN(PhotoTests)
#include "PhotoTests.moc"
