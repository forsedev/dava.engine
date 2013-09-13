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



#include "DAVAEngine.h"

#include "mainwindow.h"
#include "QtUtils.h"
#include "Project/ProjectManager.h"
#include "DockConsole/Console.h"
#include "Scene/SceneHelper.h"
#include "SpritesPacker/SpritePackerHelper.h"

#include "TextureBrowser/TextureBrowser.h"
#include "MaterialBrowser/MaterialBrowser.h"

#include "Classes/SceneEditor/EditorSettings.h"
#include "Classes/SceneEditor/EditorConfig.h"

#include "../CubemapEditor/CubemapUtils.h"
#include "../CubemapEditor/CubemapTextureBrowser.h"
#include "../Tools/AddSkyboxDialog/AddSkyboxDialog.h"

#include "Tools/BaseAddEntityDialog/BaseAddEntityDialog.h"

#include "SpeedTreeImporter.h"

#include "Tools/SelectPathWidget/SelectEntityPathWidget.h"

#include "../Tools/AddSwitchEntityDialog/AddSwitchEntityDialog.h"
#include "../Tools/AddLandscapeEntityDialog/AddLandscapeEntityDialog.h"

#include "../../Commands2/AddEntityCommand.h"
#include "StringConstants.h"
#include "SceneEditor/HintManager.h"
#include "../Tools/SettingsDialog/SettingsDialogQt.h"
#include "../Settings/SettingsManager.h"

#include "Classes/Qt/Scene/SceneEditor2.h"
#include "Classes/CommandLine/CommandLineManager.h"

#include "Render/Highlevel/ShadowVolumeRenderPass.h"
#include "../../Commands2/GroupEntitiesForMultiselectCommand.h"
#include "../../Commands2/LandscapeEditorDrawSystemActions.h"

#include "Classes/CommandLine/SceneSaver/SceneSaver.h"
#include "Classes/Qt/Main/Request.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QColorDialog>

