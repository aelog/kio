/*
    This file is part of the KDE Cookie Jar

    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this library; see the file COPYING. If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/
//----------------------------------------------------------------------------
//
// KDE Password Server
// $Id$

#include <time.h>

#include <qtimer.h>

#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kio/passdlg.h>
#include <kwallet.h>

#include "config.h"
#if defined Q_WS_X11 && ! defined K_WS_QTONLY
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include "kpasswdserver.h"

extern "C" {
    KDE_EXPORT KDEDModule *create_kpasswdserver(const QCString &name)
    {
       return new KPasswdServer(name);
    }
}

int
KPasswdServer::AuthInfoList::compareItems(QPtrCollection::Item n1, QPtrCollection::Item n2)
{
   if (!n1 || !n2)
      return 0;

   AuthInfo *i1 = (AuthInfo *) n1;
   AuthInfo *i2 = (AuthInfo *) n2;

   int l1 = i1->directory.length();
   int l2 = i2->directory.length();

   if (l1 > l2)
      return -1;
   if (l1 < l2)
      return 1;
   return 0;
}


KPasswdServer::KPasswdServer(const QCString &name)
 : KDEDModule(name)
{
    m_authDict.setAutoDelete(true);
    m_authPending.setAutoDelete(true);
    m_seqNr = 0;
    connect(this, SIGNAL(windowUnregistered(long)),
            this, SLOT(removeAuthForWindowId(long)));
}

KPasswdServer::~KPasswdServer()
{
}

KIO::AuthInfo
KPasswdServer::checkAuthInfo(KIO::AuthInfo info, long windowId)
{
    kdDebug(130) << "KPasswdServer::checkAuthInfo: User= " << info.username
              << ", WindowId = " << windowId << endl;

    QString key = createCacheKey(info);

    Request *request = m_authPending.first();
    QString path2 = info.url.directory(false, false);
    for(; request; request = m_authPending.next())
    {
       if (request->key != key)
           continue;

       if (info.verifyPath)
       {
          QString path1 = request->info.url.directory(false, false);
          if (!path2.startsWith(path1))
             continue;
       }

       request = new Request;
       request->client = callingDcopClient();
       request->transaction = request->client->beginTransaction();
       request->key = key;
       request->info = info;
       m_authWait.append(request);
       return info;
    }

    const AuthInfo *result = findAuthInfoItem(key, info);
    if (!result || result->isCanceled)
    {
       info.setModified(false);
       return info;
    }

    updateAuthExpire(key, result, windowId, false);

    return copyAuthInfo(result);
}

KIO::AuthInfo
KPasswdServer::queryAuthInfo(KIO::AuthInfo info, QString errorMsg, long windowId, long seqNr)
{
    kdDebug(130) << "KPasswdServer::queryAuthInfo: User= " << info.username
              << ", Message= " << info.prompt << ", WindowId = " << windowId << endl;
    QString key = createCacheKey(info);
    Request *request = new Request;
    request->client = callingDcopClient();
    request->transaction = request->client->beginTransaction();
    request->key = key;
    request->info = info;
    request->windowId = windowId;
    request->seqNr = seqNr;
    if (errorMsg == "<NoAuthPrompt>")
    {
       request->errorMsg = QString::null;
       request->prompt = false;
    }
    else
    {
       request->errorMsg = errorMsg;
       request->prompt = true;
    }
    m_authPending.append(request);

    if (m_authPending.count() == 1)
       QTimer::singleShot(0, this, SLOT(processRequest()));

    return info;
}

void
KPasswdServer::addAuthInfo(KIO::AuthInfo info, long windowId)
{
    kdDebug(130) << "KPasswdServer::addAuthInfo: User= " << info.username
              << ", RealmValue= " << info.realmValue << ", WindowId = " << windowId << endl;
    QString key = createCacheKey(info);

    m_seqNr++;

    addAuthInfoItem(key, info, windowId, m_seqNr, false);
}

void
KPasswdServer::processRequest()
{
    Request *request = m_authPending.first();
    if (!request)
       return;

    KIO::AuthInfo &info = request->info;

    kdDebug(130) << "KPasswdServer::processRequest: User= " << info.username
              << ", Message= " << info.prompt << endl;

    const AuthInfo *result = findAuthInfoItem(request->key, request->info);

    if (result && (request->seqNr < result->seqNr))
    {
        kdDebug(130) << "KPasswdServer::processRequest: auto retry!" << endl;
        if (result->isCanceled)
        {
           info.setModified(false);
        }
        else
        {
           updateAuthExpire(request->key, result, request->windowId, false);
           info = copyAuthInfo(result);
        }
    }
    else
    {
        m_seqNr++;
        bool askPw = request->prompt;
        if (result && !info.username.isEmpty() &&
            !request->errorMsg.isEmpty())
        {
           QString prompt = request->errorMsg;
           prompt += i18n("  Do you want to retry?");
           int dlgResult = KMessageBox::warningContinueCancel(0, prompt,
                           i18n("Authentication"), i18n("Retry"));
           if (dlgResult != KMessageBox::Continue)
              askPw = false;
        }

        int dlgResult = QDialog::Rejected;
        if (askPw)
        {
            QString username = info.username;
            QString password = info.password;
            bool hasWalletData = false;

            KWallet::Wallet* wallet = 0;
            if ( ( username.isEmpty() || password.isEmpty() )
                && !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(), KWallet::Wallet::PasswordFolder(), request->key) )
            {
                // no login+pass provided, check if kwallet has one
                wallet = KWallet::Wallet::openWallet(
                    KWallet::Wallet::NetworkWallet(), request->windowId );
                if ( wallet && wallet->hasFolder( KWallet::Wallet::PasswordFolder() ) )
                {
                    wallet->setFolder( KWallet::Wallet::PasswordFolder() );
                    QMap<QString,QString> map;
                    if ( wallet->readMap( request->key, map ) == 0 )
                    {
                        QMap<QString, QString>::ConstIterator it = map.find( "password" );
                        if ( it != map.end() )
                            password = it.data();

                        if ( !info.readOnly ) {
                            it = map.find( "login" );
                            if ( it != map.end() )
                                username = it.data();
                        }
                        hasWalletData = true;
                    }
                }
            }

            KIO::PasswordDialog dlg( info.prompt, username, info.keepPassword );
            if (info.caption.isEmpty())
               dlg.setPlainCaption( i18n("Authorization Dialog") );
            else
               dlg.setPlainCaption( info.caption );

            if ( !info.comment.isEmpty() )
               dlg.addCommentLine( info.commentLabel, info.comment );

            if ( !password.isEmpty() )
               dlg.setPassword( password );

            if (info.readOnly)
               dlg.setUserReadOnly( true );

            if (hasWalletData)
                dlg.setKeepPassword( true );

#if defined Q_WS_X11 && ! defined K_WS_QTONLY
            XSetTransientForHint( qt_xdisplay(), dlg.winId(), request->windowId);
#endif

            dlgResult = dlg.exec();

            if (dlgResult == QDialog::Accepted)
            {
               info.username = dlg.username();
               info.password = dlg.password();
               info.keepPassword = dlg.keepPassword();

               // When the user checks "keep password", that means both in the cache (kpasswdserver process)
               // and in the wallet, if enabled.
               if ( info.keepPassword ) {
                   if ( !wallet )
                       wallet = KWallet::Wallet::openWallet(
                           KWallet::Wallet::NetworkWallet(), request->windowId );
                   QString password;
                   if ( wallet ) {
                       bool ok = true;
                       if ( !wallet->hasFolder( KWallet::Wallet::PasswordFolder() ) )
                           ok = wallet->createFolder( KWallet::Wallet::PasswordFolder() );
                       if ( ok )
                       {
                           wallet->setFolder( KWallet::Wallet::PasswordFolder() );
                           QMap<QString,QString> map;
                           map.insert( "login", info.username );
                           map.insert( "password", info.password );
                           wallet->writeMap( request->key, map );
                       }
                   }
               }
            }
            delete wallet;
        }
        if ( dlgResult != QDialog::Accepted )
        {
            addAuthInfoItem(request->key, info, 0, m_seqNr, true);
            info.setModified( false );
        }
        else
        {
            addAuthInfoItem(request->key, info, request->windowId, m_seqNr, false);
            info.setModified( true );
        }
    }

    QCString replyType;
    QByteArray replyData;

    QDataStream stream2(replyData, IO_WriteOnly);
    stream2 << info << m_seqNr;
    replyType = "KIO::AuthInfo";
    request->client->endTransaction( request->transaction,
                                     replyType, replyData);

    m_authPending.remove((unsigned int) 0);

    // Check all requests in the wait queue.
    for(Request *waitRequest = m_authWait.first();
        waitRequest; )
    {
       bool keepQueued = false;
       QString key = waitRequest->key;

       request = m_authPending.first();
       QString path2 = waitRequest->info.url.directory(false, false);
       for(; request; request = m_authPending.next())
       {
           if (request->key != key)
               continue;

           if (info.verifyPath)
           {
               QString path1 = request->info.url.directory(false, false);
               if (!path2.startsWith(path1))
                   continue;
           }

           keepQueued = true;
           break;
       }
       if (keepQueued)
       {
           waitRequest = m_authWait.next();
       }
       else
       {
           const AuthInfo *result = findAuthInfoItem(waitRequest->key, waitRequest->info);

           QCString replyType;
           QByteArray replyData;

           QDataStream stream2(replyData, IO_WriteOnly);

           if (!result || result->isCanceled)
           {
               waitRequest->info.setModified(false);
               stream2 << waitRequest->info;
           }
           else
           {
               updateAuthExpire(waitRequest->key, result, waitRequest->windowId, false);
               KIO::AuthInfo info = copyAuthInfo(result);
               stream2 << info;
           }

           replyType = "KIO::AuthInfo";
           waitRequest->client->endTransaction( waitRequest->transaction,
                                                replyType, replyData);

           m_authWait.remove();
           waitRequest = m_authWait.current();
       }
    }

    if (m_authPending.count())
       QTimer::singleShot(0, this, SLOT(processRequest()));

}

QString KPasswdServer::createCacheKey( const KIO::AuthInfo &info )
{
    if( !info.url.isValid() )
        return QString::null;

    // Generate the basic key sequence.
    QString key = info.url.protocol();
    key += '-';
    if (!info.url.user().isEmpty())
    {
       key += info.url.user();
       key += "@";
    }
    key += info.url.host();
    int port = info.url.port();
    if( port )
    {
      key += ':';
      key += QString::number(port);
    }

    return key;
}

KIO::AuthInfo
KPasswdServer::copyAuthInfo(const AuthInfo *i)
{
    KIO::AuthInfo result;
    result.url = i->url;
    result.username = i->username;
    result.password = i->password;
    result.realmValue = i->realmValue;
    result.digestInfo = i->digestInfo;
    result.setModified(true);

    return result;
}

const KPasswdServer::AuthInfo *
KPasswdServer::findAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
   AuthInfoList *authList = m_authDict.find(key);
   if (!authList)
      return 0;

   QString path2 = info.url.directory(false, false);
   for(AuthInfo *current = authList->first();
       current; )
   {
       if ((current->expire == AuthInfo::expTime) &&
          (difftime(time(0), current->expireTime) > 0))
       {
          authList->remove();
          current = authList->current();
          continue;
       }

       if (info.verifyPath)
       {
          QString path1 = current->directory;
          if (path2.startsWith(path1) &&
              (info.username.isEmpty() || info.username == current->username))
             return current;
       }
       else
       {
          if (current->realmValue == info.realmValue &&
              (info.username.isEmpty() || info.username == current->username))
             return current; // TODO: Update directory info,
       }

       current = authList->next();
   }
   return 0;
}

void
KPasswdServer::removeAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
   AuthInfoList *authList = m_authDict.find(key);
   if (!authList)
      return;

   for(AuthInfo *current = authList->first();
       current; )
   {
       if (current->realmValue == info.realmValue)
       {
          authList->remove();
          current = authList->current();
       }
       else
       {
          current = authList->next();
       }
   }
   if (authList->isEmpty())
   {
       m_authDict.remove(key);
   }
}


void
KPasswdServer::addAuthInfoItem(const QString &key, const KIO::AuthInfo &info, long windowId, long seqNr, bool canceled)
{
   AuthInfoList *authList = m_authDict.find(key);
   if (!authList)
   {
      authList = new AuthInfoList;
      m_authDict.insert(key, authList);
   }
   AuthInfo *current = authList->first();
   for(; current; current = authList->next())
   {
       if (current->realmValue == info.realmValue)
       {
          authList->take();
          break;
       }
   }

   if (!current)
   {
      current = new AuthInfo;
      current->expire = AuthInfo::expTime;
      kdDebug(130) << "Creating AuthInfo" << endl;
   }
   else
   {
      kdDebug(130) << "Updating AuthInfo" << endl;
   }

   current->url = info.url;
   current->directory = info.url.directory(false, false);
   current->username = info.username;
   current->password = info.password;
   current->realmValue = info.realmValue;
   current->digestInfo = info.digestInfo;
   current->seqNr = seqNr;
   current->isCanceled = canceled;

   updateAuthExpire(key, current, windowId, info.keepPassword && !canceled);

   // Insert into list, keep the list sorted "longest path" first.
   authList->inSort(current);
}

void
KPasswdServer::updateAuthExpire(const QString &key, const AuthInfo *auth, long windowId, bool keep)
{
   AuthInfo *current = const_cast<AuthInfo *>(auth);
   if (keep)
   {
      current->expire = AuthInfo::expNever;
   }
   else if (windowId && (current->expire != AuthInfo::expNever))
   {
      current->expire = AuthInfo::expWindowClose;
      if (!current->windowList.contains(windowId))
         current->windowList.append(windowId);
   }
   else if (current->expire == AuthInfo::expTime)
   {
      current->expireTime = time(0)+10;
   }

   // Update mWindowIdList
   if (windowId)
   {
      QStringList *keysChanged = mWindowIdList.find(windowId);
      if (!keysChanged)
      {
         keysChanged = new QStringList;
         mWindowIdList.insert(windowId, keysChanged);
      }
      if (!keysChanged->contains(key))
         keysChanged->append(key);
   }
}

void
KPasswdServer::removeAuthForWindowId(long windowId)
{
   QStringList *keysChanged = mWindowIdList.find(windowId);
   if (!keysChanged) return;

   for(QStringList::ConstIterator it = keysChanged->begin();
       it != keysChanged->end(); ++it)
   {
      QString key = *it;
      AuthInfoList *authList = m_authDict.find(key);
      if (!authList)
         continue;

      AuthInfo *current = authList->first();
      for(; current; )
      {
        if (current->expire == AuthInfo::expWindowClose)
        {
           if (current->windowList.remove(windowId) && current->windowList.isEmpty())
           {
              authList->remove();
              current = authList->current();
              continue;
           }
        }
        current = authList->next();
      }
   }
}

#include "kpasswdserver.moc"
