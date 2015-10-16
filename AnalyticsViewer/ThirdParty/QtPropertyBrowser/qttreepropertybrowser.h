/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QTTREEPROPERTYBROWSER_H
#define QTTREEPROPERTYBROWSER_H

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QItemDelegate>
#include "qtpropertybrowser.h"

QT_BEGIN_NAMESPACE

class QTreeWidgetItem;
class QtTreePropertyBrowserPrivate;

// ------------ QtPropertyEditorView
class QtPropertyEditorView : public QTreeWidget
{
	Q_OBJECT
public:
	QtPropertyEditorView( QWidget *parent = 0 );

	void setEditorPrivate( QtTreePropertyBrowserPrivate *editorPrivate )
	{
		m_editorPrivate = editorPrivate;
	}

	QTreeWidgetItem *indexToItem( const QModelIndex &index ) const
	{
		return itemFromIndex( index );
	}

protected:
	void keyPressEvent( QKeyEvent *event );
	void mousePressEvent( QMouseEvent *event );
	void drawRow( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const;

private:
	QtTreePropertyBrowserPrivate *m_editorPrivate;
};

// ------------ QtPropertyEditorDelegate
class QtPropertyEditorDelegate : public QItemDelegate
{
	Q_OBJECT
public:
	QtPropertyEditorDelegate( QObject *parent = 0 )
		: QItemDelegate( parent ), m_editorPrivate( 0 ), m_editedItem( 0 ), m_editedWidget( 0 )
	{
	}

	void setEditorPrivate( QtTreePropertyBrowserPrivate *editorPrivate )
	{
		m_editorPrivate = editorPrivate;
	}

	QWidget *createEditor( QWidget *parent, const QStyleOptionViewItem &option,
		const QModelIndex &index ) const;

	void updateEditorGeometry( QWidget *editor, const QStyleOptionViewItem &option,
		const QModelIndex &index ) const;

	void paint( QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index ) const;

	QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const;

	void setModelData( QWidget *, QAbstractItemModel *,
		const QModelIndex & ) const
	{
	}

	void setEditorData( QWidget *, const QModelIndex & ) const
	{
	}

	bool eventFilter( QObject *object, QEvent *event );
	void closeEditor( QtProperty *property );

	QTreeWidgetItem *editedItem() const
	{
		return m_editedItem;
	}

	private slots:
	void slotEditorDestroyed( QObject *object );

private:
	int indentation( const QModelIndex &index ) const;

	typedef QMap<QWidget *, QtProperty *> EditorToPropertyMap;
	mutable EditorToPropertyMap m_editorToProperty;

	typedef QMap<QtProperty *, QWidget *> PropertyToEditorMap;
	mutable PropertyToEditorMap m_propertyToEditor;
	QtTreePropertyBrowserPrivate *m_editorPrivate;
	mutable QTreeWidgetItem *m_editedItem;
	mutable QWidget *m_editedWidget;
};

class QtMetaEnumWrapper : public QObject
{
	Q_OBJECT
		Q_PROPERTY( QSizePolicy::Policy policy READ policy )
public:
	QSizePolicy::Policy policy() const
	{
		return QSizePolicy::Ignored;
	}
private:
	QtMetaEnumWrapper( QObject *parent ) : QObject( parent )
	{
	}
};

class QtTreePropertyBrowser : public QtAbstractPropertyBrowser
{
    Q_OBJECT
    Q_ENUMS(ResizeMode)
    Q_PROPERTY(int indentation READ indentation WRITE setIndentation)
    Q_PROPERTY(bool rootIsDecorated READ rootIsDecorated WRITE setRootIsDecorated)
    Q_PROPERTY(bool alternatingRowColors READ alternatingRowColors WRITE setAlternatingRowColors)
    Q_PROPERTY(bool headerVisible READ isHeaderVisible WRITE setHeaderVisible)
    Q_PROPERTY(ResizeMode resizeMode READ resizeMode WRITE setResizeMode)
    Q_PROPERTY(int splitterPosition READ splitterPosition WRITE setSplitterPosition)
    Q_PROPERTY(bool propertiesWithoutValueMarked READ propertiesWithoutValueMarked WRITE setPropertiesWithoutValueMarked)
public:

    enum ResizeMode
    {
        Interactive,
        Stretch,
        Fixed,
        ResizeToContents
    };

    QtTreePropertyBrowser(QWidget *parent = 0);
    ~QtTreePropertyBrowser();

    int indentation() const;
    void setIndentation(int i);

    bool rootIsDecorated() const;
    void setRootIsDecorated(bool show);

    bool alternatingRowColors() const;
    void setAlternatingRowColors(bool enable);

    bool isHeaderVisible() const;
    void setHeaderVisible(bool visible);

    ResizeMode resizeMode() const;
    void setResizeMode(ResizeMode mode);

    int splitterPosition() const;
    void setSplitterPosition(int position);

    void setExpanded(QtBrowserItem *item, bool expanded);
    bool isExpanded(QtBrowserItem *item) const;

    bool isItemVisible(QtBrowserItem *item) const;
    void setItemVisible(QtBrowserItem *item, bool visible);

    void setBackgroundColor(QtBrowserItem *item, const QColor &color);
    QColor backgroundColor(QtBrowserItem *item) const;
    QColor calculatedBackgroundColor(QtBrowserItem *item) const;

    void setPropertiesWithoutValueMarked(bool mark);
    bool propertiesWithoutValueMarked() const;

    void editItem(QtBrowserItem *item);

Q_SIGNALS:

    void collapsed(QtBrowserItem *item);
    void expanded(QtBrowserItem *item);

protected:
    virtual void itemInserted(QtBrowserItem *item, QtBrowserItem *afterItem);
    virtual void itemRemoved(QtBrowserItem *item);
    virtual void itemChanged(QtBrowserItem *item);

private:

    QScopedPointer<QtTreePropertyBrowserPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QtTreePropertyBrowser)
    Q_DISABLE_COPY(QtTreePropertyBrowser)

    Q_PRIVATE_SLOT(d_func(), void slotCollapsed(const QModelIndex &))
    Q_PRIVATE_SLOT(d_func(), void slotExpanded(const QModelIndex &))
    Q_PRIVATE_SLOT(d_func(), void slotCurrentBrowserItemChanged(QtBrowserItem *))
    Q_PRIVATE_SLOT(d_func(), void slotCurrentTreeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *))

};

QT_END_NAMESPACE

#endif
