/* This file is part of the KDE project
   Copyright (C) 2006 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kfileitemtest.h"
#include <kfileitemlistproperties.h>
#include <qtest.h>
#include <kfileitem.h>

#include <qtemporarydir.h>
#include <qtemporaryfile.h>
#include <kuser.h>
#include <kdesktopfile.h>
#include <kconfiggroup.h>
#include "kiotesthelper.h"

#include <QMimeDatabase>

QTEST_MAIN(KFileItemTest)

void KFileItemTest::initTestCase()
{
}

void KFileItemTest::testPermissionsString()
{
    // Directory
    QTemporaryDir tempDir;
    KFileItem dirItem(QUrl::fromLocalFile(tempDir.path() + '/'));
    QCOMPARE((uint)dirItem.permissions(), (uint)0700);
    QCOMPARE(dirItem.permissionsString(), QStringLiteral("drwx------"));
    QVERIFY(dirItem.isReadable());

    // File
    QFile file(tempDir.path() + "/afile");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadOther); // 0604
    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()), QString(), KFileItem::Unknown);
    QCOMPARE((uint)fileItem.permissions(), (uint)0604);
    QCOMPARE(fileItem.permissionsString(), QStringLiteral("-rw----r--"));
    QVERIFY(fileItem.isReadable());

    // Symlink to file
    QString symlink = tempDir.path() + "/asymlink";
    QVERIFY(file.link(symlink));
    QUrl symlinkUrl = QUrl::fromLocalFile(symlink);
    KFileItem symlinkItem(symlinkUrl, QString(), KFileItem::Unknown);
    QCOMPARE((uint)symlinkItem.permissions(), (uint)0604);
    // This is a bit different from "ls -l": we get the 'l' but we see the permissions of the target.
    // This is actually useful though; the user sees it's a link, and can check if he can read the [target] file.
    QCOMPARE(symlinkItem.permissionsString(), QStringLiteral("lrw----r--"));
    QVERIFY(symlinkItem.isReadable());

    // Symlink to directory (#162544)
    QVERIFY(QFile::remove(symlink));
    QVERIFY(QFile(tempDir.path() + '/').link(symlink));
    KFileItem symlinkToDirItem(symlinkUrl, QString(), KFileItem::Unknown);
    QCOMPARE((uint)symlinkToDirItem.permissions(), (uint)0700);
    QCOMPARE(symlinkToDirItem.permissionsString(), QStringLiteral("lrwx------"));
}

void KFileItemTest::testNull()
{
    KFileItem null;
    QVERIFY(null.isNull());
    KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/")), QString(), KFileItem::Unknown);
    QVERIFY(!fileItem.isNull());
    null = fileItem; // ok, now 'null' isn't so null anymore
    QVERIFY(!null.isNull());
    QVERIFY(null.isReadable());
    QVERIFY(!null.isHidden());
}

void KFileItemTest::testDoesNotExist()
{
    KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/doesnotexist")), QString(), KFileItem::Unknown);
    QVERIFY(!fileItem.isNull());
    QVERIFY(!fileItem.isReadable());
    QVERIFY(fileItem.user().isEmpty());
    QVERIFY(fileItem.group().isEmpty());
}

void KFileItemTest::testDetach()
{
    KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/one")), QString(), KFileItem::Unknown);
    QCOMPARE(fileItem.name(), QStringLiteral("one"));
    KFileItem fileItem2(fileItem);
    QVERIFY(fileItem == fileItem2);
    QVERIFY(fileItem.d == fileItem2.d);
    fileItem2.setName(QStringLiteral("two"));
    QCOMPARE(fileItem2.name(), QStringLiteral("two"));
    QCOMPARE(fileItem.name(), QStringLiteral("one")); // it detached
    QVERIFY(fileItem == fileItem2);
    QVERIFY(fileItem.d != fileItem2.d);

    fileItem = fileItem2;
    QCOMPARE(fileItem.name(), QStringLiteral("two"));
    QVERIFY(fileItem == fileItem2);
    QVERIFY(fileItem.d == fileItem2.d);
    QVERIFY(!(fileItem != fileItem2));
}

void KFileItemTest::testBasic()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    QFile fileObj(file.fileName());
    QVERIFY(fileObj.open(QIODevice::WriteOnly));
    fileObj.write(QByteArray("Hello"));
    fileObj.close();

    QUrl url = QUrl::fromLocalFile(file.fileName());
    KFileItem fileItem(url, QString(), KFileItem::Unknown);
    QCOMPARE(fileItem.text(), url.fileName());
    QVERIFY(fileItem.isLocalFile());
    QCOMPARE(fileItem.localPath(), url.path());
    QCOMPARE(fileItem.size(), KIO::filesize_t(5));
    QVERIFY(fileItem.linkDest().isEmpty());
    QVERIFY(!fileItem.isHidden());
    QVERIFY(fileItem.isReadable());
    QVERIFY(fileItem.isWritable());
    QVERIFY(fileItem.isFile());
    QVERIFY(!fileItem.isDir());
    QVERIFY(!fileItem.isDesktopFile());
    QCOMPARE(fileItem.user(), KUser().loginName());
}

void KFileItemTest::testRootDirectory()
{
    const QString rootPath = QDir::rootPath();
    QUrl url = QUrl::fromLocalFile(rootPath);
    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    KFileItem fileItem(entry, url);
    QCOMPARE(fileItem.text(), QStringLiteral("."));
    QVERIFY(fileItem.isLocalFile());
    QCOMPARE(fileItem.localPath(), url.path());
    QVERIFY(fileItem.linkDest().isEmpty());
    QVERIFY(!fileItem.isHidden());
    QVERIFY(!fileItem.isFile());
    QVERIFY(fileItem.isDir());
    QVERIFY(!fileItem.isDesktopFile());
}

void KFileItemTest::testHiddenFile()
{
    QTemporaryDir tempDir;
    QFile file(tempDir.path() + "/.hiddenfile");
    QVERIFY(file.open(QIODevice::WriteOnly));
    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()), QString(), KFileItem::Unknown);
    QCOMPARE(fileItem.text(), QStringLiteral(".hiddenfile"));
    QVERIFY(fileItem.isLocalFile());
    QVERIFY(fileItem.isHidden());
}

void KFileItemTest::testMimeTypeOnDemand()
{
    QTemporaryFile file;
    QVERIFY(file.open());

    {
        KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
        fileItem.setDelayedMimeTypes(true);
        QVERIFY(fileItem.currentMimeType().isDefault());
        QVERIFY(!fileItem.isMimeTypeKnown());
        QVERIFY(!fileItem.isFinalIconKnown());
        //qDebug() << fileItem.determineMimeType().name();
        QCOMPARE(fileItem.determineMimeType().name(), QStringLiteral("application/x-zerosize"));
        QCOMPARE(fileItem.mimetype(), QStringLiteral("application/x-zerosize"));
        QVERIFY(fileItem.isMimeTypeKnown());
        QVERIFY(fileItem.isFinalIconKnown());
    }

    {
        // Calling mimeType directly also does mimetype determination
        KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
        fileItem.setDelayedMimeTypes(true);
        QVERIFY(!fileItem.isMimeTypeKnown());
        QCOMPARE(fileItem.mimetype(), QStringLiteral("application/x-zerosize"));
        QVERIFY(fileItem.isMimeTypeKnown());
    }

    {
        // Calling overlays should NOT do mimetype determination (#237668)
        KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
        fileItem.setDelayedMimeTypes(true);
        QVERIFY(!fileItem.isMimeTypeKnown());
        fileItem.overlays();
        QVERIFY(!fileItem.isMimeTypeKnown());
    }

    {
        QTemporaryFile file;
        QVERIFY(file.open());
        // Check whether mime-magic is used.
        // No known extension, so it should be used by determineMimeType.
        file.write(QByteArray("%PDF-"));
        QString fileName = file.fileName();
        QVERIFY(!fileName.isEmpty());
        file.close();
        KFileItem fileItem(QUrl::fromLocalFile(fileName));
        fileItem.setDelayedMimeTypes(true);
        QCOMPARE(fileItem.currentMimeType().name(), QLatin1String("application/octet-stream"));
        QVERIFY(fileItem.currentMimeType().isValid());
        QVERIFY(fileItem.currentMimeType().isDefault());
        QVERIFY(!fileItem.isMimeTypeKnown());
        QCOMPARE(fileItem.determineMimeType().name(), QStringLiteral("application/pdf"));
        QCOMPARE(fileItem.mimetype(), QStringLiteral("application/pdf"));
    }

    {
        QTemporaryFile file(QDir::tempPath() + QLatin1String("/kfileitemtest_XXXXXX.txt"));
        QVERIFY(file.open());
        // Check whether mime-magic is used.
        // Known extension, so it should NOT be used.
        file.write(QByteArray("<smil"));
        QString fileName = file.fileName();
        QVERIFY(!fileName.isEmpty());
        file.close();
        KFileItem fileItem(QUrl::fromLocalFile(fileName));
        fileItem.setDelayedMimeTypes(true);
        QCOMPARE(fileItem.currentMimeType().name(), QStringLiteral("text/plain"));
        QVERIFY(fileItem.isMimeTypeKnown());
        QCOMPARE(fileItem.determineMimeType().name(), QStringLiteral("text/plain"));
        QCOMPARE(fileItem.mimetype(), QStringLiteral("text/plain"));

        // And if the mimetype is not on demand?
        KFileItem fileItem2(QUrl::fromLocalFile(fileName));
        QCOMPARE(fileItem2.currentMimeType().name(), QStringLiteral("text/plain")); // XDG says: application/smil; but can't sniff all files so this can't work
        QVERIFY(fileItem2.isMimeTypeKnown());
    }
}

void KFileItemTest::testCmp()
{
    QTemporaryFile file;
    QVERIFY(file.open());

    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
    fileItem.setDelayedMimeTypes(true);
    KFileItem fileItem2(QUrl::fromLocalFile(file.fileName()));
    QVERIFY(fileItem == fileItem2); // created independently, but still 'equal'
    QVERIFY(fileItem.d != fileItem2.d);
    QVERIFY(!(fileItem != fileItem2));
    QVERIFY(fileItem.cmp(fileItem2));
}

void KFileItemTest::testRename()
{
    KIO::UDSEntry entry;
    const QString origName = QStringLiteral("foo");
    entry.insert(KIO::UDSEntry::UDS_NAME, origName);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    KFileItem fileItem(entry, QUrl::fromLocalFile(QStringLiteral("/dir/foo")));
    QCOMPARE(fileItem.name(), origName);
    QCOMPARE(fileItem.text(), origName);
    const QString newName = QStringLiteral("FiNeX_rocks");
    fileItem.setName(newName);
    QCOMPARE(fileItem.name(), newName);
    QCOMPARE(fileItem.text(), newName);
    QCOMPARE(fileItem.entry().stringValue(KIO::UDSEntry::UDS_NAME), newName); // #195385
}

void KFileItemTest::testRefresh()
{
    QTemporaryDir tempDir;
    QFileInfo dirInfo(tempDir.path());
    // Refresh on a dir
    KFileItem dirItem(QUrl::fromLocalFile(tempDir.path()));
    QVERIFY(dirItem.isDir());
    QVERIFY(dirItem.entry().isDir());
    QDateTime lastModified = dirInfo.lastModified();
    // Qt 5.8 adds milliseconds (but UDSEntry has no support for that)
    lastModified = lastModified.addMSecs(-lastModified.time().msec());
    QCOMPARE(dirItem.time(KFileItem::ModificationTime), lastModified);
    dirItem.refresh();
    QVERIFY(dirItem.isDir());
    QVERIFY(dirItem.entry().isDir());
    QCOMPARE(dirItem.time(KFileItem::ModificationTime), lastModified);

    // Refresh on a file
    QFile file(tempDir.path() + "/afile");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Hello world\n");
    file.close();
    QFileInfo fileInfo(file.fileName());
    const KIO::filesize_t expectedSize = 12;
    QCOMPARE(KIO::filesize_t(fileInfo.size()), expectedSize);
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadOther); // 0604
    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
    QVERIFY(fileItem.isFile());
    QVERIFY(!fileItem.isLink());
    QCOMPARE(fileItem.size(), expectedSize);
    QCOMPARE(fileItem.user(), KUser().loginName());
    // Qt 5.8 adds milliseconds (but UDSEntry has no support for that)
    lastModified = dirInfo.lastModified();
    lastModified = lastModified.addMSecs(-lastModified.time().msec());
    QCOMPARE(fileItem.time(KFileItem::ModificationTime), lastModified);
    fileItem.refresh();
    QVERIFY(fileItem.isFile());
    QVERIFY(!fileItem.isLink());
    QCOMPARE(fileItem.size(), expectedSize);
    QCOMPARE(fileItem.user(), KUser().loginName());
    QCOMPARE(fileItem.time(KFileItem::ModificationTime), lastModified);

    // Refresh on a symlink to a file
    const QString symlink = tempDir.path() + "/asymlink";
    QVERIFY(file.link(symlink));
    QDateTime symlinkTime = QDateTime::currentDateTime().addSecs(-20);
    // we currently lose milliseconds....
    symlinkTime = symlinkTime.addMSecs(-symlinkTime.time().msec());
    setTimeStamp(symlink, symlinkTime); // differenciate link time and source file time
    const QUrl symlinkUrl = QUrl::fromLocalFile(symlink);
    KFileItem symlinkItem(symlinkUrl);
    QVERIFY(symlinkItem.isFile());
    QVERIFY(symlinkItem.isLink());
    QCOMPARE(symlinkItem.size(), expectedSize);
    QCOMPARE(symlinkItem.time(KFileItem::ModificationTime), symlinkTime);
    symlinkItem.refresh();
    QVERIFY(symlinkItem.isFile());
    QVERIFY(symlinkItem.isLink());
    QCOMPARE(symlinkItem.size(), expectedSize);
    QCOMPARE(symlinkItem.time(KFileItem::ModificationTime), symlinkTime);

    // Symlink to directory (#162544)
    QVERIFY(QFile::remove(symlink));
    QVERIFY(QFile(tempDir.path() + '/').link(symlink));
    KFileItem symlinkToDirItem(symlinkUrl);
    QVERIFY(symlinkToDirItem.isDir());
    QVERIFY(symlinkToDirItem.isLink());
    symlinkToDirItem.refresh();
    QVERIFY(symlinkToDirItem.isDir());
    QVERIFY(symlinkToDirItem.isLink());
}

void KFileItemTest::testDotDirectory()
{
    QTemporaryDir tempDir;
    QFile file(tempDir.path() + "/.directory");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[Desktop Entry]\nIcon=foo\nComment=com\n");
    file.close();
    {
        KFileItem fileItem(QUrl::fromLocalFile(tempDir.path() + '/'), QString(), KFileItem::Unknown);
        QVERIFY(fileItem.isLocalFile());
        QCOMPARE(fileItem.mimeComment(), QStringLiteral("com"));
        QCOMPARE(fileItem.iconName(), QStringLiteral("foo"));
    }
    // Test for calling iconName first, to trigger mimetype resolution
    {
        KFileItem fileItem(QUrl::fromLocalFile(tempDir.path()), QString(), KFileItem::Unknown);
        QVERIFY(fileItem.isLocalFile());
        QCOMPARE(fileItem.iconName(), QStringLiteral("foo"));
    }
}

void KFileItemTest::testDecodeFileName_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("simple") << "filename" << "filename";
    QTest::newRow("/ at end") << QString(QStringLiteral("foo") + QChar(0x2044)) << QString(QStringLiteral("foo") + QChar(0x2044));
    QTest::newRow("/ at begin") << QString(QChar(0x2044)) << QString(QChar(0x2044));
}

void KFileItemTest::testDecodeFileName()
{
    QFETCH(QString, filename);
    QFETCH(QString, expectedText);
    QCOMPARE(KIO::decodeFileName(filename), expectedText);
}

void KFileItemTest::testEncodeFileName_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expectedFileName");

    QTest::newRow("simple") << "filename" << "filename";
    QTest::newRow("/ at end") << "foo/" << QString(QStringLiteral("foo") + QChar(0x2044));
    QTest::newRow("/ at begin") << "/" << QString(QChar(0x2044));
}

void KFileItemTest::testEncodeFileName()
{
    QFETCH(QString, text);
    QFETCH(QString, expectedFileName);
    QCOMPARE(KIO::encodeFileName(text), expectedFileName);
}

void KFileItemTest::testListProperties_data()
{
    QTest::addColumn<QString>("itemDescriptions");
    QTest::addColumn<bool>("expectedReading");
    QTest::addColumn<bool>("expectedDeleting");
    QTest::addColumn<bool>("expectedIsLocal");
    QTest::addColumn<bool>("expectedIsDirectory");
    QTest::addColumn<QString>("expectedMimeType");
    QTest::addColumn<QString>("expectedMimeGroup");

    QTest::newRow("one file") << "f" << true << true << true << false << "text/plain" << "text";
    QTest::newRow("one dir") << "d" << true << true << true << true << "inode/directory" << "inode";
    QTest::newRow("root dir") << "/" << true << false << true << true << "inode/directory" << "inode";
    QTest::newRow("file+dir") << "fd" << true << true << true << false << "" << "";
    QTest::newRow("two dirs") << "dd" << true << true << true << true << "inode/directory" << "inode";
    QTest::newRow("dir+root dir") << "d/" << true << false << true << true << "inode/directory" << "inode";
    QTest::newRow("two (text+html) files") << "ff" << true << true << true << false << "" << "text";
    QTest::newRow("three (text+html+empty) files") << "fff" << true << true << true << false << "" << "";
    QTest::newRow("http url") << "h" << true << true /*says kio_http...*/
                              << false << false << "application/octet-stream" << "application";
    QTest::newRow("2 http urls") << "hh" << true << true /*says kio_http...*/
                                 << false << false << "application/octet-stream" << "application";
}

