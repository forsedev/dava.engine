/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/



#include "LibraryFileSystemModel.h"

#include "Main/mainwindow.h"
#include "Scene/SceneTabWidget.h"
#include "Scene/SceneEditor2.h"
#include "Commands2/DAEConvertAction.h"

#include <QFileSystemModel>
#include <QProcess>


LibraryFileSystemModel::LibraryFileSystemModel()
    : LibraryBaseModel("File System")
{
    treeModel = new QFileSystemModel();
    listModel = new QFileSystemModel();
    
    ((QFileSystemModel *)treeModel)->setFilter(QDir::Dirs|QDir::Drives|QDir::NoDotAndDotDot|QDir::AllDirs);
    
    QStringList nameFilters;
    nameFilters << QString("*.dae");
    nameFilters << QString("*.sc2");
    ((QFileSystemModel *)listModel)->setNameFilters(nameFilters);
    ((QFileSystemModel *)listModel)->setFilter(QDir::Files);
}

void LibraryFileSystemModel::TreeItemSelected(const QItemSelection & selection)
{
    const QModelIndex index = selection.indexes().first();

    QFileInfo fileInfo = ((QFileSystemModel *)treeModel)->fileInfo(index);
    if(fileInfo.isDir())
    {
        listRootPath = fileInfo.filePath();
    }
    else
    {
        listRootPath = treeRootPath;
    }

    ((QFileSystemModel *)listModel)->setRootPath(listRootPath);
    
    //qt magic to avoid showing of folders
    //-->
    ((QFileSystemModel *)listModel)->setFilter(QDir::Dirs);
    ((QFileSystemModel *)listModel)->setFilter(QDir::Files);
    //<--
}

void LibraryFileSystemModel::ListItemSelected(const QItemSelection & selection)
{
    const QModelIndex index = selection.indexes().first();
	if(index.isValid())
    {
        QFileInfo fileInfo = ((QFileSystemModel *)treeModel)->fileInfo(index);
		if(0 == fileInfo.suffix().compare("sc2", Qt::CaseInsensitive))
		{
			ShowPreview(fileInfo.filePath().toStdString());
            return;
		}
    }
    
    HidePreview();
}


void LibraryFileSystemModel::SetProjectPath(const QString & path)
{
    treeRootPath = path + "/DataSource/3d/";
    listRootPath = treeRootPath;

    QDir rootDir(treeRootPath);
    
    ((QFileSystemModel *)treeModel)->setRootPath(rootDir.canonicalPath());
    ((QFileSystemModel *)listModel)->setRootPath(rootDir.canonicalPath());
}

const QModelIndex LibraryFileSystemModel::GetTreeRootIndex() const
{
    return ((QFileSystemModel *)treeModel)->index(treeRootPath);
}

const QModelIndex LibraryFileSystemModel::GetListRootIndex() const
{
    return ((QFileSystemModel *)listModel)->index(listRootPath);
}

bool LibraryFileSystemModel::PrepareListContextMenu(QMenu &contextMenu, const QModelIndex &index) const
{
    HidePreview();
    
    QFileInfo fileInfo = ((QFileSystemModel *)listModel)->fileInfo(index);
    if(fileInfo.isFile())
    {
        QString fileExtension = fileInfo.suffix();

        QVariant fileInfoAsVariant = QVariant::fromValue<QFileInfo>(fileInfo);
        if(0 == fileExtension.compare("sc2", Qt::CaseInsensitive))
        {
            QAction * actionAdd = contextMenu.addAction("Add Model", this, SLOT(OnModelAdd()));
            QAction * actionEdit = contextMenu.addAction("Edit Model", this, SLOT(OnModelEdit()));
            contextMenu.addSeparator();
            QAction * actionRevealAt = contextMenu.addAction("Reveal at folder", this, SLOT(OnRevealAtFolder()));
            
            actionAdd->setData(fileInfoAsVariant);
            actionEdit->setData(fileInfoAsVariant);
            actionRevealAt->setData(fileInfoAsVariant);
            
            return true;
        }
        else if(0 == fileExtension.compare("dae", Qt::CaseInsensitive))
        {
            QAction * actionConvert = contextMenu.addAction("Convert", this, SLOT(OnDAEConvert()));
            QAction * actionConvertGeometry = contextMenu.addAction("Convert geometry", this, SLOT(OnDAEConvertGeometry()));
            contextMenu.addSeparator();
            QAction * actionRevealAt = contextMenu.addAction("Reveal at folder", this, SLOT(OnRevealAtFolder()));

            actionConvert->setData(fileInfoAsVariant);
            actionConvertGeometry->setData(fileInfoAsVariant);
            actionRevealAt->setData(fileInfoAsVariant);

            return true;
        }
    }

    return false;
}

