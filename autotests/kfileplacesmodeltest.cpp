/*  This file is part of the KDE project
    Copyright (C) 2007 Kevin Ottens <ervin@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.

*/

#include <QtCore/QObject>
#include <QtDBus/QDBusInterface>

#include <kbookmarkmanager.h>
#include <kbookmark.h>
#include <QDebug>
#include <kfileplacesmodel.h>
#include <solid/device.h>

#include <QtTest/QtTest>

#include <stdlib.h>
#include <qstandardpaths.h>

#ifdef Q_OS_WIN
//c:\ as root for windows
#define KDE_ROOT_PATH "C:\\"
#else
#define KDE_ROOT_PATH "/"
#endif

// Avoid QHash randomization so that the order of the devices is stable
static void seedInit()
{
    qputenv("QT_HASH_SEED", "0");
    qputenv("QT_NO_CPU_FEATURE", "sse4.2");
}
Q_CONSTRUCTOR_FUNCTION(seedInit)

class KFilePlacesModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testInitialState();
    void testInitialList();
    void testReparse();
    void testInternalBookmarksHaveIds();
    void testHiding();
    void testMove();
    void testPlacesLifecycle();
    void testDevicePlugging();
    void testDragAndDrop();
    void testDeviceSetupTeardown();

private:
    QStringList placesUrls() const;
    QDBusInterface *fakeManager();
    QDBusInterface *fakeDevice(const QString &udi);

    KFilePlacesModel *m_places;
    KFilePlacesModel *m_places2; // To check that they always stay in sync
    // actually supposed to work across processes,
    // but much harder to test

    QMap<QString, QDBusInterface *> m_interfacesMap;
};

static QString bookmarksFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/user-places.xbel";
}

void KFilePlacesModelTest::initTestCase()
{
    qputenv("KDE_FORK_SLAVES", "yes"); // to avoid a runtime dependency on klauncher
    QStandardPaths::setTestModeEnabled(true);

    // Ensure we'll have a clean bookmark file to start
    QFile::remove(bookmarksFile());

    qRegisterMetaType<QModelIndex>();
    const QString fakeHw = QFINDTESTDATA("fakecomputer.xml");
    QVERIFY(!fakeHw.isEmpty());
    qputenv("SOLID_FAKEHW", QFile::encodeName(fakeHw));
    m_places = new KFilePlacesModel();
    m_places2 = new KFilePlacesModel();
}

void KFilePlacesModelTest::cleanupTestCase()
{
    delete m_places;
    delete m_places2;
    qDeleteAll(m_interfacesMap);
    QFile::remove(bookmarksFile());
}

QStringList KFilePlacesModelTest::placesUrls() const
{
    QStringList urls;
    for (int row = 0; row < m_places->rowCount(); ++row) {
        QModelIndex index = m_places->index(row, 0);
        urls << m_places->url(index).toDisplayString(QUrl::PreferLocalFile);
    }
    return urls;
}

#define CHECK_PLACES_URLS(urls)                                              \
    if (placesUrls() != urls) {                                              \
        qDebug() << "Expected:" << urls;                                     \
        qDebug() << "Got:" << placesUrls();                                  \
        QCOMPARE(placesUrls(), urls);                                        \
    }                                                                        \
    for (int row = 0; row < urls.size(); ++row) {                            \
        QModelIndex index = m_places->index(row, 0);                         \
        \
        QCOMPARE(m_places->url(index).toString(), QUrl::fromUserInput(urls[row]).toString()); \
        QCOMPARE(m_places->data(index, KFilePlacesModel::UrlRole).toUrl(),   \
                 QUrl(m_places->url(index)));                                \
        \
        index = m_places2->index(row, 0);                                    \
        \
        QCOMPARE(m_places2->url(index).toString(), QUrl::fromUserInput(urls[row]).toString()); \
        QCOMPARE(m_places2->data(index, KFilePlacesModel::UrlRole).toUrl(),  \
                 QUrl(m_places2->url(index)));                               \
    }                                                                        \
    \
    QCOMPARE(urls.size(), m_places->rowCount());                             \
    QCOMPARE(urls.size(), m_places2->rowCount());

QDBusInterface *KFilePlacesModelTest::fakeManager()
{
    return fakeDevice(QStringLiteral("/org/kde/solid/fakehw"));
}

