/**     @file mainwindow.cpp
 *
 *      EEEE2076 - Software Engineering & VR Project (Group Task)
 *
 *      Improvements over the individual-task version:
 *       - Cleaned-up Windows-style UI (menu bar / toolbar / status bar / docked
 *         control panel) with keyboard shortcuts.
 *       - Open-Directory action that loads every STL in a folder into the tree.
 *       - Add Child / Delete Item actions for the tree, with proper
 *         insertRows / removeRows notifications.
 *       - Live VR updates: colour, visibility and newly-added STLs propagate
 *         to the VR scene while it is running, via the VRRenderThread queue.
 *       - VR animation control (per-axis rotation rate sliders, spin toggle,
 *         reset view, environment toggle).
 *       - Verbose status-bar messages so the marker can see what just happened.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ModelPart.h"
#include "ModelPartList.h"
#include "optiondialog.h"
#include "lightdialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QStringList>

#include <functional>

#include <QVTKOpenGLNativeWidget.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCylinderSource.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLight.h>
#include <vtkLightCollection.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>


/* --------------------------------------------------------------------- */
/*  Construction / destruction                                           */
/* --------------------------------------------------------------------- */

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //VTK renderer setup
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    ui->widget->setRenderWindow(renderWindow);

    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(renderer);

    // Show a placeholder cylinder before anything is loaded
    {
        vtkNew<vtkCylinderSource> cylinder;
        cylinder->SetResolution(32);

        vtkNew<vtkPolyDataMapper> cylinderMapper;
        cylinderMapper->SetInputConnection(cylinder->GetOutputPort());

        vtkNew<vtkActor> cylinderActor;
        cylinderActor->SetMapper(cylinderMapper);
        cylinderActor->GetProperty()->SetColor(0.7, 0.55, 0.85);
        cylinderActor->RotateX(30.0);
        cylinderActor->RotateY(-45.0);

        renderer->AddActor(cylinderActor);
    }

    renderer->SetBackground(0.96, 0.96, 0.97);
    renderer->ResetCamera();
    renderer->GetActiveCamera()->Azimuth(30);
    renderer->GetActiveCamera()->Elevation(30);
    renderer->ResetCameraClippingRange();
    renderWindow->Render();

    // Tree view setup
    partList = new ModelPartList("PartsList", this);
    ui->treeView->setModel(partList);
    ui->treeView->setHeaderHidden(false);
    ui->treeView->viewport()->installEventFilter(this);


    ModelPart* rootItem = partList->getRootItem();
    for (int i = 0; i < 3; ++i) {
        QModelIndex rootIdx; /* invalid -> append to root */
        partList->appendChild(rootIdx,
                              { QString("Slot %1").arg(i + 1),
                                QString("true") });
    }
    ui->treeView->expandAll();

    ui->treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
    ui->treeView->addAction(ui->actionOpen_File);
    ui->treeView->addAction(ui->actionAdd_Child_Item);
    ui->treeView->addAction(ui->actionItem_Options);
    {
        QAction* sep = new QAction(this);
        sep->setSeparator(true);
        ui->treeView->addAction(sep);
    }
    ui->treeView->addAction(ui->actionDelete_Item);

    // Connections
    connect(this, &MainWindow::statusUpdateMessage,
            ui->statusbar, &QStatusBar::showMessage);
    connect(ui->treeView, &QTreeView::clicked,
            this, &MainWindow::handleTreeClicked);

    emit statusUpdateMessage(tr("Ready. Open a file (Ctrl+O) or a folder "
                                "(Ctrl+Shift+O) to begin."), 0);
}


MainWindow::~MainWindow()
{
    if (vrThread) {
        vrThread->issueCommand(VRRenderThread::END_RENDER, 0);
        vrThread->wait(2000);
        delete vrThread;
        vrThread = nullptr;
    }
    delete ui;
}


void MainWindow::closeEvent(QCloseEvent* event)
{
    if (vrThread) {
        vrThread->issueCommand(VRRenderThread::END_RENDER, 0);
        vrThread->wait(2000);
    }
    event->accept();
}


