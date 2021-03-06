/*
    kshorturifilter.h

    This file is part of the KDE project
    Copyright (C) 2000 Dawit Alemayehu <adawit@kde.org>
    Copyright (C) 2000 Malte Starostik <starosti@zedat.fu-berlin.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef KSHORTURIFILTER_H
#define KSHORTURIFILTER_H

#include <QtCore/QList>
#include <QtCore/QRegExp>
#include <QtCore/QVariant>

#include <kurifilter.h>


/**
* This is short URL filter class.
*
* @short A filter that converts short URLs into fully qualified ones.
*
* @author Dawit Alemayehu <adawit@kde.org>
* @author Malte Starostik <starosti@zedat.fu-berlin.de>
*/
class KShortUriFilter : public KUriFilterPlugin
{
    Q_OBJECT
public:

    /**
     * Creates a Short URI filter object
     *
     * @param parent the parent of this class.
     * @param name the internal name for this object.
     */
    explicit KShortUriFilter( QObject *parent = 0, const QVariantList &args = QVariantList() );

    /**
     * Destructor
     */
    virtual ~KShortUriFilter() {}

    /**
     * Converts short URIs into fully qualified valid URIs
     * whenever possible.
     *
     * Parses any given invalid URI to determine whether it
     * is a known short URI and converts it to its fully
     * qualified version.
     *
     * @param data the data to be filtered
     * @return true if the url has been filtered
     */
    bool filterUri( KUriFilterData &data ) const Q_DECL_OVERRIDE;

    /**
     * Returns the name of the config module for
     * this plugin.
     *
     * @return the name of the config module.
     */
    QString configName() const Q_DECL_OVERRIDE;

    /**
     * Returns an instance of the module used to configure
     * this object.
         *
         * @return the config module
         */
    KCModule* configModule( QWidget*, const char* ) const Q_DECL_OVERRIDE;

public Q_SLOTS:
    void configure();

private:

    struct URLHint
    {
        URLHint() {}
        URLHint( QString r, QString p,
                 KUriFilterData::UriTypes t = KUriFilterData::NetProtocol )
               : regexp(QRegExp(r)), prepend(p), type(t) {}
        QRegExp regexp; // if this matches, then...
        QString prepend; // ...prepend this to the url
        KUriFilterData::UriTypes type;
    };

    QList<URLHint> m_urlHints;
    QString m_strDefaultUrlScheme;
};

#endif