QDBusInterface *KFilePlacesModelTest::fakeDevice(const QString &udi)
{
    if (m_interfacesMap.contains(udi)) {
        return m_interfacesMap[udi];
    }

    QDBusInterface *iface = new QDBusInterface(QDBusConnection::sessionBus().baseService(), udi);
    m_interfacesMap[udi] = iface;

    return iface;
}

void KFilePlacesModelTest::testInitialState()
{
    QCOMPARE(m_places->rowCount(), 4); // when the xbel file is empty, KFilePlacesModel fills it with 4 default items
    QCoreApplication::processEvents(); // Devices have a delayed loading
    QCOMPARE(m_places->rowCount(), 9);
}

static const QStringList initialListOfUrls()
{
    return QStringList() << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("trash:/")
                         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
}

void KFilePlacesModelTest::testInitialList()
{
    const QStringList urls = initialListOfUrls();
    CHECK_PLACES_URLS(urls);
}

void KFilePlacesModelTest::testReparse()
{
    QStringList urls;

    // add item

    m_places->addPlace(QStringLiteral("foo"), QUrl::fromLocalFile(QStringLiteral("/foo")),
                       QString(), QString());

    urls = initialListOfUrls();
    urls << QStringLiteral("/foo");
    CHECK_PLACES_URLS(urls);

    // reparse the bookmark file

    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForFile(bookmarksFile(), QStringLiteral("kfilePlaces"));

    bookmarkManager->notifyCompleteChange(QString());

    // check if they are the same

    CHECK_PLACES_URLS(urls);

    // try to remove item

    m_places->removePlace(m_places->index(9, 0));

    urls = initialListOfUrls();
    CHECK_PLACES_URLS(urls);
}

void KFilePlacesModelTest::testInternalBookmarksHaveIds()
{
    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForFile(bookmarksFile(), QStringLiteral("kfilePlaces"));
    KBookmarkGroup root = bookmarkManager->root();

    // Verify every entry has an id or an udi
    KBookmark bookmark = root.first();
    while (!bookmark.isNull()) {
        QVERIFY(!bookmark.metaDataItem(QStringLiteral("ID")).isEmpty() || !bookmark.metaDataItem(QStringLiteral("UDI")).isEmpty());
        // It's mutualy exclusive though
        QVERIFY(bookmark.metaDataItem(QStringLiteral("ID")).isEmpty() || bookmark.metaDataItem(QStringLiteral("UDI")).isEmpty());

        bookmark = root.next(bookmark);
    }

    // Verify that adding a bookmark behind its back the model gives it an id
    // (in real life it requires the user to modify the file by hand,
    // unlikely but better safe than sorry).
    // It induces a small race condition which means several ids will be
    // successively set on the same bookmark but no big deal since it won't
    // break the system
    KBookmark foo = root.addBookmark(QStringLiteral("Foo"), QUrl(QStringLiteral("file:/foo")), QStringLiteral("red-folder"));
    QCOMPARE(foo.text(), QStringLiteral("Foo"));
    QVERIFY(foo.metaDataItem(QStringLiteral("ID")).isEmpty());
    bookmarkManager->emitChanged(root);
    QCOMPARE(foo.text(), QStringLiteral("Foo"));
    QVERIFY(!foo.metaDataItem(QStringLiteral("ID")).isEmpty());

    // Verify that all the ids are different
    bookmark = root.first();
    QSet<QString> ids;
    while (!bookmark.isNull()) {
        QString id;
        if (!bookmark.metaDataItem(QStringLiteral("UDI")).isEmpty()) {
            id = bookmark.metaDataItem(QStringLiteral("UDI"));
        } else {
            id = bookmark.metaDataItem(QStringLiteral("ID"));
        }

        QVERIFY2(!ids.contains(id), "Duplicated ID found!");
        ids << id;
        bookmark = root.next(bookmark);
    }

    // Cleanup foo
    root.deleteBookmark(foo);
    bookmarkManager->emitChanged(root);
}

