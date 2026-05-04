#ifndef LIGHTDIALOG_H
#define LIGHTDIALOG_H

#include <QDialog>

namespace Ui {
class LightDialog;
}

class LightDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LightDialog(QWidget *parent = nullptr);
    ~LightDialog();

    void setValues(int intensity, int r, int g, int b);
    void getValues(int &intensity, int &r, int &g, int &b);

private:
    Ui::LightDialog *ui;
};

#endif // LIGHTDIALOG_H