QtMainWindow::QtMainWindow(bool enableGlobalTimeout, QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, waitDialog(NULL)
	, materialEditor(NULL)
	, addSwitchEntityDialog(NULL)
	, globalInvalidateTimeoutEnabled(false)
{
	Console::Instance();
	new ProjectManager();
	new SettingsManager();
	ui->setupUi(this);

	qApp->installEventFilter(this);
	EditorConfig::Instance()->ParseConfig(EditorSettings::Instance()->GetProjectPath() + "EditorConfig.yaml");

	SetupMainMenu();
	SetupToolBars();
	SetupDocks();
	SetupActions();

	// create tool windows
	new TextureBrowser(this);
	waitDialog = new QtWaitDialog(this);

	posSaver.Attach(this);
	posSaver.LoadState(this);

	QObject::connect(ProjectManager::Instance(), SIGNAL(ProjectOpened(const QString &)), this, SLOT(ProjectOpened(const QString &)));
	QObject::connect(ProjectManager::Instance(), SIGNAL(ProjectClosed()), this, SLOT(ProjectClosed()));

	QObject::connect(SceneSignals::Instance(), SIGNAL(CommandExecuted(SceneEditor2 *, const Command2*, bool)), this, SLOT(SceneCommandExecuted(SceneEditor2 *, const Command2*, bool)));
	QObject::connect(SceneSignals::Instance(), SIGNAL(Activated(SceneEditor2 *)), this, SLOT(SceneActivated(SceneEditor2 *)));
	QObject::connect(SceneSignals::Instance(), SIGNAL(Deactivated(SceneEditor2 *)), this, SLOT(SceneDeactivated(SceneEditor2 *)));
	QObject::connect(SceneSignals::Instance(), SIGNAL(Selected(SceneEditor2 *, DAVA::Entity *)), this, SLOT(EntitySelected(SceneEditor2 *, DAVA::Entity *)));
    QObject::connect(SceneSignals::Instance(), SIGNAL(Deselected(SceneEditor2 *, DAVA::Entity *)), this, SLOT(EntityDeselected(SceneEditor2 *, DAVA::Entity *)));
	QObject::connect(SceneSignals::Instance(), SIGNAL(NotPassableTerrainToggled(SceneEditor2*)), this, SLOT(NotPassableToggled(SceneEditor2*)));

	QObject::connect(SceneSignals::Instance(), SIGNAL(EditorLightEnabled(bool)), this, SLOT(EditorLightEnabled(bool)));

    QObject::connect(ui->sceneTabWidget, SIGNAL(CloseTabRequest(int , Request *)), this, SLOT(OnCloseTabRequest(int, Request *)));

    
	LoadGPUFormat();

    EnableGlobalTimeout(enableGlobalTimeout);
	HideLandscapeEditorDocks();

	EnableProjectActions(false);
	EnableSceneActions(false);
}

QtMainWindow::~QtMainWindow()
{
	SafeRelease(materialEditor);
    
    if(HintManager::Instance())
        HintManager::Instance()->Release();
	
    TextureBrowser::Instance()->Release();

	posSaver.SaveState(this);

	delete ui;
	ui = NULL;

	ProjectManager::Instance()->Release();
	SettingsManager::Instance()->Release();
}

Ui::MainWindow* QtMainWindow::GetUI()
{
	return ui;
}

SceneTabWidget* QtMainWindow::GetSceneWidget()
{
	return ui->sceneTabWidget;
}

SceneEditor2* QtMainWindow::GetCurrentScene()
{
	return ui->sceneTabWidget->GetCurrentScene();
}

bool QtMainWindow::SaveScene( SceneEditor2 *scene )
{
	bool sceneWasSaved = false;

	DAVA::FilePath scenePath = scene->GetScenePath();
	if(!scene->IsLoaded() || scenePath.IsEmpty())
	{
		sceneWasSaved = SaveSceneAs(scene);
	} 
	else
	{
		if(scene->IsChanged())
		{
			if(!scene->Save(scenePath))
			{
				QMessageBox::warning(this, "Save error", "An error occurred while saving the scene. See log for more info.", QMessageBox::Ok);
			}
		}
	}

	return sceneWasSaved;
}


bool QtMainWindow::SaveSceneAs(SceneEditor2 *scene)
{
	bool ret = false;

	if(NULL != scene)
	{
		DAVA::FilePath saveAsPath = DAVA::FilePath(ProjectManager::Instance()->CurProjectDataSourcePath().toStdString()) + scene->GetScenePath().GetFilename();

		QString selectedPath = QFileDialog::getSaveFileName(this, "Save scene as", saveAsPath.GetAbsolutePathname().c_str(), "DAVA Scene V2 (*.sc2)");
		if(!selectedPath.isEmpty())
		{
			DAVA::FilePath scenePath = DAVA::FilePath(selectedPath.toStdString());
			if(!scenePath.IsEmpty())
			{
				scene->SetScenePath(scenePath);
				ret = scene->Save(scenePath);

				if(!ret)
				{
					QMessageBox::warning(this, "Save error", "An error occurred while saving the scene. Please, see logs for more info.", QMessageBox::Ok);
				}
				else
				{
					AddRecent(scenePath.GetAbsolutePathname().c_str());
				}
			}
		}
	}

	return ret;
}

DAVA::eGPUFamily QtMainWindow::GetGPUFormat()
{
	return EditorSettings::Instance()->GetTextureViewGPU();
}

void QtMainWindow::SetGPUFormat(DAVA::eGPUFamily gpu)
{
	EditorSettings::Instance()->SetTextureViewGPU(gpu);
	DAVA::Texture::SetDefaultGPU(gpu);

	DAVA::Map<DAVA::String, DAVA::Texture *> allScenesTextures;
	for(int tab = 0; tab < GetSceneWidget()->GetTabCount(); ++tab)
	{
		SceneEditor2 *scene = GetSceneWidget()->GetTabScene(tab);
		SceneHelper::EnumerateTextures(scene, allScenesTextures);
	}

	if(allScenesTextures.size() > 0)
	{
		int progress = 0;
		WaitStart("Reloading textures...", "", 0, allScenesTextures.size());

		DAVA::Map<DAVA::String, DAVA::Texture *>::const_iterator it = allScenesTextures.begin();
		DAVA::Map<DAVA::String, DAVA::Texture *>::const_iterator end = allScenesTextures.end();

		for(; it != end; ++it)
		{
			it->second->ReloadAs(gpu);

			WaitSetMessage(it->first.c_str());
			WaitSetValue(progress++);
		}

		WaitStop();
	}

	LoadGPUFormat();
}

void QtMainWindow::WaitStart(const QString &title, const QString &message, int min /* = 0 */, int max /* = 100 */)
{
	waitDialog->SetRange(min, max);
	waitDialog->Show(title, message, false, false);
}

void QtMainWindow::WaitSetMessage(const QString &messsage)
{
	waitDialog->SetMessage(messsage);
}

void QtMainWindow::WaitSetValue(int value)
{
	waitDialog->SetValue(value);
}

void QtMainWindow::WaitStop()
{
	waitDialog->Reset();
}

bool QtMainWindow::eventFilter(QObject *obj, QEvent *event)
{
	QEvent::Type eventType = event->type();

	if(qApp == obj && ProjectManager::Instance()->IsOpened())
	{
		if(QEvent::ApplicationActivate == eventType)
		{
			if(QtLayer::Instance())
			{
				QtLayer::Instance()->OnResume();
				Core::Instance()->GetApplicationCore()->OnResume();
			}
		}
		else if(QEvent::ApplicationDeactivate == eventType)
		{
			if(QtLayer::Instance())
			{
				QtLayer::Instance()->OnSuspend();
				Core::Instance()->GetApplicationCore()->OnSuspend();
			}
		}
	}

	if(obj == this && QEvent::WindowUnblocked == eventType)
	{
		if(isActiveWindow())
		{
			ui->sceneTabWidget->setFocus(Qt::ActiveWindowFocusReason);
		}
	}

	return QMainWindow::eventFilter(obj, event);
}

void QtMainWindow::SetupTitle()
{
	DAVA::KeyedArchive *options = DAVA::Core::Instance()->GetOptions();
	QString title = options->GetString("title").c_str();

	if(ProjectManager::Instance()->IsOpened())
	{
		title += " | Project - ";
		title += ProjectManager::Instance()->CurProjectPath();
	}

	this->setWindowTitle(title);
}

void QtMainWindow::SetupMainMenu()
{
	QAction *actionProperties = ui->dockProperties->toggleViewAction();
	QAction *actionLibrary = ui->dockLibrary->toggleViewAction();
	QAction *actionHangingObjects = ui->dockHangingObjects->toggleViewAction();
	QAction *actionParticleEditor = ui->dockParticleEditor->toggleViewAction();
	QAction *actionParticleEditorTimeLine = ui->dockParticleEditorTimeLine->toggleViewAction();
	QAction *actionSceneInfo = ui->dockSceneInfo->toggleViewAction();
	QAction *actionSceneTree = ui->dockSceneTree->toggleViewAction();
	QAction *actionConsole = ui->dockConsole->toggleViewAction();

	ui->menuView->addAction(actionSceneInfo);
	ui->menuView->addAction(actionLibrary);
	ui->menuView->addAction(actionProperties);
	ui->menuView->addAction(actionParticleEditor);
	ui->menuView->addAction(actionParticleEditorTimeLine);
	ui->menuView->addAction(actionHangingObjects);
	ui->menuView->addAction(actionSceneTree);
	ui->menuView->addAction(actionConsole);
	ui->menuView->addAction(ui->dockLODEditor->toggleViewAction());

	InitRecent();
}

void QtMainWindow::SetupToolBars()
{
	QAction *actionMainToolBar = ui->mainToolBar->toggleViewAction();
	QAction *actionModifToolBar = ui->modificationToolBar->toggleViewAction();
	QAction *actionViewModeToolBar = ui->viewModeToolBar->toggleViewAction();
	QAction *actionLandscapeToolbar = ui->landscapeToolBar->toggleViewAction();

	ui->menuToolbars->addAction(actionMainToolBar);
	ui->menuToolbars->addAction(actionModifToolBar);
	ui->menuToolbars->addAction(actionViewModeToolBar);
	ui->menuToolbars->addAction(actionLandscapeToolbar);

	modificationWidget = new ModificationWidget(NULL);
	ui->modificationToolBar->insertWidget(ui->actionModifyReset, modificationWidget);

	// adding reload sprites action
	//QToolButton *reloadSpritesBtn = new QToolButton();
	//reloadSpritesBtn->setDefaultAction(ui->actionReloadSprites);
	//reloadSpritesBtn->setMaximumWidth(100);
	//reloadSpritesBtn->setMinimumWidth(100);
	//ui->sceneToolBar->addSeparator();
	//ui->sceneToolBar->addWidget(reloadSpritesBtn);
	//reloadSpritesBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	// adding reload textures actions
	QToolButton *reloadTexturesBtn = new QToolButton();
	reloadTexturesBtn->setMenu(ui->menuTexturesForGPU);
	reloadTexturesBtn->setPopupMode(QToolButton::MenuButtonPopup);
	reloadTexturesBtn->setDefaultAction(ui->actionReloadTextures);
	reloadTexturesBtn->setMaximumWidth(100);
	reloadTexturesBtn->setMinimumWidth(100);
	ui->mainToolBar->addSeparator();
	ui->mainToolBar->addWidget(reloadTexturesBtn);
	reloadTexturesBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	reloadTexturesBtn->setAutoRaise(false);
}

void QtMainWindow::SetupDocks()
{
	QObject::connect(ui->sceneTreeFilterClear, SIGNAL(pressed()), ui->sceneTreeFilterEdit, SLOT(clear()));
	QObject::connect(ui->sceneTreeFilterEdit, SIGNAL(textChanged(const QString &)), ui->sceneTree, SLOT(SetFilter(const QString &)));
}

void QtMainWindow::SetupActions()
{
	// scene file actions
	QObject::connect(ui->actionOpenProject, SIGNAL(triggered()), this, SLOT(OnProjectOpen()));
	QObject::connect(ui->actionOpenScene, SIGNAL(triggered()), this, SLOT(OnSceneOpen()));
	QObject::connect(ui->actionNewScene, SIGNAL(triggered()), this, SLOT(OnSceneNew()));
	QObject::connect(ui->actionSaveScene, SIGNAL(triggered()), this, SLOT(OnSceneSave()));
	QObject::connect(ui->actionSaveSceneAs, SIGNAL(triggered()), this, SLOT(OnSceneSaveAs()));
	QObject::connect(ui->actionSaveToFolder, SIGNAL(triggered()), this, SLOT(OnSceneSaveToFolder()));

    QObject::connect(ui->menuFile, SIGNAL(triggered(QAction *)), this, SLOT(OnRecentTriggered(QAction *)));

	// export
	QObject::connect(ui->menuExport, SIGNAL(triggered(QAction *)), this, SLOT(ExportMenuTriggered(QAction *)));
	
    // import
    QObject::connect(ui->actionImportSpeedTreeXML, SIGNAL(triggered()), this, SLOT(OnImportSpeedTreeXML()));

	// reload
	ui->actionReloadPoverVRIOS->setData(GPU_POWERVR_IOS);
	ui->actionReloadPoverVRAndroid->setData(GPU_POWERVR_ANDROID);
	ui->actionReloadTegra->setData(GPU_TEGRA);
	ui->actionReloadMali->setData(GPU_MALI);
	ui->actionReloadAdreno->setData(GPU_ADRENO);
	ui->actionReloadPNG->setData(GPU_UNKNOWN);
	QObject::connect(ui->menuTexturesForGPU, SIGNAL(triggered(QAction *)), this, SLOT(OnReloadTexturesTriggered(QAction *)));
	QObject::connect(ui->actionReloadTextures, SIGNAL(triggered()), this, SLOT(OnReloadTextures()));
    QObject::connect(ui->actionReloadSprites, SIGNAL(triggered()), this, SLOT(OnReloadSprites()));

	
	// scene undo/redo
	QObject::connect(ui->actionUndo, SIGNAL(triggered()), this, SLOT(OnUndo()));
	QObject::connect(ui->actionRedo, SIGNAL(triggered()), this, SLOT(OnRedo()));

	// scene modifications
	QObject::connect(ui->actionModifySelect, SIGNAL(triggered()), this, SLOT(OnSelectMode()));
	QObject::connect(ui->actionModifyMove, SIGNAL(triggered()), this, SLOT(OnMoveMode()));
	QObject::connect(ui->actionModifyRotate, SIGNAL(triggered()), this, SLOT(OnRotateMode()));
	QObject::connect(ui->actionModifyScale, SIGNAL(triggered()), this, SLOT(OnScaleMode()));
	QObject::connect(ui->actionPivotCenter, SIGNAL(triggered()), this, SLOT(OnPivotCenterMode()));
	QObject::connect(ui->actionPivotCommon, SIGNAL(triggered()), this, SLOT(OnPivotCommonMode()));
	QObject::connect(ui->actionManualModifMode, SIGNAL(triggered()), this, SLOT(OnManualModifMode()));
	QObject::connect(ui->actionModifyPlaceOnLandscape, SIGNAL(triggered()), this, SLOT(OnPlaceOnLandscape()));
	QObject::connect(ui->actionModifySnapToLandscape, SIGNAL(triggered()), this, SLOT(OnSnapToLandscape()));

	// tools
	QObject::connect(ui->actionMaterialEditor, SIGNAL(triggered()), this, SLOT(OnMaterialEditor()));
	QObject::connect(ui->actionTextureConverter, SIGNAL(triggered()), this, SLOT(OnTextureBrowser()));
	QObject::connect(ui->actionEnableCameraLight, SIGNAL(triggered()), this, SLOT(OnSceneLightMode()));
	QObject::connect(ui->actionCubemapEditor, SIGNAL(triggered()), this, SLOT(OnCubemapEditor()));
	QObject::connect(ui->actionShowNotPassableLandscape, SIGNAL(triggered()), this, SLOT(OnNotPassableTerrain()));

	QObject::connect(ui->actionSkyboxEditor, SIGNAL(triggered()), this, SLOT(OnSetSkyboxNode()));

	QObject::connect(ui->actionLandscape, SIGNAL(triggered()), this, SLOT(OnLandscapeDialog()));
	QObject::connect(ui->actionLight, SIGNAL(triggered()), this, SLOT(OnLightDialog()));
	QObject::connect(ui->actionCamera, SIGNAL(triggered()), this, SLOT(OnCameraDialog()));
	QObject::connect(ui->actionImposter, SIGNAL(triggered()), this, SLOT(OnImposterDialog()));
	QObject::connect(ui->actionUserNode, SIGNAL(triggered()), this, SLOT(OnUserNodeDialog()));
	QObject::connect(ui->actionSwitchNode, SIGNAL(triggered()), this, SLOT(OnSwitchEntityDialog()));
	QObject::connect(ui->actionParticleEffectNode, SIGNAL(triggered()), this, SLOT(OnParticleEffectDialog()));
	QObject::connect(ui->actionUniteEntitiesWithLODs, SIGNAL(triggered()), this, SLOT(OnUniteEntitiesWithLODs()));
	QObject::connect(ui->menuCreateNode, SIGNAL(aboutToShow()), this, SLOT(OnAddEntityMenuAboutToShow()));
			
	QObject::connect(ui->actionShowSettings, SIGNAL(triggered()), this, SLOT(OnShowSettings()));
	
	QObject::connect(ui->actionSetShadowColor, SIGNAL(triggered()), this, SLOT(OnSetShadowColor()));
	QObject::connect(ui->actionDynamicBlendModeAlpha, SIGNAL(triggered()), this, SLOT(OnShadowBlendModeAlpha()));
	QObject::connect(ui->actionDynamicBlendModeMultiply, SIGNAL(triggered()), this, SLOT(OnShadowBlendModeMultiply()));

    QObject::connect(ui->actionSaveHeightmapToPNG, SIGNAL(triggered()), this, SLOT(OnSaveHeightmapToPNG()));
	QObject::connect(ui->actionSaveTiledTexture, SIGNAL(triggered()), this, SLOT(OnSaveTiledTexture()));
    
#if defined(__DAVAENGINE_MACOS__)
    ui->menuTools->removeAction(ui->actionBeast);
#elif defined(__DAVAENGINE_WIN32__)
	QObject::connect(ui->actionBeast, SIGNAL(triggered()), this, SLOT(OnBeast()));
#endif //OS
    

	//Help
    QObject::connect(ui->actionHelp, SIGNAL(triggered()), this, SLOT(OnOpenHelp()));
}

void QtMainWindow::InitRecent()
{
	for(int i = 0; i < EditorSettings::Instance()->GetLastOpenedCount(); ++i)
	{
		DAVA::String path = EditorSettings::Instance()->GetLastOpenedFile(i);
		QAction *action = ui->menuFile->addAction(path.c_str());

		action->setData(QString(path.c_str()));
		recentScenes.push_back(action);
	}
}

void QtMainWindow::AddRecent(const QString &path)
{
    while(recentScenes.size())
    {
        ui->menuFile->removeAction(recentScenes[0]);
        recentScenes.removeAt(0);
    }
    
    EditorSettings::Instance()->AddLastOpenedFile(DAVA::FilePath(path.toStdString()));

    InitRecent();
}

// ###################################################################################################
// Scene signals
// ###################################################################################################

void QtMainWindow::ProjectOpened(const QString &path)
{
	EnableProjectActions(true);
	SetupTitle();
}

void QtMainWindow::ProjectClosed()
{
	EnableProjectActions(false);
	SetupTitle();
}

void QtMainWindow::SceneActivated(SceneEditor2 *scene)
{
	EnableSceneActions(true);

	LoadUndoRedoState(scene);
	LoadModificationState(scene);
	LoadEditorLightState(scene);
	LoadNotPassableState(scene);
	LoadShadowBlendModeState(scene);

	// TODO: remove this code. it is for old material editor -->
    CreateMaterialEditorIfNeed();
    if(materialEditor)
    {
        DAVA::UIControl* parent = materialEditor->GetParent();
        if(NULL != parent && NULL != scene)
        {
            parent->RemoveControl(materialEditor);
            materialEditor->SetWorkingScene(scene, scene->selectionSystem->GetSelection()->GetEntity(0));
            parent->AddControl(materialEditor);
        }
    }
	// <---
    
    UpdateStatusBar();
}

void QtMainWindow::SceneDeactivated(SceneEditor2 *scene)
{
	// block some actions, when there is no scene
	EnableSceneActions(false);
}

void QtMainWindow::EnableProjectActions(bool enable)
{
	ui->actionNewScene->setEnabled(enable);
	ui->actionOpenScene->setEnabled(enable);
	ui->actionSaveScene->setEnabled(enable);
	ui->actionSaveToFolder->setEnabled(enable);
	ui->actionCubemapEditor->setEnabled(enable);
	ui->dockLibrary->setEnabled(enable);
}

void QtMainWindow::EnableSceneActions(bool enable)
{
	ui->actionUndo->setEnabled(enable);
	ui->actionRedo->setEnabled(enable);

	ui->dockLODEditor->setEnabled(enable);
	ui->dockProperties->setEnabled(enable);
	ui->dockSceneTree->setEnabled(enable);
	ui->dockSceneInfo->setEnabled(enable);

	ui->actionSaveScene->setEnabled(enable);
	ui->actionSaveSceneAs->setEnabled(enable);
	ui->actionSaveToFolder->setEnabled(enable);

	ui->actionModifySelect->setEnabled(enable);
	ui->actionModifyMove->setEnabled(enable);
	ui->actionModifyReset->setEnabled(enable);
	ui->actionModifyRotate->setEnabled(enable);
	ui->actionModifyScale->setEnabled(enable);
	ui->actionModifyPlaceOnLandscape->setEnabled(enable);
	ui->actionModifySnapToLandscape->setEnabled(enable);
	ui->actionConvertToShadow->setEnabled(enable);
	ui->actionPivotCenter->setEnabled(enable);
	ui->actionPivotCommon->setEnabled(enable);
	ui->actionManualModifMode->setEnabled(enable);

	modificationWidget->setEnabled(enable);

	ui->actionTextureConverter->setEnabled(enable);
	ui->actionMaterialEditor->setEnabled(enable);
	ui->actionSkyboxEditor->setEnabled(enable);
	ui->actionHeightMapEditor->setEnabled(enable);
	ui->actionTileMapEditor->setEnabled(enable);
	ui->actionShowNotPassableLandscape->setEnabled(enable);
	ui->actionRulerTool->setEnabled(enable);
	ui->actionVisibilityCheckTool->setEnabled(enable);
	ui->actionCustomColorsEditor->setEnabled(enable);

	ui->actionEnableCameraLight->setEnabled(enable);
	ui->actionReloadTextures->setEnabled(enable);
	ui->actionReloadSprites->setEnabled(enable);

	ui->menuExport->setEnabled(enable);
	ui->menuEdit->setEnabled(enable);
	ui->menuCreateNode->setEnabled(enable);
	ui->menuComponent->setEnabled(enable);
	ui->menuScene->setEnabled(enable);
}

void QtMainWindow::CreateMaterialEditorIfNeed()
{
	if(!materialEditor)
	{
		if(HintManager::Instance() == NULL)
		{
			new HintManager();//needed for hints in MaterialEditor
		}

		materialEditor = new MaterialEditor(DAVA::Rect(20, 20, 500, 600));
	}
}

void QtMainWindow::AddSwitchDialogFinished(int result)
{
	QObject::disconnect(addSwitchEntityDialog, SIGNAL(finished(int)), this, SLOT(AddSwitchDialogFinished(int)));

	SceneEditor2* scene = GetCurrentScene();

	Entity* switchEntity = addSwitchEntityDialog->GetEntity();

	if(result != QDialog::Accepted || NULL == scene)
	{
		addSwitchEntityDialog->CleanupPathWidgets();
		addSwitchEntityDialog->SetEntity(NULL);
		addSwitchEntityDialog = NULL;
		return;
	}

	Vector<Entity*> vector;
	addSwitchEntityDialog->GetPathEntities(vector, scene);	
	addSwitchEntityDialog->CleanupPathWidgets();
	
	Q_FOREACH(Entity* item, vector)
	{
		switchEntity->AddNode(item);
	}
	if(vector.size())
	{
		AddEntityCommand* command = new AddEntityCommand(switchEntity, scene);
		scene->Exec(command);
		SafeRelease(switchEntity);
		
		Entity* affectedEntity = command->GetEntity();
		scene->selectionSystem->SetSelection(affectedEntity);
		scene->ImmediateEvent(affectedEntity, Component::SWITCH_COMPONENT, EventSystem::SWITCH_CHANGED);
	}

	addSwitchEntityDialog->SetEntity(NULL);
	addSwitchEntityDialog = NULL;
}

void QtMainWindow::SceneCommandExecuted(SceneEditor2 *scene, const Command2* command, bool redo)
{
	if(scene == GetCurrentScene())
	{
		LoadUndoRedoState(scene);
	}
}

void QtMainWindow::UpdateRulerToolLength(SceneEditor2 *scene, double length, double previewLength)
{
	QString l = QString("Current length: %1").arg(length);
	QString pL = QString("Preview length: %1").arg(previewLength);

	QString msg;
	if (length >= 0.0)
	{
		msg = l;
	}
	if (previewLength >= 0.0)
	{
		msg += ";    " + pL;
	}

	ui->statusBar->showMessage(msg);
}

// ###################################################################################################
// Mainwindow Qt actions
// ###################################################################################################

void QtMainWindow::OnProjectOpen()
{
	QString newPath = ProjectManager::Instance()->ProjectOpenDialog();
	ProjectManager::Instance()->ProjectOpen(newPath);
}

void QtMainWindow::OnProjectClose()
{
	// TODO:
	// Close all scenes
	// ...
	// 

	ProjectManager::Instance()->ProjectClose();
}

void QtMainWindow::OnSceneNew()
{
	int index = ui->sceneTabWidget->OpenTab();
	ui->sceneTabWidget->SetCurrentTab(index);
}

void QtMainWindow::OnSceneOpen()
{
	QString path = QFileDialog::getOpenFileName(this, "Open scene file", ProjectManager::Instance()->CurProjectDataSourcePath(), "DAVA Scene V2 (*.sc2)");
	if (!path.isEmpty())
	{
		int index = ui->sceneTabWidget->OpenTab(DAVA::FilePath(path.toStdString()));
		ui->sceneTabWidget->SetCurrentTab(index);

		AddRecent(path);
	}
}

void QtMainWindow::OnSceneSave()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		SaveScene(scene);
	}
}

