/**     @file ModelPartList.cpp
 *
 *      EEEE2076 - Software Engineering & VR Project
 */

#include "ModelPartList.h"
#include "ModelPart.h"

ModelPartList::ModelPartList(const QString& data, QObject* parent)
    : QAbstractItemModel(parent)
{
    Q_UNUSED(data);
    /* Root item acts as the column headers */
    rootItem = new ModelPart({ tr("Part"), tr("Visible?") });
}

ModelPartList::~ModelPartList()
{
    delete rootItem;
}

int ModelPartList::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return rootItem->columnCount();
}

QVariant ModelPartList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())          return QVariant();
    if (role != Qt::DisplayRole)   return QVariant();

    ModelPart* item = static_cast<ModelPart*>(index.internalPointer());
    return item->data(index.column());
}

Qt::ItemFlags ModelPartList::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return QAbstractItemModel::flags(index);
}

QVariant ModelPartList::headerData(int section, Qt::Orientation orientation,
                                   int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);
    return QVariant();
}

QModelIndex ModelPartList::index(int row, int column,
                                 const QModelIndex& parent) const
{
    ModelPart* parentItem;

    if (!parent.isValid() || !hasIndex(row, column, parent))
        parentItem = rootItem;
    else
        parentItem = static_cast<ModelPart*>(parent.internalPointer());

    ModelPart* childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex ModelPartList::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    ModelPart* childItem  = static_cast<ModelPart*>(index.internalPointer());
    ModelPart* parentItem = childItem->parentItem();

    if (parentItem == rootItem || parentItem == nullptr)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int ModelPartList::rowCount(const QModelIndex& parent) const
{
    ModelPart* parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<ModelPart*>(parent.internalPointer());

    return parentItem->childCount();
}

ModelPart* ModelPartList::getRootItem() { return rootItem; }

QModelIndex ModelPartList::appendChild(QModelIndex& parent,
                                       const QList<QVariant>& data)
{
    ModelPart* parentPart;

    if (parent.isValid())
        parentPart = static_cast<ModelPart*>(parent.internalPointer());
    else
        parentPart = rootItem;

    const int newRow = parentPart->childCount();

    beginInsertRows(parent.isValid() ? parent : QModelIndex(),
                    newRow, newRow);

    ModelPart* childPart = new ModelPart(data, parentPart);
    parentPart->appendChild(childPart);

    endInsertRows();

    return createIndex(newRow, 0, childPart);
}

QModelIndex ModelPartList::appendChildToItem(ModelPart* parentItem,
                                             const QList<QVariant>& data)
{
    if (!parentItem) parentItem = rootItem;

    /* Build a model index for the parent (or invalid for the root) */
    QModelIndex parentIndex;
    if (parentItem != rootItem)
        parentIndex = createIndex(parentItem->row(), 0, parentItem);

    return appendChild(parentIndex, data);
}

bool ModelPartList::removePart(const QModelIndex& index)
{
    if (!index.isValid())
        return false;

    ModelPart* item = static_cast<ModelPart*>(index.internalPointer());
    if (!item) return false;

    ModelPart* parentItem = item->parentItem();
    if (!parentItem)
        return false;

    QModelIndex parentIndex = index.parent();
    int row = index.row();

    beginRemoveRows(parentIndex, row, row);
    parentItem->removeChild(row);
    endRemoveRows();

    return true;
}

bool ModelPartList::removeRows(int row, int count,
                               const QModelIndex& parent)
{
    ModelPart* parentItem;

    if (parent.isValid())
        parentItem = static_cast<ModelPart*>(parent.internalPointer());
    else
        parentItem = rootItem;

    if (!parentItem) return false;

    beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        parentItem->removeChild(row);
    }
    endRemoveRows();

    return true;
}
