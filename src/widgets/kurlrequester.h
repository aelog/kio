/* This file is part of the KDE libraries
    Copyright (C) 1999,2000,2001 Carsten Pfeiffer <pfeiffer@kde.org>
    Copyright (C) 2013           Teo Mrnjavac <teo@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KURLREQUESTER_H
#define KURLREQUESTER_H

#include "kiowidgets_export.h"

#include <keditlistwidget.h>
#include <kfile.h>

#include <QFileDialog>
#include <QPushButton>
#include <QUrl>

class KComboBox;
class KLineEdit;
class KUrlCompletion;

class QEvent;
class QPushButton;
class QString;

/**
 * This class is a widget showing a lineedit and a button, which invokes a
 * filedialog. File name completion is available in the lineedit.
 *
 * The defaults for the filedialog are to ask for one existing local file.
 * The default filter is "*", i.e. show all files, and the start directory is
 * the current working directory, or the last directory where a file has been
 * selected.
 *
 * You can change this behavior by using setMode(), setFilter() and setStartDir().
 *
 * The default window modality for the file dialog is Qt::ApplicationModal
 *
 * \image html kurlrequester.png "KDE URL Requester"
 *
 * @short A widget to request a filename/url from the user
 * @author Carsten Pfeiffer <pfeiffer@kde.org>
 */
class KIOWIDGETS_EXPORT KUrlRequester : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY textChanged USER true)
    Q_PROPERTY(QString filter READ filter WRITE setFilter)
    Q_PROPERTY(KFile::Modes mode READ mode WRITE setMode)
#ifndef KIOWIDGETS_NO_DEPRECATED
    Q_PROPERTY(QString clickMessage READ clickMessage WRITE setClickMessage)
#endif
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::WindowModality fileDialogModality READ fileDialogModality WRITE setFileDialogModality)

public:
    /**
     * Constructs a KUrlRequester widget.
     */
    explicit KUrlRequester(QWidget *parent = 0);

    /**
     * Constructs a KUrlRequester widget with the initial URL @p url.
     */
    explicit KUrlRequester(const QUrl &url, QWidget *parent = 0);

    /**
     * Special constructor, which creates a KUrlRequester widget with a custom
     * edit-widget. The edit-widget can be either a KComboBox or a KLineEdit
     * (or inherited thereof). Note: for geometry management reasons, the
     * edit-widget is reparented to have the KUrlRequester as parent.
     */
    KUrlRequester(QWidget *editWidget, QWidget *parent);
    /**
     * Destructs the KUrlRequester.
     */
    ~KUrlRequester();

    /**
     * @returns the current url in the lineedit. May be malformed, if the user
     * entered something weird. For local files, ~user or environment variables
     * are substituted, relative paths will be resolved against startDir()
     */
    QUrl url() const;

    /**
     * @returns the current start dir
     * @since 4.3
     */
    QUrl startDir() const;

    /**
     * @returns the current text in the lineedit or combobox.
     * This does not do the URL expansion that url() does, it's only provided
     * for cases where KUrlRequester is used to enter URL-or-something-else,
     * like KOpenWithDialog where you can type a full command with arguments.
     *
     * @since 4.2
     */
    QString text() const;

    /**
     * Sets the mode of the file dialog.
     * Note: you can only select one file with the filedialog,
     * so KFile::Files doesn't make much sense.
     * @see QFileDialog::setFileMode()
     */
    void setMode(KFile::Modes m);

    /**
    * Returns the current mode
    * @see QFileDialog::fileMode()
    */
    KFile::Modes mode() const;

    /**
     * Sets the filters for the file dialog, separated by \n.
     * @see QFileDialog::setNameFilters()
     */
    void setFilter(const QString &filter);

    /**
    * Returns the filters for the file dialog, separated by \n.
    * @see QFileDialog::nameFilters()
    */
    QString filter() const;

    /**
     * @returns a pointer to the filedialog.
     * You can use this to customize the dialog, e.g. to call setLocationLabel
     * or other things which are not accessible in the KUrlRequester API.
     *
     * Never returns 0. This method creates the file dialog on demand.
     *
     * @deprecated since 5.0. The dialog will be created anyway when the user
     * requests it, and will behave according to the properties of KUrlRequester.
     */
#ifndef KIOWIDGETS_NO_DEPRECATED
    virtual KIOWIDGETS_DEPRECATED QFileDialog *fileDialog() const;
#endif

    /**
     * @returns a pointer to the lineedit, either the default one, or the
     * special one, if you used the special constructor.
     *
     * It is provided so that you can e.g. set an own completion object
     * (e.g. KShellCompletion) into it.
     */
    KLineEdit *lineEdit() const;

    /**
     * @returns a pointer to the combobox, in case you have set one using the
     * special constructor. Returns 0L otherwise.
     */
    KComboBox *comboBox() const;

    /**
     * @returns a pointer to the pushbutton. It is provided so that you can
     * specify an own pixmap or a text, if you really need to.
     */
    QPushButton *button() const;

    /**
     * @returns the KUrlCompletion object used in the lineedit/combobox.
     */
    KUrlCompletion *completionObject() const;

    /**
     * @returns an object, suitable for use with KEditListWidget. It allows you
     * to put this KUrlRequester into a KEditListWidget.
     * Basically, do it like this:
     * \code
     * KUrlRequester *req = new KUrlRequester( someWidget );
     * [...]
     * KEditListWidget *editListWidget = new KEditListWidget( req->customEditor(), someWidget );
     * \endcode
     */
    const KEditListWidget::CustomEditor &customEditor();

    /**
     * @returns the message set with setClickMessage
     * @since 4.2
     * @deprecated use KUrlRequester::placeholderText instead.
     */