void KFilePlacesModelTest::testHiding()
{
    // Verify that nothing is hidden
    for (int row = 0; row < m_places->rowCount(); ++row) {
        QModelIndex index = m_places->index(row, 0);
        QVERIFY(!m_places->isHidden(index));
    }

    QModelIndex a = m_places->index(2, 0);
    QModelIndex b = m_places->index(6, 0);

    QList<QVariant> args;
    QSignalSpy spy(m_places, SIGNAL(dataChanged(QModelIndex,QModelIndex)));

    // Verify that hidden is taken into account and is not global
    m_places->setPlaceHidden(a, true);
    QVERIFY(m_places->isHidden(a));
    QVERIFY(m_places->data(a, KFilePlacesModel::HiddenRole).toBool());
    QVERIFY(!m_places->isHidden(b));
    QVERIFY(!m_places->data(b, KFilePlacesModel::HiddenRole).toBool());
    QCOMPARE(spy.count(), 1);
    args = spy.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), a);
    QCOMPARE(args.at(1).toModelIndex(), a);

    m_places->setPlaceHidden(b, true);
    QVERIFY(m_places->isHidden(a));
    QVERIFY(m_places->data(a, KFilePlacesModel::HiddenRole).toBool());
    QVERIFY(m_places->isHidden(b));
    QVERIFY(m_places->data(b, KFilePlacesModel::HiddenRole).toBool());
    QCOMPARE(spy.count(), 1);
    args = spy.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), b);
    QCOMPARE(args.at(1).toModelIndex(), b);

    m_places->setPlaceHidden(a, false);
    m_places->setPlaceHidden(b, false);
    QVERIFY(!m_places->isHidden(a));
    QVERIFY(!m_places->data(a, KFilePlacesModel::HiddenRole).toBool());
    QVERIFY(!m_places->isHidden(b));
    QVERIFY(!m_places->data(b, KFilePlacesModel::HiddenRole).toBool());
    QCOMPARE(spy.count(), 2);
    args = spy.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), a);
    QCOMPARE(args.at(1).toModelIndex(), a);
    args = spy.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), b);
    QCOMPARE(args.at(1).toModelIndex(), b);
}

void KFilePlacesModelTest::testMove()
{
    QList<QVariant> args;
    QSignalSpy spy_inserted(m_places, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spy_removed(m_places, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForFile(bookmarksFile(), QStringLiteral("kfilePlaces"));
    KBookmarkGroup root = bookmarkManager->root();
    KBookmark trash = m_places->bookmarkForIndex(m_places->index(3, 0));
    KBookmark before_trash = m_places->bookmarkForIndex(m_places->index(2, 0));

    // Move the trash at the end of the list
    KBookmark last = root.first();
    while (!root.next(last).isNull()) {
        last = root.next(last);
    }
    root.moveBookmark(trash, last);
    bookmarkManager->emitChanged(root);

    QStringList urls;
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH)
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign") << QStringLiteral("trash:/");

    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 8);
    QCOMPARE(args.at(2).toInt(), 8);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);

    // Move the trash at the beginning of the list
    root.moveBookmark(trash, KBookmark());
    bookmarkManager->emitChanged(root);

    urls.clear();
    urls << QStringLiteral("trash:/") << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH)
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");

    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 0);
    QCOMPARE(args.at(2).toInt(), 0);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 9);
    QCOMPARE(args.at(2).toInt(), 9);

    // Move the trash in the list (at its original place)
    root.moveBookmark(trash, before_trash);
    bookmarkManager->emitChanged(root);
    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("trash:/")
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 0);
    QCOMPARE(args.at(2).toInt(), 0);
}