/* --------------------------------------------------------------------- */
/*  Helpers                                                              */
/* --------------------------------------------------------------------- */

ModelPart* MainWindow::selectedPart() const
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid()) return nullptr;
    return static_cast<ModelPart*>(idx.internalPointer());
}


bool MainWindow::isVRActive() const
{
    return vrThread && vrThread->isRunning();
}


void MainWindow::loadStlIntoItem(ModelPart* item, const QString& filePath)
{
    if (!item || filePath.isEmpty()) return;

    QFileInfo info(filePath);
    item->set(0, info.fileName());
    item->set(1, "true");
    item->setVisible(true);
    item->loadSTL(filePath);

    if (isVRActive()) {
        if (vtkActor* a = item->createVRActor()) {
            vrThread->addActorLive(a);
        }
    }
}


bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->treeView->viewport() &&
        event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::RightButton) {
            QModelIndex idx = ui->treeView->indexAt(me->pos());
            if (idx.isValid())
                ui->treeView->setCurrentIndex(idx);
        }
    }
    return QMainWindow::eventFilter(obj, event);
}


void MainWindow::handleTreeClicked()
{
    ModelPart* part = selectedPart();
    if (!part) return;

    emit statusUpdateMessage(tr("Selected: %1").arg(part->data(0).toString()),
                             0);
    refreshQuickPanelForSelection();
}


void MainWindow::refreshQuickPanelForSelection()
{
    ModelPart* p = selectedPart();
    if (!p) return;


    ui->checkVisible->blockSignals(true);
    ui->checkVisible->setChecked(p->visible());
    ui->checkVisible->blockSignals(false);
}


/* --------------------------------------------------------------------- */
/*  File menu                                                            */
/* --------------------------------------------------------------------- */

void MainWindow::on_actionOpen_File_triggered()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open STL file"),
        QString(),
        tr("STL Files (*.stl *.STL);;All Files (*.*)"));
    if (filePath.isEmpty()) return;

    ModelPart* item = selectedPart();
    if (!item) {
        emit statusUpdateMessage(
            tr("Cannot load STL - please select a tree item first."), 0);
        return;
    }

    loadStlIntoItem(item, filePath);

    // Refresh tree row
    QModelIndex idx = ui->treeView->currentIndex();
    QModelIndex left  = idx.sibling(idx.row(), 0);
    QModelIndex right = idx.sibling(idx.row(), 1);
    emit partList->dataChanged(left, right);

    ui->treeView->expandAll();
    updateRender();

    emit statusUpdateMessage(
        tr("Loaded STL into '%1': %2")
            .arg(item->data(0).toString(), QFileInfo(filePath).fileName()),
        0);
}