void QtMainWindow::OnSceneSaveAs()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		SaveSceneAs(scene);
	}
}

void QtMainWindow::OnSceneSaveToFolder()
{
	SceneEditor2* scene = GetCurrentScene();
	if(!scene) return;

	FilePath scenePathname = scene->GetScenePath();
	if(scenePathname.IsEmpty() && scenePathname.GetType() == FilePath::PATH_IN_MEMORY)
	{
		ShowErrorDialog("Can't save not saved scene.");
		return;
	}

	QString path = QFileDialog::getExistingDirectory(NULL, QString("Open Folder"), QString("/"));
	if(path.isEmpty())
		return;

	FilePath folder = PathnameToDAVAStyle(path);
	folder.MakeDirectoryPathname();

	SceneSaver sceneSaver;
	sceneSaver.SetInFolder(scene->GetScenePath().GetDirectory());
	sceneSaver.SetOutFolder(folder);

	Set<String> errorsLog;
	scene->PopEditorEntities();
	sceneSaver.SaveScene(scene, scene->GetScenePath(), errorsLog);
	scene->PushEditorEntities();

	ShowErrorDialog(errorsLog);
}

void QtMainWindow::OnCloseTabRequest(int tabIndex, Request *closeRequest)
{
    SceneEditor2 *scene = ui->sceneTabWidget->GetTabScene(tabIndex);
    if(!scene || !scene->IsChanged())
    {
        closeRequest->Accept();
        return;
    }
    
    int answer = QMessageBox::question(NULL, "Scene was changed", "Do you want to save changes, made to scene?",
                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);
    
    if(answer == QMessageBox::Cancel)
    {
        closeRequest->Cancel();
        return;
    }
	else if(answer == QMessageBox::No)
	{
		closeRequest->Accept();
		return;
	}

	bool sceneWasSaved = SaveScene(scene);
    if(sceneWasSaved)
    {
		closeRequest->Accept();
    }
	else
	{
		closeRequest->Cancel();
	}
}


