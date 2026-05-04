#include "ModelPart.h"

#include <vtkSTLReader.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkShrinkFilter.h>
#include <vtkClipDataSet.h>
#include <vtkPlane.h>
#include <vtkAlgorithmOutput.h>
#include <vtkDataSetMapper.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>


ModelPart::ModelPart(const QList<QVariant>& data, ModelPart* parent)
    : m_itemData(data), m_parentItem(parent)
{
    isVisible = true;
    filter1   = "None";
    filter2   = "None";
}

ModelPart::~ModelPart()
{
    qDeleteAll(m_childItems);
}

// Tree management  */

void ModelPart::appendChild(ModelPart* item)
{
    if (!item) return;
    item->m_parentItem = this;
    m_childItems.append(item);
}

void ModelPart::removeChild(int row)
{
    if (row < 0 || row >= m_childItems.size())
        return;
    delete m_childItems.takeAt(row);
}

ModelPart* ModelPart::child(int row)
{
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

int ModelPart::childCount() const  { return m_childItems.count(); }
int ModelPart::columnCount() const { return m_itemData.count();   }

QVariant ModelPart::data(int column) const
{
    if (column < 0 || column >= m_itemData.size())
        return QVariant();
    return m_itemData.at(column);
}

void ModelPart::set(int column, const QVariant& value)
{
    if (column < 0 || column >= m_itemData.size())
        return;
    m_itemData.replace(column, value);
}

ModelPart* ModelPart::parentItem() { return m_parentItem; }

int ModelPart::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<ModelPart*>(this));
    return 0;
}

/*  Visual properties  */

void ModelPart::setColour(unsigned char R, unsigned char G, unsigned char B)
{
    m_r = R;
    m_g = G;
    m_b = B;

    const double r = m_r / 255.0;
    const double g = m_g / 255.0;
    const double b = m_b / 255.0;

    if (actor)   actor->GetProperty()->SetColor(r, g, b);
    if (vrActor) vrActor->GetProperty()->SetColor(r, g, b);
}

unsigned char ModelPart::getColourR() { return m_r; }
unsigned char ModelPart::getColourG() { return m_g; }
unsigned char ModelPart::getColourB() { return m_b; }

void ModelPart::setVisible(bool v)
{
    isVisible = v;
    if (actor)   actor->SetVisibility(v ? 1 : 0);
    if (vrActor) vrActor->SetVisibility(v ? 1 : 0);
}

bool ModelPart::visible() { return isVisible; }

/* Filters  */

void ModelPart::setFilters(QString f1, QString f2)
{
    filter1 = f1;
    filter2 = f2;
    rebuildPipeline();
}

QString ModelPart::getFilter1() { return filter1; }
QString ModelPart::getFilter2() { return filter2; }

vtkAlgorithmOutput* ModelPart::applyFilter(QString filterName,
                                           vtkAlgorithmOutput* inputPort)
{
    if (filterName == "Shrink") {
        shrinkFilter = vtkSmartPointer<vtkShrinkFilter>::New();
        shrinkFilter->SetInputConnection(inputPort);
        shrinkFilter->SetShrinkFactor(0.8);
        shrinkFilter->Update();
        return shrinkFilter->GetOutputPort();
    }

    if (filterName == "Clip") {
        double bounds[6];
        file->GetOutput()->GetBounds(bounds);
        const double xMiddle = (bounds[0] + bounds[1]) / 2.0;

        clipPlane = vtkSmartPointer<vtkPlane>::New();
        clipPlane->SetOrigin(xMiddle, 0.0, 0.0);
        clipPlane->SetNormal(1.0, 0.0, 0.0);

        clipFilter = vtkSmartPointer<vtkClipDataSet>::New();
        clipFilter->SetInputConnection(inputPort);
        clipFilter->SetClipFunction(clipPlane);
        clipFilter->Update();
        return clipFilter->GetOutputPort();
    }

    return inputPort;
}

void ModelPart::rebuildPipeline()
{
    if (!file) return;

    // Re-build GUI pipeline */
    vtkSmartPointer<vtkDataSetMapper> dsMapper =
        vtkSmartPointer<vtkDataSetMapper>::New();

    vtkAlgorithmOutput* outputPort = file->GetOutputPort();
    outputPort = applyFilter(filter1, outputPort);
    outputPort = applyFilter(filter2, outputPort);

    dsMapper->SetInputConnection(outputPort);
    mapper = dsMapper;

    if (!actor)
        actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    actor->GetProperty()->SetColor(m_r / 255.0, m_g / 255.0, m_b / 255.0);
    actor->GetProperty()->SetInterpolationToPhong();
    actor->GetProperty()->SetSpecular(0.5);
    actor->GetProperty()->SetSpecularPower(60);
    actor->GetProperty()->SetSpecularColor(1.0, 1.0, 1.0);
    actor->SetVisibility(isVisible ? 1 : 0);


    if (vrActor) {
        vrMapper = vtkSmartPointer<vtkPolyDataMapper>::New();                         
        vrMapper->SetInputConnection(file->GetOutputPort());
        vrActor->SetMapper(vrMapper);
        vrActor->GetProperty()->SetColor(
            m_r / 255.0, m_g / 255.0, m_b / 255.0);
        vrActor->SetVisibility(isVisible ? 1 : 0);
    }
}

/*  File I/O  */

void ModelPart::loadSTL(QString fileName)
{
    file = vtkSmartPointer<vtkSTLReader>::New();
    file->SetFileName(fileName.toStdString().c_str());
    file->Update();

    filter1 = "None";
    filter2 = "None";

    rebuildPipeline();
}

void ModelPart::clearSTL()
{
    file         = nullptr;
    mapper       = nullptr;
    actor        = nullptr;
    vrMapper     = nullptr;
    vrActor      = nullptr;
    shrinkFilter = nullptr;
    clipFilter   = nullptr;
    clipPlane    = nullptr;

    filter1 = "None";
    filter2 = "None";
}

/*  VTK accessors  */

vtkActor* ModelPart::getActor()  { return actor;   }
vtkActor* ModelPart::getVRActor() { return vrActor; }

vtkActor* ModelPart::createVRActor()
{
    if (!file) return nullptr;

    vrMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vrMapper->SetInputConnection(file->GetOutputPort());

    vrActor = vtkSmartPointer<vtkActor>::New();
    vrActor->SetMapper(vrMapper);
    vrActor->GetProperty()->SetColor(
        m_r / 255.0, m_g / 255.0, m_b / 255.0);
    vrActor->GetProperty()->SetInterpolationToPhong();
    vrActor->GetProperty()->SetSpecular(0.4);
    vrActor->GetProperty()->SetSpecularPower(40);
    vrActor->SetVisibility(isVisible ? 1 : 0);

    return vrActor;
}
