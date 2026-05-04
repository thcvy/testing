#include "lightdialog.h"
#include "ui_lightdialog.h"

LightDialog::LightDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LightDialog)
{
    ui->setupUi(this);
}

LightDialog::~LightDialog()
{
    delete ui;
}

void LightDialog::setValues(int intensity, int r, int g, int b)
{
    ui->horizontalSliderIntensity->setValue(intensity);
    ui->spinBoxR->setValue(r);
    ui->spinBoxG->setValue(g);
    ui->spinBoxB->setValue(b);
}

void LightDialog::getValues(int &intensity, int &r, int &g, int &b)
{
    intensity = ui->horizontalSliderIntensity->value();
    r = ui->spinBoxR->value();
    g = ui->spinBoxG->value();
    b = ui->spinBoxB->value();
}