void MainWindow::on_actionOpen_Directory_triggered()
{
    QString dirPath = QFileDialog::getExistingDirectory(
        this,
        tr("Open folder of STL files"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath.isEmpty()) return;

    QDir dir(dirPath);
    QStringList filters; filters << "*.stl" << "*.STL";
    QFileInfoList stls = dir.entryInfoList(filters, QDir::Files,
                                           QDir::Name);
    if (stls.isEmpty()) {
        emit statusUpdateMessage(
            tr("No STL files found in: %1").arg(dirPath), 0);
        return;
    }

    ModelPart* parentItem = selectedPart();
    if (!parentItem) parentItem = partList->getRootItem();

    QModelIndex groupIdx = partList->appendChildToItem(
        parentItem,
        { QFileInfo(dirPath).fileName().isEmpty()
              ? QStringLiteral("Folder")
              : QFileInfo(dirPath).fileName(),
          QStringLiteral("true") });
    ModelPart* groupItem =
        static_cast<ModelPart*>(groupIdx.internalPointer());

    int loaded = 0;
    for (const QFileInfo& fi : stls) {
        QModelIndex newChildIdx = partList->appendChildToItem(
            groupItem,
            { fi.fileName(), QStringLiteral("true") });
        ModelPart* newChild =
            static_cast<ModelPart*>(newChildIdx.internalPointer());
        loadStlIntoItem(newChild, fi.absoluteFilePath());
        ++loaded;
    }

    ui->treeView->expandAll();
    updateRender();

    emit statusUpdateMessage(
        tr("Loaded %1 STL file(s) from %2").arg(loaded).arg(dirPath), 0);
}


void MainWindow::on_actionQuit_triggered()
{
    close();
}


/* --------------------------------------------------------------------- */
/*  Edit menu                                                            */
/* --------------------------------------------------------------------- */

void MainWindow::on_actionAdd_Child_Item_triggered()
{
    ModelPart* parentItem = selectedPart();
    if (!parentItem) parentItem = partList->getRootItem();

    QModelIndex newIdx = partList->appendChildToItem(
        parentItem,
        { QStringLiteral("New Item"), QStringLiteral("true") });

    ui->treeView->setCurrentIndex(newIdx);
    ui->treeView->expandAll();

    emit statusUpdateMessage(
        tr("Added new child item under '%1'").arg(parentItem->data(0).toString()),
        0);
}


void MainWindow::on_actionDelete_Item_triggered()
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid()) {
        emit statusUpdateMessage(
            tr("Cannot delete - please select an item first."), 0);
        return;
    }

    ModelPart* item = static_cast<ModelPart*>(idx.internalPointer());
    if (!item) return;

    const QString name = item->data(0).toString();


    if (isVRActive() && item->getVRActor()) {
        vrThread->requestVisibilityUpdate(item->getVRActor(), false);
    }

    if (!partList->removePart(idx)) {
        emit statusUpdateMessage(tr("Failed to delete item."), 0);
        return;
    }

    updateRender();
    emit statusUpdateMessage(tr("Deleted item: %1").arg(name), 0);
}


void MainWindow::on_actionItem_Options_triggered()
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid()) {
        emit statusUpdateMessage(
            tr("Open Properties: please select a tree item first."), 0);
        return;
    }

    ModelPart* item = static_cast<ModelPart*>(idx.internalPointer());
    if (!item) return;

    QString name = item->data(0).toString();
    int r = item->getColourR();
    int g = item->getColourG();
    int b = item->getColourB();
    bool vis = item->visible();

    OptionDialog dialog(this);
    dialog.setValues(name, r, g, b, vis);
    dialog.setFilters(item->getFilter1(), item->getFilter2());

    if (dialog.exec() != QDialog::Accepted) return;

    dialog.getValues(name, r, g, b, vis);

    item->set(0, name);
    item->setColour(static_cast<unsigned char>(r),
                    static_cast<unsigned char>(g),
                    static_cast<unsigned char>(b));
    item->setVisible(vis);
    item->setFilters(dialog.getFilter1(), dialog.getFilter2());
    item->set(1, vis ? "true" : "false");

    // Push the new colour / visibility / filter mapper into VR. */
    if (isVRActive() && item->getVRActor()) {
        vrThread->requestColourUpdate(item->getVRActor(),
                                      r / 255.0, g / 255.0, b / 255.0);
        vrThread->requestVisibilityUpdate(item->getVRActor(), vis);
    }

    QModelIndex left  = idx.sibling(idx.row(), 0);
    QModelIndex right = idx.sibling(idx.row(), 1);
    emit partList->dataChanged(left, right);

    updateRender();
    emit statusUpdateMessage(tr("Updated properties for '%1'").arg(name), 0);
}


/* --------------------------------------------------------------------- */
/*  View menu                                                            */
/* --------------------------------------------------------------------- */

void MainWindow::on_actionBackground_Colour_triggered()
{
    QColor c = QColorDialog::getColor(Qt::white, this,
                                      tr("Choose Background Colour"));
    if (!c.isValid()) return;

    renderer->SetBackground(c.redF(), c.greenF(), c.blueF());
    renderWindow->Render();
    emit statusUpdateMessage(
        tr("Background colour set to %1").arg(c.name()), 0);
}


