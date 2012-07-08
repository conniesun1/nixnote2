#include "ntagview.h"
#include "global.h"
#include "ntagviewitem.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QByteArray>
#include <QtSql>
#include "sql/tagtable.h"
#include "filters/filtercriteria.h"

#define NAME_POSITION 0

extern Global global;

// Constructor
NTagView::NTagView(QWidget *parent) :
    QTreeWidget(parent)
{
    QFont f = this->font();
    f.setPointSize(8);
    this->setFont(f);

    filterPosition = 0;
    // setup options
    this->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->setRootIsDecorated(true);
    this->setSortingEnabled(false);
    this->header()->setVisible(false);
    //QString qss = "QTreeWidget::branch:closed:has-children  {border-image: none; image: url(qss/branch-closed.png); } QTreeWidget::branch:open:has-children { border-image: none; image: url(qss/branch-open.png); }";
    //this->setStyleSheet(qss + QString("QTreeWidget { background-color: rgb(216,216,216);  border: none; image: url(qss/branch-closed.png);  }"));
    this->setStyleSheet("QTreeWidget {  border: none; background-color:transparent; }");

    // Build the root item
    QIcon icon(":tag.png");
    root = new NTagViewItem(this);
    root->setIcon(NAME_POSITION,icon);
    root->setData(NAME_POSITION, Qt::UserRole, "root");
    root->setData(NAME_POSITION, Qt::DisplayRole, tr("Tags"));
    root->setRootColor(true);
    this->setMinimumHeight(1);
    this->addTopLevelItem(root);
    this->rebuildTagTreeNeeded = true;
    this->loadData();
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(calculateHeight()));
    connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)), this, SLOT(calculateHeight()));
    connect(this, SIGNAL(itemSelectionChanged()), this, SLOT(buildSelection()));
}


// Destructor
NTagView::~NTagView() {
    delete root;
}


void NTagView::calculateHeight()
{
    int h = 0;

    int topLevelCount = topLevelItemCount();

    for(int i = 0;i < topLevelCount;i++)
    {
        QTreeWidgetItem * item = topLevelItem(i);
        h += calculateHeightRec(item);
        h += item->sizeHint(0).height() + 5;
    }

    if(h != 0)
    {
//        h += header()->sizeHint().height();

        setMinimumHeight(h);
        setMaximumHeight(h);
    }
}

int NTagView::calculateHeightRec(QTreeWidgetItem * item)
{
    if(!item)
        return 0;

    QModelIndex index = indexFromItem(item);

    if(!item->isExpanded())
    {
        return rowHeight(index);
    }

    int h = item->sizeHint(0).height() + 2 + rowHeight(index);
    int childCount = item->childCount();
    for(int i = 0; i<childCount; i++) {
        h += calculateHeightRec(item->child(i));
    }
    return h;
}



// This alows for the tree item to be toggled.  If the prior item is selected again
// it is deselected.  If it is the root item, we don't permit the selection.
void NTagView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex item = indexAt(event->pos());
    bool selected = selectionModel()->isSelected(indexAt(event->pos()));
    QTreeView::mousePressEvent(event);
    if (selected)
        selectionModel()->select(item, QItemSelectionModel::Deselect);

    for (int i=0; i<this->selectedItems() .size(); i++) {
        if (this->selectedIndexes().at(i).data(Qt::UserRole) == "root") {
            selectionModel()->select(this->selectedIndexes().at(i), QItemSelectionModel::Deselect);
        }
    }
}



// Load up the data from the database
void NTagView::loadData() {
    QSqlQuery query;
    TagTable tagTable;
    query.exec("Select lid, name, parent_gid from TagModel order by name");
    while (query.next()) {
        NTagViewItem *newWidget = new NTagViewItem();
        newWidget->setData(NAME_POSITION, Qt::DisplayRole, query.value(1).toString());
        newWidget->setData(NAME_POSITION, Qt::UserRole, query.value(0).toInt());
        this->dataStore.insert(query.value(0).toInt(), newWidget);
        newWidget->parentGuid = query.value(2).toString();
        newWidget->parentLid = tagTable.getLid(query.value(2).toString());
        root->addChild(newWidget);
    }
    this->rebuildTree();
}