void KFilePlacesModelTest::testDragAndDrop()
{
    QList<QVariant> args;
    QSignalSpy spy_moved(m_places, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)));

    // Monitor rowsInserted() and rowsRemoved() to ensure they are never emitted:
    // Moving with drag and drop is expected to emit rowsMoved()
    QSignalSpy spy_inserted(m_places, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spy_removed(m_places, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    // Move the trash at the end of the list
    QModelIndexList indexes;
    indexes << m_places->index(3, 0);
    QMimeData *mimeData = m_places->mimeData(indexes);
    QVERIFY(m_places->dropMimeData(mimeData, Qt::MoveAction, -1, 0, QModelIndex()));

    QStringList urls;
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH)
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign") << QStringLiteral("trash:/");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 0);
    QCOMPARE(spy_moved.count(), 1);
    args = spy_moved.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);
    QCOMPARE(args.at(3).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(4).toInt(), 9);

    // Move the trash at the beginning of the list
    indexes.clear();
    indexes << m_places->index(8, 0);
    mimeData = m_places->mimeData(indexes);
    QVERIFY(m_places->dropMimeData(mimeData, Qt::MoveAction, 0, 0, QModelIndex()));

    urls.clear();
    urls << QStringLiteral("trash:/") << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH)
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 0);
    QCOMPARE(spy_moved.count(), 1);
    args = spy_moved.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 8);
    QCOMPARE(args.at(2).toInt(), 8);
    QCOMPARE(args.at(3).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(4).toInt(), 0);

    // Move the trash in the list (at its original place)
    indexes.clear();
    indexes << m_places->index(0, 0);
    mimeData = m_places->mimeData(indexes);
    QVERIFY(m_places->dropMimeData(mimeData, Qt::MoveAction, 4, 0, QModelIndex()));

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("trash:/")
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 0);
    QCOMPARE(spy_moved.count(), 1);
    args = spy_moved.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 0);
    QCOMPARE(args.at(2).toInt(), 0);
    QCOMPARE(args.at(3).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(4).toInt(), 4);

    // Dropping on an item is not allowed
    indexes.clear();
    indexes << m_places->index(4, 0);
    mimeData = m_places->mimeData(indexes);
    QVERIFY(!m_places->dropMimeData(mimeData, Qt::MoveAction, -1, 0, m_places->index(2, 0)));
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 0);
    QCOMPARE(spy_moved.count(), 0);
}

void KFilePlacesModelTest::testPlacesLifecycle()
{
    QList<QVariant> args;
    QSignalSpy spy_inserted(m_places, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spy_removed(m_places, SIGNAL(rowsRemoved(QModelIndex,int,int)));
    QSignalSpy spy_changed(m_places, SIGNAL(dataChanged(QModelIndex,QModelIndex)));

    m_places->addPlace(QStringLiteral("Foo"), QUrl::fromLocalFile(QStringLiteral("/home/foo")));

    QStringList urls;
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("trash:/")
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign") << QStringLiteral("/home/foo");

    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 9);
    QCOMPARE(args.at(2).toInt(), 9);
    QCOMPARE(spy_removed.count(), 0);

    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForFile(bookmarksFile(), QStringLiteral("kfilePlaces"));
    KBookmarkGroup root = bookmarkManager->root();
    KBookmark before_trash = m_places->bookmarkForIndex(m_places->index(2, 0));
    KBookmark foo = m_places->bookmarkForIndex(m_places->index(9, 0));

    root.moveBookmark(foo, before_trash);
    bookmarkManager->emitChanged(root);

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("/home/foo")
         << QStringLiteral("trash:/") << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 10);
    QCOMPARE(args.at(2).toInt(), 10);

    m_places->editPlace(m_places->index(3, 0), QStringLiteral("Foo"), QUrl::fromLocalFile(QStringLiteral("/mnt/foo")));

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("/mnt/foo")
         << QStringLiteral("trash:/") << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 0);
    QCOMPARE(spy_changed.count(), 1);
    args = spy_changed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), m_places->index(3, 0));
    QCOMPARE(args.at(1).toModelIndex(), m_places->index(3, 0));

    foo = m_places->bookmarkForIndex(m_places->index(3, 0));
    foo.setFullText(QStringLiteral("Bar"));
    bookmarkManager->notifyCompleteChange(QString());

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("/mnt/foo")
         << QStringLiteral("trash:/") << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 0);
    QCOMPARE(spy_changed.count(), 10);
    args = spy_changed[3];
    QCOMPARE(args.at(0).toModelIndex(), m_places->index(3, 0));
    QCOMPARE(args.at(1).toModelIndex(), m_places->index(3, 0));
    spy_changed.clear();

    m_places->removePlace(m_places->index(3, 0));

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("trash:/")
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);

    m_places->addPlace(QStringLiteral("Foo"), QUrl::fromLocalFile(QStringLiteral("/home/foo")), QString(), QString(), m_places->index(1, 0));

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral("/home/foo") << QStringLiteral(KDE_ROOT_PATH)
         << QStringLiteral("trash:/") << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");

    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 2);
    QCOMPARE(args.at(2).toInt(), 2);
    QCOMPARE(spy_removed.count(), 0);

    m_places->removePlace(m_places->index(2, 0));
}

