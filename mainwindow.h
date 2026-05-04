#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

#include "ModelPartList.h"
#include "VRRenderThread.h"

#include <vtkSmartPointer.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class vtkRenderer;
class vtkGenericOpenGLRenderWindow;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    /** Single-line status update emitted to the status bar. */
    void statusUpdateMessage(const QString& message, int timeout);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    /* File menu */
    void on_actionOpen_File_triggered();
    void on_actionOpen_Directory_triggered();
    void on_actionQuit_triggered();

    /* Edit menu */
    void on_actionAdd_Child_Item_triggered();
    void on_actionDelete_Item_triggered();
    void on_actionItem_Options_triggered();

    /* View menu */
    void on_actionBackground_Colour_triggered();
    void on_actionLight_Settings_triggered();
    void on_actionDark_Mode_triggered(bool checked);
    void on_actionReset_Camera_triggered();

    /* VR menu */
    void on_actionStart_VR_triggered();
    void on_actionStop_VR_triggered();
    void on_actionReset_VR_View_triggered();
    void on_actionToggle_Spin_triggered();
    void on_actionToggle_Floor_triggered();

    /* Help menu */
    void on_actionAbout_triggered();

    /* VR side-panel buttons */
    void on_btnStartVR_clicked();
    void on_btnStopVR_clicked();
    void on_btnResetView_clicked();
    void on_btnToggleSpin_clicked();
    void on_btnToggleFloor_clicked();
    void on_btnBackgroundColour_clicked();
    void on_btnLightSettings_clicked();
    void on_btnDarkMode_toggled(bool checked);
    void on_btnQuickColour_clicked();
    void on_checkVisible_toggled(bool checked);

    /* Sliders */
    void on_sliderRotateX_valueChanged(int v);
    void on_sliderRotateY_valueChanged(int v);
    void on_sliderRotateZ_valueChanged(int v);

    /* Tree view */
    void handleTreeClicked();

private:
    /* --- Helpers --- */
    void   updateRender();
    void   updateRenderFromTree(class ModelPart* item);
    void   loadStlIntoItem(class ModelPart* item, const QString& filePath);
    void   refreshQuickPanelForSelection();
    class  ModelPart* selectedPart() const;
    bool   isVRActive() const;

private:
    Ui::MainWindow* ui;

    /* Lighting state - kept on the main window so light dialog can edit it. */
    double currentLightIntensity = 1.0;
    double currentLightR         = 1.0;
    double currentLightG         = 1.0;
    double currentLightB         = 1.0;

    ModelPartList* partList = nullptr;

    vtkSmartPointer<vtkRenderer>                 renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;

    bool    isDarkMode = false;

    /* VR thread */
    QPointer<VRRenderThread> vrThread;
};

#endif