void QtMainWindow::ExportMenuTriggered(QAction *exportAsAction)
{
	SceneEditor2* scene = GetCurrentScene();
	if (scene)
	{
		eGPUFamily gpuFamily = (eGPUFamily)exportAsAction->data().toInt();
		if (!scene->Export(gpuFamily))
		{
			QMessageBox::warning(this, "Export error", "An error occurred while exporting the scene. See log for more info.", QMessageBox::Ok);
		}
	}
}

void QtMainWindow::OnImportSpeedTreeXML()
{
    QString projectPath = ProjectManager::Instance()->CurProjectPath();
    QString path = QFileDialog::getOpenFileName(this, "Import SpeedTree", projectPath, "SpeedTree RAW File (*.xml)");
    if (!path.isEmpty())
    {
        DAVA::FilePath filePath = DAVA::SpeedTreeImporter::ImportSpeedTreeFromXML(path.toStdString(), ProjectManager::Instance()->CurProjectDataSourcePath().toStdString() + "Trees/");
        QMessageBox::information(this, "SpeedTree Import", QString(("SpeedTree model was imported to " + filePath.GetAbsolutePathname()).c_str()), QMessageBox::Ok);
    }
}

void QtMainWindow::OnRecentTriggered(QAction *recentAction)
{
	if(recentScenes.contains(recentAction))
	{
		QString path = recentAction->data().toString();

		int index = ui->sceneTabWidget->OpenTab(DAVA::FilePath(path.toStdString()));
		ui->sceneTabWidget->SetCurrentTab(index);

		if(-1 != index)
		{
			AddRecent(path);
		}
	}
}