void KFileItemTest::testListProperties()
{
    QFETCH(QString, itemDescriptions);
    QFETCH(bool, expectedReading);
    QFETCH(bool, expectedDeleting);
    QFETCH(bool, expectedIsLocal);
    QFETCH(bool, expectedIsDirectory);
    QFETCH(QString, expectedMimeType);
    QFETCH(QString, expectedMimeGroup);

    QTemporaryDir tempDir;
    QDir baseDir(tempDir.path());
    KFileItemList items;
    for (int i = 0; i < itemDescriptions.size(); ++i) {
        QString fileName = tempDir.path() + "/file" + QString::number(i);
        switch (itemDescriptions[i].toLatin1()) {
        case 'f': {
            if (i == 1) { // 2nd file is html
                fileName += QLatin1String(".html");
            }
            QFile file(fileName);
            QVERIFY(file.open(QIODevice::WriteOnly));
            if (i != 2) { // 3rd file is empty
                file.write("Hello");
            }
            items << KFileItem(QUrl::fromLocalFile(fileName), QString(), KFileItem::Unknown);
        }
        break;
        case 'd':
            QVERIFY(baseDir.mkdir(fileName));
            items << KFileItem(QUrl::fromLocalFile(fileName), QString(), KFileItem::Unknown);
            break;
        case '/':
            items << KFileItem(QUrl::fromLocalFile(QStringLiteral("/")), QString(), KFileItem::Unknown);
            break;
        case 'h':
            items << KFileItem(QUrl(QStringLiteral("http://www.kde.org")), QString(), KFileItem::Unknown);
            break;
        default:
            QVERIFY(false);
        }
    }
    KFileItemListProperties props(items);
    QCOMPARE(props.supportsReading(), expectedReading);
    QCOMPARE(props.supportsDeleting(), expectedDeleting);
    QCOMPARE(props.isLocal(), expectedIsLocal);
    QCOMPARE(props.isDirectory(), expectedIsDirectory);
    QCOMPARE(props.mimeType(), expectedMimeType);
    QCOMPARE(props.mimeGroup(), expectedMimeGroup);
}