void MainWindow::on_actionLight_Settings_triggered()
{
    LightDialog dialog(this);
    dialog.setValues(static_cast<int>(currentLightIntensity * 100),
                     static_cast<int>(currentLightR * 255),
                     static_cast<int>(currentLightG * 255),
                     static_cast<int>(currentLightB * 255));

    if (dialog.exec() != QDialog::Accepted) return;

    int intensity, r, g, b;
    dialog.getValues(intensity, r, g, b);
    currentLightIntensity = intensity / 100.0;
    currentLightR = r / 255.0;
    currentLightG = g / 255.0;
    currentLightB = b / 255.0;

    updateRender();
    emit statusUpdateMessage(tr("Scene lighting updated"), 0);
}


void MainWindow::on_actionDark_Mode_triggered(bool checked)
{
    ui->btnDarkMode->blockSignals(true);
    ui->btnDarkMode->setChecked(checked);
    ui->btnDarkMode->setText(checked ? "Light Mode" : "Dark Mode");
    ui->btnDarkMode->blockSignals(false);

    isDarkMode = checked;
    if (checked) {
        qApp->setStyleSheet(
            "QMainWindow { background-color: #2b2b2b; color: white; }"
            "QPushButton { background-color: #444; color: white; "
            "              border: 1px solid #555; padding: 4px; }"
            "QPushButton:hover { background-color: #555; }"
            "QGroupBox { color: white; border: 1px solid #555; "
            "            margin-top: 8px; padding-top: 8px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 8px; }"
            "QTreeView { background-color: #333; color: white; "
            "            alternate-background-color: #2e2e2e; }"
            "QHeaderView::section { background-color: #444; color: white; }"
            "QLabel { color: white; }"
            "QCheckBox { color: white; }"
            "QMenu, QMenuBar, QStatusBar { background-color: #2b2b2b; "
            "                              color: white; }"
            "QMenuBar::item:selected { background-color: #444; }"
            "QToolBar { background-color: #2b2b2b; border: 0; }");
        renderer->SetBackground(0.10, 0.10, 0.11);
    } else {
        qApp->setStyleSheet("");
        renderer->SetBackground(0.96, 0.96, 0.97);
    }
    renderWindow->Render();

    emit statusUpdateMessage(
        checked ? tr("Dark mode on") : tr("Dark mode off"), 0);
}


void MainWindow::on_actionReset_Camera_triggered()
{
    renderer->ResetCamera();
    renderer->GetActiveCamera()->Azimuth(30);
    renderer->GetActiveCamera()->Elevation(30);
    renderer->ResetCameraClippingRange();
    renderWindow->Render();
    emit statusUpdateMessage(tr("Camera reset"), 0);
}


/* --------------------------------------------------------------------- */
/*  VR menu                                                              */
/* --------------------------------------------------------------------- */

void MainWindow::on_actionStart_VR_triggered()
{
    if (isVRActive()) {
        emit statusUpdateMessage(tr("VR is already running."), 0);
        return;
    }

    if (vrThread) {
        vrThread->wait(1000);
        delete vrThread;
        vrThread = nullptr;
    }

    vrThread = new VRRenderThread(this);

    int actorCount = 0;
    std::function<void(ModelPart*)> addActors;
    addActors = [&](ModelPart* item) {
        if (!item) return;
        if (item->visible() && item->hasSTL()) {
            if (vtkActor* va = item->createVRActor()) {
                vrThread->addActorOffline(va);
                ++actorCount;
            }
        }
        for (int i = 0; i < item->childCount(); ++i)
            addActors(item->child(i));
    };
    addActors(partList->getRootItem());

    if (actorCount == 0) {
        emit statusUpdateMessage(
            tr("Starting VR with an empty scene - load some STLs to see "
               "your model."),
            0);
    } else {
        emit statusUpdateMessage(
            tr("Starting VR with %1 actor(s)...").arg(actorCount), 0);
    }

                                                 
    vrThread->issueCommand(VRRenderThread::ROTATE_X,
                           ui->sliderRotateX->value() / 10.0);
    vrThread->issueCommand(VRRenderThread::ROTATE_Y,
                           ui->sliderRotateY->value() / 10.0);
    vrThread->issueCommand(VRRenderThread::ROTATE_Z,
                           ui->sliderRotateZ->value() / 10.0);

    vrThread->start();
}


