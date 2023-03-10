#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QApplication>
#include <QMainWindow>
#include "core.h"

namespace Ui {
class MainWindow;
}

enum Tasks
{
    TASK0,
    TASK1,
    TASK2,
    TASK3
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:


private:
    Ui::MainWindow *ui;

    Core *m_pCore;

public slots:
};

#endif // MAINWINDOW_H