void KFileItemTest::testIconNameForUrl_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedIcon");

    QTest::newRow("root") << "file:/" << "folder"; // the icon comes from KProtocolInfo
    if (QFile::exists(QStringLiteral("/tmp"))) {
        QTest::newRow("subdir") << "file:/tmp" << "inode-directory";
    }
    // TODO more tests
}

void KFileItemTest::testIconNameForUrl()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedIcon);

    QCOMPARE(KIO::iconNameForUrl(QUrl(url)), expectedIcon);
}

void KFileItemTest::testMimetypeForRemoteFolder()
{
    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, QStringLiteral("foo"));
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    QUrl url(QStringLiteral("smb://remoteFolder/foo"));
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.mimetype(), QStringLiteral("inode/directory"));
}

void KFileItemTest::testMimetypeForRemoteFolderWithFileType()
{
    QString udsMimeType = QStringLiteral("application/x-smb-workgroup");
    QVERIFY2(QMimeDatabase().mimeTypeForName(udsMimeType).isValid(),
             qPrintable(QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation).join(':'))); // kcoreaddons installed? XDG_DATA_DIRS set?
    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, QStringLiteral("foo"));
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, udsMimeType);

    QUrl url(QStringLiteral("smb://remoteFolder/foo"));
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.mimetype(), udsMimeType);
}