void QtMainWindow::OnUndo()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->Undo();
	}
}

void QtMainWindow::OnRedo()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->Redo();
	}
}

void QtMainWindow::OnReloadTextures()
{
	SetGPUFormat(GetGPUFormat());
}

void QtMainWindow::OnReloadTexturesTriggered(QAction *reloadAction)
{
	DAVA::eGPUFamily gpu = (DAVA::eGPUFamily) reloadAction->data().toInt();
	if(gpu >= DAVA::GPU_UNKNOWN && gpu < DAVA::GPU_FAMILY_COUNT)
	{
		SetGPUFormat(gpu);
	}
}

void QtMainWindow::OnReloadSprites()
{
    SpritePackerHelper::Instance()->UpdateParticleSprites();
}

void QtMainWindow::OnSelectMode()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->modifSystem->SetModifMode(ST_MODIF_OFF);
		LoadModificationState(scene);
	}
}

void QtMainWindow::OnMoveMode()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->modifSystem->SetModifMode(ST_MODIF_MOVE);
		LoadModificationState(scene);
	}
}

void QtMainWindow::OnRotateMode()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->modifSystem->SetModifMode(ST_MODIF_ROTATE);
		LoadModificationState(scene);
	}
}

void QtMainWindow::OnScaleMode()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->modifSystem->SetModifMode(ST_MODIF_SCALE);
		LoadModificationState(scene);
	}
}