void LibraryFileSystemModel::OnModelEdit()
{
    QVariant indexAsVariant = ((QAction *)sender())->data();
    const QFileInfo fileInfo = indexAsVariant.value<QFileInfo>();
 
    QtMainWindow::Instance()->OpenScene(fileInfo.absoluteFilePath());
}

void LibraryFileSystemModel::OnModelAdd()
{
    QVariant indexAsVariant = ((QAction *)sender())->data();
    const QFileInfo fileInfo = indexAsVariant.value<QFileInfo>();
    
    SceneEditor2 *scene = QtMainWindow::Instance()->GetCurrentScene();
    if(NULL != scene)
    {
        QtMainWindow::Instance()->WaitStart("Add object to scene", fileInfo.absoluteFilePath());

        scene->structureSystem->Add(fileInfo.absoluteFilePath().toStdString());
        
        QtMainWindow::Instance()->WaitStop();
    }
}

void LibraryFileSystemModel::OnDAEConvert()
{
    QVariant indexAsVariant = ((QAction *)sender())->data();
    const QFileInfo fileInfo = indexAsVariant.value<QFileInfo>();
    
    QtMainWindow::Instance()->WaitStart("DAE to SC2 Conversion", fileInfo.absoluteFilePath());
    
    Command2 *daeCmd = new DAEConvertAction(fileInfo.absoluteFilePath().toStdString());
    daeCmd->Redo();
    delete daeCmd;
    
    QtMainWindow::Instance()->WaitStop();
}

void LibraryFileSystemModel::OnDAEConvertGeometry()
{
    QVariant indexAsVariant = ((QAction *)sender())->data();
    const QFileInfo fileInfo = indexAsVariant.value<QFileInfo>();

    QtMainWindow::Instance()->WaitStart("DAE to SC2 Conversion", fileInfo.absoluteFilePath());
    
    Command2 *daeCmd = new DAEConvertWithSettingsAction(fileInfo.absoluteFilePath().toStdString());
    daeCmd->Redo();
    delete daeCmd;
    
    QtMainWindow::Instance()->WaitStop();
}

void LibraryFileSystemModel::OnRevealAtFolder()
{
    QVariant indexAsVariant = ((QAction *)sender())->data();
    const QFileInfo fileInfo = indexAsVariant.value<QFileInfo>();
    
#if defined (Q_WS_MAC)
    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "activate";
    args << "-e";
    args << "select POSIX file \""+fileInfo.absoluteFilePath()+"\"";
    args << "-e";
    args << "end tell";
    QProcess::startDetached("osascript", args);
#elif defined (Q_WS_WIN)
    QStringList args;
    args << "/select," << QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    QProcess::startDetached("explorer", args);
#endif//
}



void LibraryFileSystemModel::HidePreview() const
{
    SceneTabWidget *widget = QtMainWindow::Instance()->GetSceneWidget();
    widget->HideScenePreview();
}

void LibraryFileSystemModel::ShowPreview(const DAVA::FilePath & pathname) const
{
    SceneTabWidget *widget = QtMainWindow::Instance()->GetSceneWidget();
    widget->ShowScenePreview(pathname);
}