void KFileItemTest::testCurrentMimetypeForRemoteFolder()
{
    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, QStringLiteral("foo"));
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    QUrl url(QStringLiteral("smb://remoteFolder/foo"));
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.currentMimeType().name(), QStringLiteral("inode/directory"));
}

void KFileItemTest::testCurrentMimetypeForRemoteFolderWithFileType()
{
    QString udsMimeType = QStringLiteral("application/x-smb-workgroup");
    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_NAME, QStringLiteral("foo"));
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, udsMimeType);

    QUrl url(QStringLiteral("smb://remoteFolder/foo"));
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.currentMimeType().name(), udsMimeType);
}

void KFileItemTest::testIconNameForCustomFolderIcons()
{
    // Custom folder icons should be displayed (bug 350612)

    const QString iconName = QStringLiteral("folder-music");

    QTemporaryDir tempDir;
    const QUrl url = QUrl::fromLocalFile(tempDir.path());
    KDesktopFile cfg(tempDir.path() + QLatin1String("/.directory"));
    cfg.desktopGroup().writeEntry("Icon", iconName);
    cfg.sync();

    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.iconName(), iconName);
}

void KFileItemTest::testIconNameForStandardPath()
{
    const QString iconName = QStringLiteral("folder-videos");
    const QUrl url = QUrl::fromLocalFile(QDir::homePath() + QLatin1String("/Videos"));
    QStandardPaths::setTestModeEnabled(true);

    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.iconName(), iconName);
}

#ifndef Q_OS_WIN // user/group/other write permissions are not handled on windows

void KFileItemTest::testIsReadable_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("readable");

    QTest::newRow("fully-readable") << 0444 << true;
    QTest::newRow("user-readable") << 0400 << true;
    QTest::newRow("not-readable-by-us") << 0004 << false;
    QTest::newRow("not-readable-at-all") << 0000 << false;
}

void KFileItemTest::testIsReadable()
{
    QFETCH(int, mode);
    QFETCH(bool, readable);

    QTemporaryFile file;
    QVERIFY(file.open());
    int ret = fchmod(file.handle(), (mode_t)mode);
    QCOMPARE(ret, 0);

    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
    QCOMPARE(fileItem.isReadable(), readable);
}

#endif
