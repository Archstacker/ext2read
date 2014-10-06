/**
 * Ext2read
 * File: ext2read.cpp
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
/**
  * This file contains implementation of scanning and retrieving
  * partition information. For now we only support MBR style partitions.
  **/

#include <dirent.h>
#include <cstring>
#include <iostream>
#include <QTextCodec>
#include <QDir>

#include "ext2read.h"
#include "platform.h"
#include "partition.h"
#include "parttypes.h"
#include "lvm.h"



Ext2Read::Ext2Read()
{
    scan_system();
}

Ext2Read::~Ext2Read()
{
    clear_partitions();
}

void Ext2Read::scan_system()
{
    char pathname[20];
    int ret;

    ndisks = get_ndisks();
    LOG_INFO("No of disks %d\n", ndisks);

    for(int i = 0; i < ndisks; i++)
    {
        get_nthdevice(pathname, ndisks);
        LOG_INFO("Scanning %s\n", pathname);
        ret = scan_partitions(pathname, i);
        if(ret < 0)
        {
            LOG_INFO("Scanning of %s failed: ", pathname);
            continue;
        }
    }

    // Now Mount the LVM Partitions
    if(groups.empty())
        return;

    list<VolumeGroup *>::iterator i;
    VolumeGroup *grp;
    for(i = groups.begin(); i != groups.end(); i++)
    {
        grp = (*i);
        grp->logical_mount();
    }
}

void Ext2Read::clear_partitions()
{
    list<Ext2Partition *>::iterator i;
    for(i = nparts.begin(); i != nparts.end(); i++)
    {
        delete *i;
    }

    nparts.clear();
}

list<Ext2Partition *> Ext2Read::get_partitions()
{
    return nparts;
}


/* Reads The Extended Partitions */
int Ext2Read::scan_ebr(FileHandle handle, lloff_t base, int sectsize, int disk)
{
    unsigned char sector[SECTOR_SIZE];
    struct MBRpartition *part, *part1;
    Ext2Partition *partition;
    int logical = 4, ret;
    lloff_t  ebrBase, nextPart, ebr2=0;

    ebrBase = base;
    nextPart = base;
    while(1)
    {
        ret = read_disk(handle, sector, nextPart, 1, sectsize);
        if(ret < 0)
            return ret;

        if(ret < sectsize)
        {
            LOG("Error Reading the EBR \n");
            return -1;
        }
        part = pt_offset(sector, 0);
        LOG("index %d ID %X size %Ld \n", logical, part->sys_ind, get_nr_sects(part));

        /*if((part->sys_ind == 0x05) || (part->sys_ind == 0x0f))
        {
            // special case. ebr has extended partition with offset to another ebr.
            ebr2 += get_start_sect(part);
            nextPart = (ebr2 + ebrBase);
            continue;
        }*/

        if(part->sys_ind == EXT2)
        {
            partition = new Ext2Partition(get_nr_sects(part), get_start_sect(part)+ ebrBase + ebr2, sectsize, handle, NULL);
            if(partition->is_valid)
            {
                partition->set_linux_name("/dev/sd", disk, logical);
                nparts.push_back(partition);
            }
            else
            {
                delete partition;
            }
        }

        if(part->sys_ind == LINUX_LVM)
        {
            LOG("LVM Physical Volume found disk %d partition %d\n", disk, logical);
            LVM lvm(handle, get_start_sect(part)+ ebrBase + ebr2, this);
            lvm.scan_pv();
        }

        part1 = pt_offset(sector, 1);
        ebr2 = get_start_sect(part1);
        nextPart = (ebr2 + ebrBase);

        logical++;
        if(part1->sys_ind == 0)
            break;
    }
    return logical;
}