void NTagView::rebuildTree() {
    if (!this->rebuildTagTreeNeeded)
        return;

    QHashIterator<int, NTagViewItem *> i(dataStore);
    TagTable tagTable;

    while (i.hasNext()) {
        i.next();
        NTagViewItem *widget = i.value();
        if (widget->parentGuid != "") {
            if (widget->parentLid == 0) {
                widget->parentLid = tagTable.getLid(widget->parentGuid);
            }
            NTagViewItem *parent = dataStore[widget->parentLid];
            widget->parent()->removeChild(widget);
            parent->childrenGuids.append(i.key());
            parent->addChild(widget);
        }
    }
    this->sortByColumn(NAME_POSITION, Qt::AscendingOrder);
    this->rebuildTagTreeNeeded = false;
    this->resetSize();
}

void NTagView::tagUpdated(int lid, QString name) {
    this->rebuildTagTreeNeeded = true;

    // Check if it already exists
    if (this->dataStore.contains(lid)) {
        TagTable tagTable;
        Tag tag;
        tagTable.get(tag, lid);
        NTagViewItem *newWidget = dataStore.value(lid);
        newWidget->setData(NAME_POSITION, Qt::DisplayRole, name);
        newWidget->setData(NAME_POSITION, Qt::UserRole, lid);
        newWidget->parentGuid = QString::fromStdString(tag.parentGuid);
        newWidget->parentLid = tagTable.getLid(tag.parentGuid);
        root->addChild(newWidget);
    } else {
        TagTable tagTable;
        Tag tag;
        tagTable.get(tag, lid);
        NTagViewItem *newWidget = new NTagViewItem();
        newWidget->setData(NAME_POSITION, Qt::DisplayRole, name);
        newWidget->setData(NAME_POSITION, Qt::UserRole, lid);
        newWidget->parentGuid = QString::fromStdString(tag.parentGuid);
        newWidget->parentLid = tagTable.getLid(tag.parentGuid);
        this->dataStore.insert(lid, newWidget);
        root->addChild(newWidget);
        if (this->dataStore.count() == 1) {
            this->expandAll();
        }
    }
    resetSize();
    this->sortByColumn(NAME_POSITION);
}


void NTagView::resetSize() {
    calculateHeight();
}


void NTagView::buildSelection() {
    QLOG_TRACE() << "Inside NTagView::buildSelection()";

    QList<QTreeWidgetItem*> selectedItems = this->selectedItems();
    if (selectedItems.size() > 0 && selectedItems[0]->data(0,Qt::UserRole) == "root")
        return;

    // First, find out if we're already viewing history.  If we are we
    // chop off the end of the history & start a new one
    if (global.filterPosition+1 < global.filterCriteria.size()) {
        while (global.filterPosition+1 < global.filterCriteria.size())
            global.filterCriteria.removeLast();
    }

    filterPosition++;
    FilterCriteria *newFilter = new FilterCriteria();
    global.filterCriteria.push_back(newFilter);
    global.filterPosition++;

    if (selectedItems.size() > 0) {
        newFilter->setTags(selectedItems);
    }
    newFilter->resetAttribute = true;
    newFilter->resetDeletedOnly = true;
    newFilter->resetSavedSearch = true;
    newFilter->resetTags = true;

    emit updateSelectionRequested();

    QLOG_TRACE() << "Leaving NTagView::buildSelection()";
}


void NTagView::updateSelection() {
    blockSignals(true);

    FilterCriteria *criteria = global.filterCriteria[global.filterPosition];
    if (global.filterPosition != filterPosition) {
        QList<QTreeWidgetItem*> selectedItems = this->selectedItems();
        for (int i=0; i<selectedItems.size() && criteria->resetTags; i++) {
            selectedItems[i]->setSelected(false);
        }


        for (int i=0; i<criteria->getTags().size() && criteria->isTagsSet(); i++) {
            criteria->getTags()[i]->setSelected(true);
        }
    }
    root->setSelected(false);
    filterPosition = global.filterPosition;

    blockSignals(false);
}


void NTagView::addNewTag(int lid) {
    TagTable tagTable;
    Tag newTag;
    tagTable.get(newTag, lid);
    if (newTag.__isset.guid) {
        tagUpdated(lid, QString::fromStdString(newTag.name));
    }
}


void NTagView::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasFormat("application/x-nixnote-note")) {
        if (event->answerRect().intersects(childrenRect()))
            event->acceptProposedAction();
        return;
    }
}



void NTagView::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat("application/x-nixnote-note")) {
        event->accept();
        return;
    }
    if (event->source() == this) {
        if (global.tagBehavior() == "HideInactiveCount") {
            event->ignore();
            return;
        }
        QMimeData mdata(currentItem()->data(NAME_POSITION, Qt::UserRole).toInt());
        mdata.setText("application/x-nixnote-tag");
        //event->mimeData()->set(mdata);
        event->accept();
        return;
    }
    event->ignore();
}
