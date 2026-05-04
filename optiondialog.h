#ifndef OPTIONDIALOG_H
#define OPTIONDIALOG_H

#include <QDialog>

namespace Ui {
class OptionDialog;
}

class OptionDialog : public QDialog
{
    Q_OBJECT

    public:

    explicit OptionDialog(QWidget *parent = nullptr);
    ~OptionDialog();

    // MODEL SETTINGS ONLY
    void setValues(const QString& name, int r, int g, int b, bool visible);
    void getValues(QString& name, int& r, int& g, int& b, bool& visible);


    QString getFilter1() const;
    QString getFilter2() const;


    void setFilters(QString f1, QString f2);  

    private:

    Ui::OptionDialog *ui;
private slots:

};

#endif // OPTIONDIALOG_H
