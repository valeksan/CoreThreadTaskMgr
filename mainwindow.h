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
    TASK3,
    TASK4,
    TASK5,
    TASK6,
    TASK7,
    TASK8,
    TASK9
};

class Test
{
public:
    Test()
    {
        ;
    }

    int doTest(int a, int b)
    {
        return a+b;
    }
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

    Test t;

public slots:
};

#endif // MAINWINDOW_H
