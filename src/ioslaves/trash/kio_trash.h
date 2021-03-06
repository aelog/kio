/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>

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

#ifndef KIO_TRASH_H
#define KIO_TRASH_H

#include <kio/slavebase.h>
#include "trashimpl.h"
namespace KIO
{
class Job;
}

typedef TrashImpl::TrashedFileInfo TrashedFileInfo;
typedef TrashImpl::TrashedFileInfoList TrashedFileInfoList;

class TrashProtocol : public QObject, public KIO::SlaveBase
{
    Q_OBJECT
public:
    TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    virtual ~TrashProtocol();
    void stat(const QUrl &url) Q_DECL_OVERRIDE;
    void listDir(const QUrl &url) Q_DECL_OVERRIDE;
    void get(const QUrl &url) Q_DECL_OVERRIDE;
    void put(const QUrl &url, int, KIO::JobFlags flags) Q_DECL_OVERRIDE;
    void rename(const QUrl &src, const QUrl &dest, KIO::JobFlags) Q_DECL_OVERRIDE;
    void copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) Q_DECL_OVERRIDE;
    // TODO (maybe) chmod( const QUrl& url, int permissions );
    void del(const QUrl &url, bool isfile) Q_DECL_OVERRIDE;
    /**
     * Special actions: (first int in the byte array)
     * 1 : empty trash
     * 2 : migrate old (pre-kde-3.4) trash contents
     * 3 : restore a file to its original location. Args: QUrl trashURL.
     */
    void special(const QByteArray &data) Q_DECL_OVERRIDE;

Q_SIGNALS:
    void leaveModality();

protected:
    void virtual_hook(int id, void *data) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void slotData(KIO::Job *, const QByteArray &);
    void slotMimetype(KIO::Job *, const QString &);
    void jobFinished(KJob *job);

private:
    typedef enum { Copy, Move } CopyOrMove;
    void copyOrMove(const QUrl &src, const QUrl &dest, bool overwrite, CopyOrMove action);
    void createTopLevelDirEntry(KIO::UDSEntry &entry);
    bool createUDSEntry(const QString &physicalPath, const QString &displayFileName, const QString &internalFileName,
                        KIO::UDSEntry &entry, const TrashedFileInfo &info);
    void listRoot();
    void restore(const QUrl &trashURL);
    void enterLoop();
    void fileSystemFreeSpace(const QUrl &url);

    TrashImpl impl;
    QString m_userName;
    QString m_groupName;
};

#endif