void QtMainWindow::OnPivotCenterMode()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->selectionSystem->SetPivotPoint(ST_PIVOT_ENTITY_CENTER);
		LoadModificationState(scene);
	}
}

void QtMainWindow::OnPivotCommonMode()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->selectionSystem->SetPivotPoint(ST_PIVOT_COMMON_CENTER);
		LoadModificationState(scene);
	}
}

void QtMainWindow::OnManualModifMode()
{
	if(ui->actionManualModifMode->isChecked())
	{
		modificationWidget->SetPivotMode(ModificationWidget::PivotRelative);
	}
	else
	{
		modificationWidget->SetPivotMode(ModificationWidget::PivotAbsolute);
	}
}

void QtMainWindow::OnPlaceOnLandscape()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->modifSystem->PlaceOnLandscape(scene->selectionSystem->GetSelection());
	}
}

void QtMainWindow::OnSnapToLandscape()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		scene->modifSystem->SetLandscapeSnap(ui->actionModifySnapToLandscape->isChecked());
		LoadModificationState(scene);
	}
}

void QtMainWindow::OnMaterialEditor()
{
    if(!materialEditor) return;
    
	if(NULL == materialEditor->GetParent())
	{
		SceneEditor2* sceneEditor = GetCurrentScene();
		if(NULL != sceneEditor)
		{
			materialEditor->SetWorkingScene(sceneEditor, sceneEditor->selectionSystem->GetSelection()->GetEntity(0));
			
			DAVA::UIScreen *curScreen = DAVA::UIScreenManager::Instance()->GetScreen();
			curScreen->AddControl(materialEditor);
		}
	}
	else
	{
		materialEditor->GetParent()->RemoveControl(materialEditor);
		materialEditor->SetWorkingScene(NULL, NULL);
	}
}

void QtMainWindow::OnTextureBrowser()
{
	SceneEditor2* sceneEditor = GetCurrentScene();
	DAVA::Entity *selectedEntity = NULL;

	if(NULL != sceneEditor)
	{
		selectedEntity = sceneEditor->selectionSystem->GetSelection()->GetEntity(0);
	}

	TextureBrowser::Instance()->sceneActivated(sceneEditor);
	TextureBrowser::Instance()->sceneNodeSelected(sceneEditor, selectedEntity); 
	TextureBrowser::Instance()->show();
}

void QtMainWindow::OnSceneLightMode()
{
	SceneEditor2* scene = GetCurrentScene();
	if(NULL != scene)
	{
		if(ui->actionEnableCameraLight->isChecked())
		{
			scene->editorLightSystem->SetCameraLightEnabled(true);
		}
		else
		{
			scene->editorLightSystem->SetCameraLightEnabled(false);
		}

		LoadEditorLightState(scene);
	}
}

void QtMainWindow::OnCubemapEditor()
{
	SceneEditor2* scene = GetCurrentScene();
	
	CubeMapTextureBrowser dlg(scene, dynamic_cast<QWidget*>(parent()));
	dlg.exec();
}

void QtMainWindow::OnNotPassableTerrain()
{
	SceneEditor2* scene = GetCurrentScene();
	if (!scene)
	{
		return;
	}

	if (scene->landscapeEditorDrawSystem->IsNotPassableTerrainEnabled())
	{
		scene->Exec(new ActionDisableNotPassable(scene));
	}
	else
	{
		scene->Exec(new ActionEnableNotPassable(scene));
	}
}

void QtMainWindow::OnSetSkyboxNode()
{
	SceneEditor2* scene = GetCurrentScene();
	if (!scene)
	{
		return;
	}
	
	AddSkyboxDialog dlg(this);
	dlg.Show(scene);
}

void QtMainWindow::OnSwitchEntityDialog()
{
	if(addSwitchEntityDialog!= NULL)//dialog is on screen, do nothing
	{
		return;
	}
	addSwitchEntityDialog = new AddSwitchEntityDialog( this);
	addSwitchEntityDialog->setAttribute( Qt::WA_DeleteOnClose, true );
	Entity* entityToAdd = new Entity();
	entityToAdd->SetName(ResourceEditor::SWITCH_NODE_NAME);
	entityToAdd->AddComponent(new SwitchComponent());
	KeyedArchive *customProperties = entityToAdd->GetCustomProperties();
	customProperties->SetBool(Entity::SCENE_NODE_IS_SOLID_PROPERTY_NAME, false);
	addSwitchEntityDialog->SetEntity(entityToAdd);
	
	SafeRelease(entityToAdd);
	
	QObject::connect(addSwitchEntityDialog, SIGNAL(finished(int)), this, SLOT(AddSwitchDialogFinished(int)));
	addSwitchEntityDialog->show();
}