void MainWindow::on_actionStop_VR_triggered()
{
    if (!vrThread) {
        emit statusUpdateMessage(tr("VR is not currently running."), 0);
        return;
    }

    vrThread->issueCommand(VRRenderThread::END_RENDER, 0);
    vrThread->wait(2000);
    delete vrThread;
    vrThread = nullptr;

    emit statusUpdateMessage(tr("VR stopped"), 0);
}


void MainWindow::on_actionReset_VR_View_triggered()
{
    if (!isVRActive()) {
        emit statusUpdateMessage(tr("Reset VR View: VR is not running."), 0);
        return;
    }

    // Sliders to zero
    ui->sliderRotateX->setValue(0);
    ui->sliderRotateY->setValue(0);
    ui->sliderRotateZ->setValue(0);

    vrThread->issueCommand(VRRenderThread::RESET_VIEW, 0);
    emit statusUpdateMessage(tr("VR view reset"), 0);
}


void MainWindow::on_actionToggle_Spin_triggered()
{
    if (!isVRActive()) {
        emit statusUpdateMessage(tr("Toggle Spin: VR is not running."), 0);
        return;
    }
    vrThread->issueCommand(VRRenderThread::TOGGLE_SPIN, 0);
    emit statusUpdateMessage(tr("Toggled spin animation"), 0);
}


void MainWindow::on_actionToggle_Floor_triggered()
{
    if (!isVRActive()) {
        emit statusUpdateMessage(tr("Toggle Floor: VR is not running."), 0);
        return;
    }
    vrThread->issueCommand(VRRenderThread::TOGGLE_FLOOR, 0);
    emit statusUpdateMessage(tr("Toggled floor / scenery"), 0);
}


/* --------------------------------------------------------------------- */
/*  Help menu                                                            */
/* --------------------------------------------------------------------- */

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, tr("About FormulaVRviewer"),
        tr("<h3>FormulaVRviewer</h3>"
           "<p>EEEE2076 - Software Engineering &amp; VR (Group Task)</p>"
           "<p>Loads CAD models in STL format and displays them in both a "
           "desktop OpenGL window and a VR headset (via OpenVR/SteamVR).</p>"
           "<p><b>Tips:</b><br/>"
           "Ctrl+O - open a single STL into the selected slot<br/>"
           "Ctrl+Shift+O - load every STL in a folder<br/>"
           "Ctrl+E - edit the selected item's properties<br/>"
           "F5 / F6 - start / stop VR</p>"));
}


/* --------------------------------------------------------------------- */
/*  Side panel buttons - delegate to the matching actions                */
/* --------------------------------------------------------------------- */

void MainWindow::on_btnStartVR_clicked()        { on_actionStart_VR_triggered(); }
void MainWindow::on_btnStopVR_clicked()         { on_actionStop_VR_triggered();  }
void MainWindow::on_btnResetView_clicked()      { on_actionReset_VR_View_triggered(); }
void MainWindow::on_btnToggleSpin_clicked()     { on_actionToggle_Spin_triggered(); }
void MainWindow::on_btnToggleFloor_clicked()    { on_actionToggle_Floor_triggered(); }
void MainWindow::on_btnBackgroundColour_clicked(){ on_actionBackground_Colour_triggered(); }
void MainWindow::on_btnLightSettings_clicked()  { on_actionLight_Settings_triggered(); }

void MainWindow::on_btnDarkMode_toggled(bool checked)
{
    ui->actionDark_Mode->setChecked(checked);
    on_actionDark_Mode_triggered(checked);
}


void MainWindow::on_btnQuickColour_clicked()
{
    ModelPart* p = selectedPart();
    if (!p) {
        emit statusUpdateMessage(
            tr("Quick colour: select an item first."), 0);
        return;
    }

    QColor initial = QColor(p->getColourR(), p->getColourG(), p->getColourB());
    QColor c = QColorDialog::getColor(initial, this,
                                      tr("Choose Item Colour"));
    if (!c.isValid()) return;

    p->setColour(static_cast<unsigned char>(c.red()),
                 static_cast<unsigned char>(c.green()),
                 static_cast<unsigned char>(c.blue()));

    if (isVRActive() && p->getVRActor()) {
        vrThread->requestColourUpdate(p->getVRActor(),
                                      c.redF(), c.greenF(), c.blueF());
    }

    updateRender();
    emit statusUpdateMessage(
        tr("Set colour of '%1' to %2").arg(p->data(0).toString(), c.name()),
        0);
}


