/*
 *
 * This file is part of the KDE project.
 * Copyright (C) 2001 Carsten Pfeiffer <pfeiffer@kde.org>
 *
 * You can Freely distribute this program under the GNU Library General Public
 * License. See the file "COPYING" for the exact licensing terms.
 */

#include <qapplication.h>
#include <kurlrequester.h>
#include <kurlrequesterdialog.h>
#include <QDebug>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    QUrl url = KUrlRequesterDialog::getUrl(QUrl(QStringLiteral("ftp://ftp.kde.org")));
    qDebug() << "Selected url:" << url;

    KUrlRequester *req = new KUrlRequester();
    KEditListWidget *el = new KEditListWidget(req->customEditor());
    el->setWindowTitle(QStringLiteral("Test"));
    el->show();

    KUrlRequester *req1 = new KUrlRequester();
    req1->setWindowTitle(QStringLiteral("AAAAAAAAAAAA"));
    req1->show();

    KUrlComboRequester *comboReq = new KUrlComboRequester();
    comboReq->setWindowTitle(QStringLiteral("KUrlComboRequester"));
    comboReq->show();

    return app.exec();
}