void QtMainWindow::OnLandscapeDialog()
{
	SceneEditor2* sceneEditor = GetCurrentScene();
	if(!sceneEditor)
	{
		return;	
	}
	Entity *landscapeEntity = FindLandscapeEntity(sceneEditor);
	Entity* entityToProcess = NULL;
	if(!landscapeEntity)
	{
		entityToProcess = new Entity();
		entityToProcess->AddComponent(new RenderComponent(ScopedPtr<Landscape>(new Landscape())));
		entityToProcess->SetName(ResourceEditor::LANDSCAPE_NODE_NAME);
	}
	else
	{
		entityToProcess = landscapeEntity;
	}
	
	AddLandscapeEntityDialog * dlg = new AddLandscapeEntityDialog(entityToProcess, this);
	
	dlg->SetEntity(entityToProcess);
	dlg->exec();
	
	if(dlg->result() == QDialog::Accepted && !landscapeEntity)
	{
		AddEntityCommand* command = new AddEntityCommand(entityToProcess, sceneEditor);
		sceneEditor->Exec(command);
		sceneEditor->selectionSystem->SetSelection(command->GetEntity());
	}
	
	if(!landscapeEntity)
	{
		SafeRelease(entityToProcess);
	}
	
	delete dlg;
}

void QtMainWindow::OnLightDialog()
{
	Entity* sceneNode = new Entity();
	sceneNode->AddComponent(new LightComponent(ScopedPtr<Light>(new Light)));
	sceneNode->SetName(ResourceEditor::LIGHT_NODE_NAME);
	CreateAndDisplayAddEntityDialog(sceneNode);
}

void QtMainWindow::OnCameraDialog()
{
	Entity* sceneNode = new Entity();
	Camera * camera = new Camera();

	camera->SetUp(DAVA::Vector3(0.0f, 0.0f, 1.0f));
	camera->SetPosition(DAVA::Vector3(0.0f, 0.0f, 0.0f));
	camera->SetTarget(DAVA::Vector3(1.0f, 0.0f, 0.0f));
	camera->SetupPerspective(70.0f, 320.0f / 480.0f, 1.0f, 5000.0f);
	camera->SetAspect(1.0f);

	sceneNode->AddComponent(new CameraComponent(camera));
	sceneNode->SetName(ResourceEditor::CAMERA_NODE_NAME);
	CreateAndDisplayAddEntityDialog(sceneNode);
	SafeRelease(camera);
}

void QtMainWindow::OnImposterDialog()
{
	Entity* sceneNode = new ImposterNode();
	sceneNode->SetName(ResourceEditor::IMPOSTER_NODE_NAME);
	CreateAndDisplayAddEntityDialog(sceneNode);
}

void QtMainWindow::OnUserNodeDialog()
{
	Entity* sceneNode = new Entity();
	sceneNode->AddComponent(new UserComponent());
	sceneNode->SetName(ResourceEditor::USER_NODE_NAME);
	CreateAndDisplayAddEntityDialog(sceneNode);
}

void QtMainWindow::OnParticleEffectDialog()
{
	Entity* sceneNode = new Entity();
	sceneNode->AddComponent(new ParticleEffectComponent());
	sceneNode->SetName(ResourceEditor::PARTICLE_EFFECT_NODE_NAME);
	CreateAndDisplayAddEntityDialog(sceneNode);
}

void QtMainWindow::OnUniteEntitiesWithLODs()
{
	SceneEditor2* sceneEditor = GetCurrentScene();
	if(NULL == sceneEditor)
	{
		return;
	}
	const EntityGroup* selectedEntities = sceneEditor->selectionSystem->GetSelection();

	GroupEntitiesForMultiselectCommand* command = new GroupEntitiesForMultiselectCommand(selectedEntities);
	sceneEditor->Exec(command);
	sceneEditor->selectionSystem->SetSelection(command->GetEntity());
}


void QtMainWindow::OnAddEntityMenuAboutToShow()
{
	SceneEditor2* sceneEditor = GetCurrentScene();
	if(NULL == sceneEditor)
	{
		ui->actionUniteEntitiesWithLODs->setEnabled(false);
		return;
	}
	int32 selectedItemsNumber =	sceneEditor->selectionSystem->GetSelection()->Size();
	ui->actionUniteEntitiesWithLODs->setEnabled(selectedItemsNumber > 1);
}

void QtMainWindow::CreateAndDisplayAddEntityDialog(Entity* entity)
{
	SceneEditor2* sceneEditor = GetCurrentScene();
	BaseAddEntityDialog* dlg = new BaseAddEntityDialog(this);

	dlg->SetEntity(entity);
	dlg->exec();
	
	if(dlg->result() == QDialog::Accepted && sceneEditor)
	{
		AddEntityCommand* command = new AddEntityCommand(entity, sceneEditor);
		sceneEditor->Exec(command);
		sceneEditor->selectionSystem->SetSelection(command->GetEntity());
	}
	
	SafeRelease(entity);
	
	delete dlg;
}

void QtMainWindow::OnShowSettings()
{
	SettingsDialogQt t(this);
	t.exec();
}

void QtMainWindow::OnOpenHelp()
{
	FilePath docsPath = ResourceEditor::DOCUMENTATION_PATH + "index.html";
	QString docsFile = QString::fromStdString("file:///" + docsPath.GetAbsolutePathname());
	QDesktopServices::openUrl(QUrl(docsFile));
}

// ###################################################################################################
// Mainwindow load state functions
// ###################################################################################################

void QtMainWindow::LoadModificationState(SceneEditor2 *scene)
{
	if(NULL != scene)
	{
		ui->actionModifySelect->setChecked(false);
		ui->actionModifyMove->setChecked(false);
		ui->actionModifyRotate->setChecked(false);
		ui->actionModifyScale->setChecked(false);

		ST_ModifMode modifMode = scene->modifSystem->GetModifMode();
		modificationWidget->SetModifMode(modifMode);

		switch (modifMode)
		{
		case ST_MODIF_OFF:
			ui->actionModifySelect->setChecked(true);
			break;
		case ST_MODIF_MOVE:
			ui->actionModifyMove->setChecked(true);
			break;
		case ST_MODIF_ROTATE:
			ui->actionModifyRotate->setChecked(true);
			break;
		case ST_MODIF_SCALE:
			ui->actionModifyScale->setChecked(true);
			break;
		default:
			break;
		}


		// pivot point
		if(scene->selectionSystem->GetPivotPoint() == ST_PIVOT_ENTITY_CENTER)
		{
			ui->actionPivotCenter->setChecked(true);
			ui->actionPivotCommon->setChecked(false);
		}
		else
		{
			ui->actionPivotCenter->setChecked(false);
			ui->actionPivotCommon->setChecked(true);
		}

		// landscape snap
		ui->actionModifySnapToLandscape->setChecked(scene->modifSystem->GetLandscapeSnap());
	}
}

void QtMainWindow::LoadUndoRedoState(SceneEditor2 *scene)
{
	if(NULL != scene)
	{
		ui->actionUndo->setEnabled(scene->CanUndo());
		ui->actionRedo->setEnabled(scene->CanRedo());
	}
}