void MainWindow::on_checkVisible_toggled(bool checked)
{
    ModelPart* p = selectedPart();
    if (!p) return;

    p->setVisible(checked);
    p->set(1, checked ? "true" : "false");

    if (isVRActive() && p->getVRActor()) {
        vrThread->requestVisibilityUpdate(p->getVRActor(), checked);
    }

    QModelIndex idx = ui->treeView->currentIndex();
    QModelIndex left  = idx.sibling(idx.row(), 0);
    QModelIndex right = idx.sibling(idx.row(), 1);
    emit partList->dataChanged(left, right);

    updateRender();
    emit statusUpdateMessage(
        tr("'%1' visibility = %2")
            .arg(p->data(0).toString(), checked ? "on" : "off"),
        0);
}


/* --------------------------------------------------------------------- */
/*  Animation sliders                                                    */
/* --------------------------------------------------------------------- */

static void updateValueLabel(QLabel* label, int sliderValue)
{
    label->setText(QString::number(sliderValue / 10.0, 'f', 1));
}


void MainWindow::on_sliderRotateX_valueChanged(int v)
{
    updateValueLabel(ui->labelRotateXValue, v);
    if (isVRActive())
        vrThread->issueCommand(VRRenderThread::ROTATE_X, v / 10.0);
}

void MainWindow::on_sliderRotateY_valueChanged(int v)
{
    updateValueLabel(ui->labelRotateYValue, v);
    if (isVRActive())
        vrThread->issueCommand(VRRenderThread::ROTATE_Y, v / 10.0);
}

void MainWindow::on_sliderRotateZ_valueChanged(int v)
{
    updateValueLabel(ui->labelRotateZValue, v);
    if (isVRActive())
        vrThread->issueCommand(VRRenderThread::ROTATE_Z, v / 10.0);
}


/* --------------------------------------------------------------------- */
/*  Desktop renderer refresh                                             */
/* --------------------------------------------------------------------- */

void MainWindow::updateRender()
{
    if (!renderer || !renderWindow) return;

    renderer->RemoveAllViewProps();
    renderer->RemoveAllLights();

    updateRenderFromTree(partList->getRootItem());

    // KEY LIGHT
    {
        vtkSmartPointer<vtkLight> light = vtkSmartPointer<vtkLight>::New();
        light->SetLightTypeToSceneLight();
        light->SetPosition(100, 100, 100);
        light->SetFocalPoint(0, 0, 0);
        light->SetDiffuseColor(currentLightR, currentLightG, currentLightB);
        light->SetSpecularColor(currentLightR, currentLightG, currentLightB);
        light->SetAmbientColor(0.6, 0.6, 0.6);
        light->SetIntensity(currentLightIntensity);
        renderer->AddLight(light);
    }

    // Fill LIGHT
    {
        vtkSmartPointer<vtkLight> fill = vtkSmartPointer<vtkLight>::New();
        fill->SetPosition(-100, -100, 100);
        fill->SetFocalPoint(0, 0, 0);
        fill->SetIntensity(0.4);
        fill->SetDiffuseColor(1, 1, 1);
        renderer->AddLight(fill);
    }
    {
        vtkSmartPointer<vtkLight> back = vtkSmartPointer<vtkLight>::New();
        back->SetPosition(-100, 100, -100);
        back->SetFocalPoint(0, 0, 0);
        back->SetIntensity(0.4);
        back->SetDiffuseColor(1, 1, 1);
        renderer->AddLight(back);
    }

    renderer->ResetCameraClippingRange();
    renderWindow->Render();
}


void MainWindow::updateRenderFromTree(ModelPart* item)
{
    if (!item) return;

    if (item->visible() && item->getActor())
        renderer->AddActor(item->getActor());

    int row = 0;
    while (ModelPart* child = item->child(row++))
        updateRenderFromTree(child);
}
