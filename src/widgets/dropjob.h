/* This file is part of the KDE libraries
    Copyright (C) 2014 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef DROPJOB_H
#define DROPJOB_H

#include <QUrl>

#include "kiowidgets_export.h"
#include <kio/job_base.h>

class QAction;
class QDropEvent;
class KFileItemListProperties;

namespace KIO
{

class DropJobPrivate;

/**
 * A KIO job that handles dropping into a file-manager-like view.
 * @see KIO::drop
 *
 * The popupmenu that can appear on drop, can be customized with plugins,
 * see KIO::DndPopupMenuPlugin.
 *
 * @since 5.6
 */
class KIOWIDGETS_EXPORT DropJob : public Job
{
    Q_OBJECT

public:
    virtual ~DropJob();

    /**
     * Allows the application to set additional actions in the drop popup menu.
     * For instance, the application handling the desktop might want to add
     * "set as wallpaper" if the dropped url is an image file.
     * This can be called upfront, or for convenience, when popupMenuAboutToShow is emitted.
     */
    void setApplicationActions(const QList<QAction *> &actions);

Q_SIGNALS:
    /**
     * Signals that a file or directory was created.
     */
    void itemCreated(const QUrl &url);

    /**
     * Signals that the popup menu is about to be shown.
     * Applications can use the information provided about the dropped URLs
     * (e.g. the mimetype) to decide whether to call setApplicationActions.
     * @param itemProps properties of the dropped items
     */
    void popupMenuAboutToShow(const KFileItemListProperties &itemProps);

protected Q_SLOTS:
    void slotResult(KJob *job) Q_DECL_OVERRIDE;

protected:
    DropJob(DropJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(DropJob)
    Q_PRIVATE_SLOT(d_func(), void slotStart())
};

/**
 * Drops the clipboard contents.
 *
 * If the mime data contains URLs, a popup appears to choose between
 *  Move, Copy, Link and Cancel
 * which is then implemented by the job, using KIO::move, KIO::copy or KIO::link
 * Additional actions provided by the application or by plugins can be shown in the popup.
 *
 * If the mime data contains other data than URLs, it is saved into a file after asking
 * the user to choose a filename and the preferred data format.
 *
 * This job takes care of recording the subjob in the FileUndoManager, and emits
 * itemCreated for every file or directory being created, so that the view can select
 * these items.
 *
 * @param dropEvent the drop event, from which the job will extract mimeData, dropAction, etc.
         The application should take care of calling dropEvent->acceptProposedAction().
 * @param destUrl The URL of the target file or directory
 * @param flags passed to the sub job
 *
 * @return A pointer to the job handling the operation.
 * @since 5.4
 */
KIOWIDGETS_EXPORT DropJob *drop(const QDropEvent *dropEvent, const QUrl &destUrl, JobFlags flags = DefaultFlags);

}

#endif