void QtMainWindow::LoadEditorLightState(SceneEditor2 *scene)
{
	if(NULL != scene)
	{
		ui->actionEnableCameraLight->setChecked(scene->editorLightSystem->GetCameraLightEnabled());
	}
}

void QtMainWindow::LoadNotPassableState(SceneEditor2* scene)
{
	if (!scene)
	{
		return;
	}

	ui->actionShowNotPassableLandscape->setChecked(scene->landscapeEditorDrawSystem->IsNotPassableTerrainEnabled());
}

void QtMainWindow::LoadShadowBlendModeState(SceneEditor2* scene)
{
	if(NULL != scene)
	{
		const ShadowVolumeRenderPass::eBlend blend = scene->GetShadowBlendMode();

		ui->actionDynamicBlendModeAlpha->setChecked(blend == ShadowVolumeRenderPass::MODE_BLEND_ALPHA);
		ui->actionDynamicBlendModeMultiply->setChecked(blend == ShadowVolumeRenderPass::MODE_BLEND_MULTIPLY);
	}
}


void QtMainWindow::LoadGPUFormat()
{
	int curGPU = GetGPUFormat();

	QList<QAction *> allActions = ui->menuTexturesForGPU->actions();
	for(int i = 0; i < allActions.size(); ++i)
	{
		QAction *actionN = allActions[i];

		if(!actionN->data().isNull() &&
			actionN->data().toInt() == curGPU)
		{
			actionN->setChecked(true);
			ui->actionReloadTextures->setText(actionN->text());
		}
		else
		{
			actionN->setChecked(false);
		}
	}
}

void QtMainWindow::OnSetShadowColor()
{
	SceneEditor2* scene = GetCurrentScene();
    if(!scene) return;
    
    QColor color = QColorDialog::getColor(ColorToQColor(scene->GetShadowColor()), 0, tr("Shadow Color"), QColorDialog::ShowAlphaChannel);
    scene->SetShadowColor(QColorToColor(color));
}

void QtMainWindow::OnShadowBlendModeAlpha()
{
	SceneEditor2* scene = GetCurrentScene();
    if(!scene) return;

	scene->SetShadowBlendMode(ShadowVolumeRenderPass::MODE_BLEND_ALPHA);
}

void QtMainWindow::OnShadowBlendModeMultiply()
{
	SceneEditor2* scene = GetCurrentScene();
    if(!scene) return;

	scene->SetShadowBlendMode(ShadowVolumeRenderPass::MODE_BLEND_MULTIPLY);
}

void QtMainWindow::OnSaveHeightmapToPNG()
{
	SceneEditor2* scene = GetCurrentScene();
    if(!scene) return;

    Landscape *landscape = FindLandscape(scene);
    if(!landscape) return;
    
    Heightmap * heightmap = landscape->GetHeightmap();
    FilePath heightmapPath = landscape->GetHeightmapPathname();
    heightmapPath.ReplaceExtension(".png");
    heightmap->SaveToImage(heightmapPath);
}

void QtMainWindow::OnSaveTiledTexture()
{
	SceneEditor2* scene = GetCurrentScene();
    if(!scene) return;

    Landscape *landscape = FindLandscape(scene);
    if(!landscape) return;
    
    FilePath texPathname = landscape->SaveFullTiledTexture();
    FilePath descriptorPathname = TextureDescriptor::GetDescriptorPathname(texPathname);
    
    TextureDescriptor *descriptor = TextureDescriptor::CreateFromFile(descriptorPathname);
    if(!descriptor)
    {
        descriptor = new TextureDescriptor();
        descriptor->pathname = descriptorPathname;
        descriptor->Save();
    }
    
    landscape->SetTexture(Landscape::TEXTURE_TILE_FULL, descriptor->pathname);
    
    SafeRelease(descriptor);
}

void QtMainWindow::OnGlobalInvalidateTimeout()
{
    UpdateStatusBar();
    
    emit GlobalInvalidateTimeout();
    if(globalInvalidateTimeoutEnabled)
    {
        StartGlobalInvalidateTimer();
    }
}

void QtMainWindow::UpdateStatusBar()
{
	SceneEditor2* scene = GetCurrentScene();
    if(!scene)
    {
        ui->statusBar->ResetDistanceToCamera();
        return;
    }

    const EntityGroup *selection = scene->selectionSystem->GetSelection();
    if(selection->Size())
    {
        float32 distanceToCamera = scene->selectionSystem->GetDistanceToCamera();
        ui->statusBar->SetDistanceToCamera(distanceToCamera);
    }
    else
    {
        ui->statusBar->ResetDistanceToCamera();
    }
}

void QtMainWindow::EntitySelected(SceneEditor2 *scene, DAVA::Entity *entity)
{
    UpdateStatusBar();
}
                     
void QtMainWindow::EntityDeselected(SceneEditor2 *scene, DAVA::Entity *entity)
{
    UpdateStatusBar();
}

void QtMainWindow::EnableGlobalTimeout(bool enable)
{
    if(globalInvalidateTimeoutEnabled != enable)
    {
        globalInvalidateTimeoutEnabled = enable;
        
        if(globalInvalidateTimeoutEnabled)
        {
            StartGlobalInvalidateTimer();
        }
    }
}

void QtMainWindow::StartGlobalInvalidateTimer()
{
    QTimer::singleShot(GLOBAL_INVALIDATE_TIMER_DELTA, this, SLOT(OnGlobalInvalidateTimeout()));
}

void QtMainWindow::HideLandscapeEditorDocks()
{
	ui->dockCustomColorsEditor->hide();
	ui->dockVisibilityToolEditor->hide();
	ui->dockHeightmapEditor->hide();
	ui->dockTilemaskEditor->hide();
	ui->dockRulerTool->hide();
}

void QtMainWindow::NotPassableToggled(SceneEditor2* scene)
{
	ui->actionShowNotPassableLandscape->setChecked(scene->landscapeEditorDrawSystem->IsNotPassableTerrainEnabled());
}

void QtMainWindow::EditorLightEnabled( bool enabled )
{
	ui->actionEnableCameraLight->setChecked(enabled);
}

void QtMainWindow::OnBeast()
{
#if defined (__DAVAENGINE_WIN32__)

	SceneEditor2* scene = GetCurrentScene();
	if(!scene) return;

	int32 ret = ShowQuestion("Beast", "This operation will take a lot of time. Do you agree to wait?", MB_FLAG_YES | MB_FLAG_NO, MB_FLAG_NO);		
	if(ret == MB_FLAG_NO) return;


#endif //#if defined (__DAVAENGINE_WIN32__)
}
 