/* Scans The partitions */
int Ext2Read::scan_partitions(char *path, int diskno)
{
    unsigned char sector[SECTOR_SIZE];
    struct MBRpartition *part;
    Ext2Partition *partition;
    FileHandle handle;
    int sector_size;
    int ret, i;

    handle = open_disk(path, &sector_size);
    if(handle < 0)
        return -1;

    ret = read_disk(handle,sector, 0, 1, sector_size);
    if(ret < 0)
        return ret;

    if(ret < sector_size)
    {
        LOG("Error Reading the MBR on %s \n", path);
        return -1;
    }
   // part = pt_offset(sector, 0);
    if(!valid_part_table_flag(sector))
    {
        LOG("Partition Table Error on %s\n", path);
        LOG("Invalid End of sector marker");
        return -INVALID_TABLE;
    }

    /* First Scan primary Partitions */
    for(i = 0; i < 4; i++)
    {
        part = pt_offset(sector, i);
        if((part->sys_ind != 0x00) || (get_nr_sects(part) != 0x00))
        {
            LOG("index %d ID %X size %Ld \n", i, part->sys_ind, get_nr_sects(part));

            if(part->sys_ind == EXT2)
            {
                partition = new Ext2Partition(get_nr_sects(part), get_start_sect(part), sector_size, handle, NULL);
                if(partition->is_valid)
                {
                    partition->set_linux_name("/dev/sd", diskno, i);
                    nparts.push_back(partition);
                    LOG("Linux Partition found on disk %d partition %d\n", diskno, i);
                }
                else
                {
                    delete partition;
                }
            }

            if(part->sys_ind == LINUX_LVM)
            {
                LOG("LVM Physical Volume found disk %d partition %d\n", diskno, i);
                LVM lvm(handle, get_start_sect(part), this);
                lvm.scan_pv();
            }
            else if((part->sys_ind == 0x05) || (part->sys_ind == 0x0f))
            {
                scan_ebr(handle, get_start_sect(part), sector_size, diskno);
            }
        }
    }

    return 0;
}

int Ext2Read::add_loopback(const char *file)
{
    int ret, sector_size;
    Ext2Partition *partition;
    FileHandle handle;

    ndisks++;
   ret = scan_partitions((char *)file, ndisks);
   if(ret == -INVALID_TABLE)
   {
       handle = open_disk(file, &sector_size);
       partition = new Ext2Partition(0, 0, sector_size, handle, NULL);
       if(partition->is_valid)
       {
            partition->set_image_name(file);
            nparts.push_back(partition);
            LOG("Linux Partition found on disk %d partition %d\n", ndisks, 0);
            return 1;
        }
       else
       {
           delete partition;
       }
   }
   return 0;
}

void Ext2Read::show_partitions()
{
    Ext2Partition *temp;
    list<Ext2Partition *> parts;
    list<Ext2Partition *>::iterator i;
    void *ptr;

    parts = this->get_partitions();
    for(i = parts.begin(); i != parts.end(); i++)
    {
        temp = (*i);

        if(!temp->get_root())
        {
            continue;
        }
        std::cout<<temp->get_linux_name()<<endl;
    }
}

void Ext2Read::find_file(char *argv[])
{
    Ext2Partition *temp;
    list<Ext2Partition *> parts;
    list<Ext2Partition *>::iterator i;
    Ext2Partition *part;
    EXT2DIRENT *dir;
    Ext2File *file;
    char *dest_path;
    char *delim = "/\\";
    parts = this->get_partitions();
    dest_path = argv[2];
    for(i = parts.begin(); i != parts.end(); i++)
    {
        if((*i)->get_linux_name() == argv[1])
        {
            temp = *i;
            break;
        }
    }
    if(i == parts.end())
    {
        return;
    }

    blksize =temp->get_blocksize();
    buffer = new char [blksize];
    filetosave = NULL;
    cancelOperation = false;
    codec = QTextCodec::codecForName("utf-8");

    file = temp->get_root();
    dest_path = strtok(dest_path,delim);
    dir = temp->open_dir(file);
    if(dest_path)
    {
        do
        {
            while((file = temp->read_dir(dir)) != NULL)
            {
                if(file->file_name == dest_path)
                    break;
            }
            dir = temp->open_dir(file);
        }while(dest_path = strtok(NULL,delim));
    }
    else
    {
        for(int t=strlen(argv[1])-1;t;t--)
        {
            if(argv[1][t] == '/')
            {
                file->file_name=&argv[1][t+1];
                break;
            }
        }
    }

    QString filename(argv[3]);
    if(EXT2_S_ISDIR(file->inode.i_mode))
    {
        copy_folder(filename, file);
        return ;
    }
    else if(!EXT2_S_ISREG(file->inode.i_mode))
        return ;

    filename=filename+"\\"+file->file_name.c_str();
    copy_file(filename, file);
}

