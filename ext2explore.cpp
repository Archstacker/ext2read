/**
 * Ext2read
 * File: ext2explore.cpp
 **/
/**
 * Copyright (C) 2005, 2010 by Manish Regmi   (regmi dot manish at gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 **/

#include <QFileDialog>

#include "ext2explore.h"
#include "ui_ext2explore.h"

Ext2Explore::Ext2Explore(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Ext2Explore)
{
    filemodel = new QStandardItemModel(this);
    app = new Ext2Read();

    ui->setupUi(this);

    ui->tree->setModel(filemodel);
    ui->list->setModel(filemodel);
    root = filemodel->invisibleRootItem();
    init_root_fs();

    connect(ui->tree, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(on_action_item_dbclicked(QModelIndex)));
    connect(ui->list, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(on_action_item_dbclicked(QModelIndex)));

}

Ext2Explore::~Ext2Explore()
{
    delete ui;
    delete filemodel;
    delete app;
}

void Ext2Explore::init_root_fs()
{
    Ext2Partition *temp;
    list<Ext2Partition *> parts;
    list<Ext2Partition *>::iterator i;
    QStandardItem *item;

    parts = app->get_partitions();
    for(i = parts.begin(); i != parts.end(); i++)
    {
        temp = (*i);

        // check if itis already in the view
        if(temp->onview)
            continue;

        item = new QStandardItem(QIcon(":/resource/foldernew.png"),
                                 QString(temp->get_linux_name().c_str()));
        if(!temp->get_root())
        {
            LOG("Root folder for %s is invalid. \n", temp->get_linux_name().c_str());
            delete item;
            continue;
        }

        item->setData(QVariant(QMetaType::VoidStar, temp), Qt::UserRole);
        root->appendRow(item);

        temp->onview = true;
    }
}

void Ext2Explore::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void Ext2Explore::on_action_Exit_triggered()
{
    close();
}

void Ext2Explore::on_actionP_roperties_triggered()
{
    property.show();
}

void Ext2Explore::on_action_About_triggered()
{
    about.show();
}

void Ext2Explore::on_action_Rescan_System_triggered()
{
    delete app;

    app = new Ext2Read();
}

void Ext2Explore::on_actionOpen_Image_triggered()
{
    int result;
    QString filename;

    filename = QFileDialog::getOpenFileName(this,
         tr("Open Disk Image"), "", tr("All Disk Image Files (*)"));
    //LOG("Opening file %s as a disk image. \n", filename.toAscii());

    result = app->add_loopback(filename.toAscii());
    if(result <= 0)
    {
        LOG("No valid Ext2 Partitions found in the disk image.");
        return;
    }

    init_root_fs();
}

void Ext2Explore::on_action_item_dbclicked(const QModelIndex &index)
{
    QStandardItem *children;
    QStandardItem *parentItem;
    Ext2File *parentFile;
    Ext2File *files;
    QVariant fileData;
    EXT2DIRENT *dir;
    Ext2Partition *part;


    parentItem = filemodel->itemFromIndex(index);
    fileData = parentItem->data(Qt::UserRole);
    parentFile = static_cast<Ext2File *>(fileData.data());
    part = parentFile->partition;

    dir = part->open_dir(parentFile);

    while((files = part->read_dir(dir)) != NULL)
    {
        children = new QStandardItem(QIcon(":/resource/foldernew.png"),
                                 QString(part->get_linux_name().c_str()));

        children->setData(QVariant(QMetaType::VoidStar, files), Qt::UserRole);
        parentItem->appendRow(children);
    }
}