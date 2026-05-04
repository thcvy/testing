/**     @file ModelPart.h
 *
 *      EEEE2076 - Software Engineering & VR Project
 *
 *      Tree item that owns the VTK pipeline for a single CAD part.
 *      Each part has both a GUI actor (used in the QVTKOpenGLNativeWidget)
 *      and a VR actor (used in the OpenVR scene). Updates to colour /
 *      visibility / filters are applied to BOTH so what you see in the
 *      desktop window matches what you see in the headset.
 */
#ifndef VIEWER_MODELPART_H
#define VIEWER_MODELPART_H

#include <QString>
#include <QList>
#include <QVariant>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkSmartPointer.h>

/* Forward declare VTK classes - keeps header lightweight */
class vtkMapper;
class vtkActor;
class vtkSTLReader;
class vtkShrinkFilter;
class vtkClipDataSet;
class vtkPlane;
class vtkAlgorithmOutput;

class ModelPart
{
public:
    /** Constructor
     * @param data is a List (array) of strings for each property of this item
     * @param parent is the parent of this item (one level up in tree)
     */
    ModelPart(const QList<QVariant>& data, ModelPart* parent = nullptr);

    /** Destructor */
    ~ModelPart();

    /* --- Tree management --- */
    void appendChild(ModelPart* item);
    void removeChild(int row);
    ModelPart* child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    void set(int column, const QVariant& value);
    ModelPart* parentItem();
    int row() const;

    /* --- Visual properties --- */
    void setColour(unsigned char R, unsigned char G, unsigned char B);
    unsigned char getColourR();
    unsigned char getColourG();
    unsigned char getColourB();

    void setVisible(bool isVisible);
    bool visible();

    /* --- File loading --- */
    void loadSTL(QString fileName);

    /** Clear the loaded STL and pipeline (used for "remove model" actions). */
    void clearSTL();

    /** Whether this part currently has an STL loaded. */
    bool hasSTL() const { return file != nullptr; }

    /* --- Filters --- */
    void    setFilters(QString f1, QString f2);
    QString getFilter1();
    QString getFilter2();
    void    rebuildPipeline();
    vtkAlgorithmOutput* applyFilter(QString filterName,
                                    vtkAlgorithmOutput* inputPort);

    /* --- VTK actor access --- */
    /** Actor used in the desktop QVTKOpenGLNativeWidget. */
    vtkActor* getActor();

    /** Actor used in the VR scene. Created on first request, kept around so
     *  GUI changes (colour/visibility) can be reflected in VR while it is
     *  running. */
    vtkActor* createVRActor();

    /** Returns the VR actor without creating one if it doesn't exist. */
    vtkActor* getVRActor();

private:
    /* Colour, kept here so we can re-apply when actors are rebuilt. */
    unsigned char m_r = 200;
    unsigned char m_g = 200;
    unsigned char m_b = 200;

    QList<ModelPart*> m_childItems;
    QList<QVariant>   m_itemData;
    ModelPart*        m_parentItem;

    bool isVisible;

    /* GUI VTK pipeline */
    vtkSmartPointer<vtkSTLReader> file;
    vtkSmartPointer<vtkMapper>    mapper;
    vtkSmartPointer<vtkActor>     actor;

    /* Independent VR pipeline so we can transform / animate it without
     * affecting the desktop view, and vice-versa.                      */
    vtkSmartPointer<vtkPolyDataMapper> vrMapper;
    vtkSmartPointer<vtkActor>          vrActor;

    /* Filters - kept as members so the smart pointers stay alive. */
    vtkSmartPointer<vtkShrinkFilter> shrinkFilter;
    vtkSmartPointer<vtkClipDataSet>  clipFilter;
    vtkSmartPointer<vtkPlane>        clipPlane;

    QString filter1 = "None";
    QString filter2 = "None";
};

#endif