bool Ext2Read::copy_file(QString &destfile, Ext2File *srcfile)
{
    lloff_t blocks, blkindex;
    QString qsrc;
    QString nullstr = QString::fromAscii("");
    QByteArray ba;
    int extra;
    int ret;

    filetosave = new QFile(destfile);
    if (!filetosave->open(QIODevice::ReadWrite | QIODevice::Truncate))
    {
        LOG("Error creating file %s.\n", srcfile->file_name.c_str());
        return false;
    }
    //ba = destfile.toAscii();
    //const char *c_str2 = ba.data();

    //LOG("Copying file %s as %s\n", srcfile->file_name.c_str(), c_str2);
    qsrc = codec->toUnicode(srcfile->file_name.c_str());
    blocks = srcfile->file_size / blksize;
    for(blkindex = 0; blkindex < blocks; blkindex++)
    {
        if(cancelOperation)
        {
            return false;
        }
        ret = srcfile->partition->read_data_block(&srcfile->inode, blkindex, buffer);
        if(ret < 0)
        {
            filetosave->close();
            return false;
        }
        filetosave->write(buffer, blksize);
    }

    extra = srcfile->file_size % blksize;
    if(extra)
    {
        ret = srcfile->partition->read_data_block(&srcfile->inode, blkindex, buffer);
        if(ret < 0)
        {
            filetosave->close();
            return false;
        }
        filetosave->write(buffer, extra);
    }
    filetosave->close();
    return true;
}

bool Ext2Read::copy_folder(QString &path, Ext2File *parent)
{
    QDir dir(path);
    QString filetosave;
    QString rootname = path;
    EXT2DIRENT *dirent;
    Ext2Partition *part = parent->partition;
    Ext2File *child;
    QByteArray ba;
    bool ret;


    if(!EXT2_S_ISDIR(parent->inode.i_mode))
        return false;

    dir.mkdir(codec->toUnicode(parent->file_name.c_str()));
    /*ba = path.toAscii();
    const char *c_str2 = ba.data();
    LOG("Creating Folder %s as %s\n", parent->file_name.c_str(), c_str2);
*/
    dirent = part->open_dir(parent);
    while((child = part->read_dir(dirent)) != NULL)
    {
        filetosave = rootname;
        filetosave.append(QString::fromAscii("/"));
        filetosave.append(codec->toUnicode(parent->file_name.c_str()));
        if(EXT2_S_ISDIR(child->inode.i_mode))
        {

            ret = copy_folder(filetosave, child);
            if((ret == false) && (cancelOperation == true))
            {
                part->close_dir(dirent);
                return false;
            }
            continue;
        }
        else if(!EXT2_S_ISREG(child->inode.i_mode))
        {
            continue;
            //part->close_dir(dirent);
            //return false;
        }

        filetosave.append(QString::fromAscii("/"));
        filetosave.append(codec->toUnicode(child->file_name.c_str()));
        ret = copy_file(filetosave, child);
        if((ret == false) && (cancelOperation == true))
        {
            part->close_dir(dirent);
            return false;
        }
    }
    return true;
}
