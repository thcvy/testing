/**     @file ModelPartList.h
 *
 *      EEEE2076 - Software Engineering & VR Project
 *
 *      Tree model used to drive the QTreeView. Provides the typical
 *      QAbstractItemModel API plus convenience helpers for adding /
 *      removing parts in a way that correctly notifies the view.
 */
#ifndef VIEWER_MODELPARTLIST_H
#define VIEWER_MODELPARTLIST_H

#include "ModelPart.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QString>
#include <QList>

class ModelPart;

class ModelPartList : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit ModelPartList(const QString& data, QObject* parent = nullptr);
    ~ModelPartList() override;

    int          columnCount(const QModelIndex& parent) const override;
    QVariant     data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant     headerData(int section, Qt::Orientation orientation,
                            int role) const override;
    QModelIndex  index(int row, int column,
                       const QModelIndex& parent) const override;
    QModelIndex  parent(const QModelIndex& index) const override;
    int          rowCount(const QModelIndex& parent) const override;

    /** Get the root item of the tree (used to seed the tree at start-up). */
    ModelPart*   getRootItem();

    /** Append a new child to `parent` and return its index.
     *  If parent is invalid, the child is added under the root. */
    QModelIndex  appendChild(QModelIndex& parent, const QList<QVariant>& data);

    /** Convenience: append a child to a specific item. */
    QModelIndex  appendChildToItem(ModelPart* parentItem,
                                   const QList<QVariant>& data);

    /** Remove the item at the given index from the tree. */
    bool         removePart(const QModelIndex& index);

    /** QAbstractItemModel override - used by QTreeView when removing rows. */
    bool         removeRows(int row, int count,
                            const QModelIndex& parent = QModelIndex()) override;

private:
    ModelPart* rootItem;
};
#endif