void KFilePlacesModelTest::testDevicePlugging()
{
    QList<QVariant> args;
    QSignalSpy spy_inserted(m_places, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy spy_removed(m_places, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    fakeManager()->call(QStringLiteral("unplug"), "/org/kde/solid/fakehw/volume_part1_size_993284096");

    QStringList urls;
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("trash:/")
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 6);
    QCOMPARE(args.at(2).toInt(), 6);

    fakeManager()->call(QStringLiteral("plug"), "/org/kde/solid/fakehw/volume_part1_size_993284096");

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("trash:/")
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 6);
    QCOMPARE(args.at(2).toInt(), 6);
    QCOMPARE(spy_removed.count(), 0);

    // Move the device in the list, and check that it memorizes the position across plug/unplug

    KBookmarkManager *bookmarkManager = KBookmarkManager::managerForFile(bookmarksFile(), QStringLiteral("kfilePlaces"));
    KBookmarkGroup root = bookmarkManager->root();
    KBookmark before_trash = m_places->bookmarkForIndex(m_places->index(2, 0));
    KBookmark device = root.first(); // The device we'll move is the 7th bookmark
    for (int i = 0; i < 6; i++) {
        device = root.next(device);
    }

    root.moveBookmark(device, before_trash);
    bookmarkManager->emitChanged(root);

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("/media/XO-Y4")
         << QStringLiteral("trash:/") << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 7);
    QCOMPARE(args.at(2).toInt(), 7);

    fakeManager()->call(QStringLiteral("unplug"), "/org/kde/solid/fakehw/volume_part1_size_993284096");

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH)
         << QStringLiteral("trash:/") << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 0);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);

    fakeManager()->call(QStringLiteral("plug"), "/org/kde/solid/fakehw/volume_part1_size_993284096");

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("/media/XO-Y4")
         << QStringLiteral("trash:/") << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);
    QCOMPARE(spy_removed.count(), 0);

    KBookmark seventh = root.first();
    for (int i = 0; i < 6; i++) {
        seventh = root.next(seventh);
    }
    root.moveBookmark(device, seventh);
    bookmarkManager->emitChanged(root);

    urls.clear();
    urls << QDir::homePath() << QStringLiteral("remote:/") << QStringLiteral(KDE_ROOT_PATH) << QStringLiteral("trash:/")
         << QStringLiteral("/media/nfs") << QStringLiteral("/media/floppy0") << QStringLiteral("/media/XO-Y4") << QStringLiteral("/media/cdrom") << QStringLiteral("/foreign");
    CHECK_PLACES_URLS(urls);
    QCOMPARE(spy_inserted.count(), 1);
    args = spy_inserted.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 6);
    QCOMPARE(args.at(2).toInt(), 6);
    QCOMPARE(spy_removed.count(), 1);
    args = spy_removed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(args.at(1).toInt(), 3);
    QCOMPARE(args.at(2).toInt(), 3);
}

void KFilePlacesModelTest::testDeviceSetupTeardown()
{
    QList<QVariant> args;
    QSignalSpy spy_changed(m_places, SIGNAL(dataChanged(QModelIndex,QModelIndex)));

    fakeDevice(QStringLiteral("/org/kde/solid/fakehw/volume_part1_size_993284096/StorageAccess"))->call(QStringLiteral("teardown"));

    QCOMPARE(spy_changed.count(), 1);
    args = spy_changed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex().row(), 6);
    QCOMPARE(args.at(1).toModelIndex().row(), 6);

    fakeDevice(QStringLiteral("/org/kde/solid/fakehw/volume_part1_size_993284096/StorageAccess"))->call(QStringLiteral("setup"));

    QCOMPARE(spy_changed.count(), 1);
    args = spy_changed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex().row(), 6);
    QCOMPARE(args.at(1).toModelIndex().row(), 6);
}

QTEST_MAIN(KFilePlacesModelTest)

#include "kfileplacesmodeltest.moc"