#ifndef KIOWIDGETS_NO_DEPRECATED
    KIOWIDGETS_DEPRECATED QString clickMessage() const;
#endif

    /**
     * Set a click message @p msg
     * @since 4.2
     * @deprecated use KUrlRequester::setPlaceholderText instead.
     */
#ifndef KIOWIDGETS_NO_DEPRECATED
    KIOWIDGETS_DEPRECATED void setClickMessage(const QString &msg);
#endif

    /**
     * @return the message set with setPlaceholderText
     * @since 5.0
    */
    QString placeholderText() const;

    /**
     * This makes the KUrlRequester line edit display a grayed-out hinting text as long as
     * the user didn't enter any text. It is often used as indication about
     * the purpose of the line edit.
     * @since 5.0
    */
    void setPlaceholderText(const QString &msg);

    /**
     * @returns the window modality of the file dialog set with setFileDialogModality
     * @since 4.4
     */
    Qt::WindowModality fileDialogModality() const;

    /**
     * Set the window modality for the file dialog to @p modality
     * Directory selection dialogs are always modal
     * @since 4.4
     */
    void setFileDialogModality(Qt::WindowModality modality);

public Q_SLOTS:
    /**
     * Sets the url in the lineedit to @p url.
     */
    void setUrl(const QUrl &url);

    /**
     * Sets the start dir @p startDir.
     * The start dir is only used when the URL isn't set.
     * @since 4.3
     */
    void setStartDir(const QUrl &startDir);

    /**
     * Sets the url in the lineedit to @p QUrl::fromLocalFile(path).
     * This is only for local paths; do not pass a url here.
     * This method is mostly for "local paths only" url requesters,
     * for instance those set up with setMode(KFile::File|KFile::ExistingOnly|KFile::LocalOnly)
     *
     * @deprecated Use setUrl(QUrl::fromLocalFile(path)) instead.
     */
#ifndef KIOWIDGETS_NO_DEPRECATED
    KIOWIDGETS_DEPRECATED void setPath(const QString &path);
#endif

    /**
     * Sets the current text in the lineedit or combobox.
     * This is used for cases where KUrlRequester is used to
     * enter URL-or-something-else, like KOpenWithDialog where you
     * can type a full command with arguments.
     *
     * @see text
     * @since 4.3
     */
    void setText(const QString &text);

    /**
     * Clears the lineedit/combobox.
     */
    void clear();

Q_SIGNALS:
    // forwards from LineEdit
    /**
     * Emitted when the text in the lineedit changes.
     * The parameter contains the contents of the lineedit.
     */
    void textChanged(const QString &);

    /**
     * Emitted when the text in the lineedit was modified by the user.
     * Unlike textChanged(), this signal is not emitted when the text is changed programmatically, for example, by calling setText().
     * @since 5.21
     */
    void textEdited(const QString &);

    /**
     * Emitted when return or enter was pressed in the lineedit.
     */
    void returnPressed();

    /**
     * Emitted when return or enter was pressed in the lineedit.
     * The parameter contains the contents of the lineedit.
     */
    void returnPressed(const QString &);

    /**
     * Emitted before the filedialog is going to open. Connect
     * to this signal to "configure" the filedialog, e.g. set the
     * filefilter, the mode, a preview-widget, etc. It's usually
     * not necessary to set a URL for the filedialog, as it will
     * get set properly from the editfield contents.
     *
     * If you use multiple KUrlRequesters, you can connect all of them
     * to the same slot and use the given KUrlRequester pointer to know
     * which one is going to open.
     */
    void openFileDialog(KUrlRequester *);

    /**
     * Emitted when the user changed the URL via the file dialog.
     * The parameter contains the contents of the lineedit.
     */
    void urlSelected(const QUrl &);

protected:
    void changeEvent(QEvent *e) Q_DECL_OVERRIDE;
    bool eventFilter(QObject *obj, QEvent *ev) Q_DECL_OVERRIDE;

private:
    class KUrlRequesterPrivate;
    KUrlRequesterPrivate *const d;

    Q_DISABLE_COPY(KUrlRequester)

    Q_PRIVATE_SLOT(d, void _k_slotUpdateUrl())
    Q_PRIVATE_SLOT(d, void _k_slotOpenDialog())
    Q_PRIVATE_SLOT(d, void _k_slotFileDialogAccepted())

};

class KIOWIDGETS_EXPORT KUrlComboRequester : public KUrlRequester // krazy:exclude=dpointer (For use in Qt Designer)
{
    Q_OBJECT
public:
    /**
     * Constructs a KUrlRequester widget with a combobox.
     */
    explicit KUrlComboRequester(QWidget *parent = 0);

private:
    class Private;
    Private *const d;
};

#endif // KURLREQUESTER_H
