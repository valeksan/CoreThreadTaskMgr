#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QThread>

struct TestStruct
{
    int a;
    int b;
    int c;
    int operator()(int x)
    {
        return x+100;
    }
};

Q_DECLARE_METATYPE(TestStruct)

struct TestStruct2
{
    int a;
    int b;
    int c;
};

TestStruct test(int a, int b, int c)
{
    return TestStruct{a,b,c};
}

void test_static()
{
    static long t = 0;
    qDebug() << t;
    ++t;
}

void test2(TestStruct s)
{
    s.a += 1;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), m_pCore(new Core())
{
    ui->setupUi(this);

    ui->menuBar->setHidden(true);
    ui->mainToolBar->setHidden(true);

    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

//    try
//    {
//        m_pCore->registerTask(666, test);
//    }
//    catch(const CoreException &ex)
//    {
//        qDebug() << ex.message();
//    }

//    try
//    {
//        m_pCore->addTask(5555);
//    }
//    catch(const CoreException &ex)
//    {
//        qDebug() << ex.message();
//    }

    connect(ui->listWidget, &QListWidget::customContextMenuRequested, this, [=](const QPoint& pos)
    {
        QListWidgetItem *item = ui->listWidget->itemAt(pos);
        if(!item) return;

        QStringList tmp = item->text().split(' ');

        if(item)
        {
            QMenu *pMenu = new QMenu();
            pMenu->setAttribute(Qt::WA_DeleteOnClose);

            pMenu->addAction("StopTask", [=]()
            {
                m_pCore->stopTaskById(tmp[1].toInt());
            });

            pMenu->addAction("TerminateTask", [=]()
            {
                m_pCore->terminateTaskById(tmp[1].toInt());
            });

            pMenu->exec(ui->listWidget->viewport()->mapToGlobal(pos));
        }
    });

    m_pCore->registerTask(TASK0, [=]()
    {
        auto stopFlag = m_pCore->stopTaskFlag();
        forever
        {
            if(stopFlag->load()) return;
            qDebug() << "TASK0" << QThread::currentThread();
            QThread::msleep(1000);
        }
    }, 0);

//    m_pCore->registerTask(TASK0, [=]()
//    {
//        auto stopFlag = m_pCore->stopTaskFlag();
//        forever
//        {
//            if(stopFlag->load()) return;
//            qDebug() << "TASK00" << QThread::currentThread();
//            QThread::msleep(1000);
//        }
//    }, 0);

    m_pCore->registerTask(TASK1, [=]()
    {
        forever
        {
            qDebug() << "TASK1" << QThread::currentThread();
            QThread::msleep(1000);
        }
    }, 1, 2000);

    m_pCore->registerTask(TASK2, [=](int n)
    {
        int step = n;
        auto stopFlag = m_pCore->stopTaskFlag();
        while(step && !stopFlag->load())
        {
            qDebug() << "TASK2" << QThread::currentThread();
            QThread::msleep(1000);
            step--;
        }
    }, 2);


    m_pCore->registerTask(TASK4, &Test::doTest, &t);

    m_pCore->registerTask(TASK5, test);

    m_pCore->registerTask(TASK6, TestStruct());

    try
    {
        m_pCore->registerTask(TASK7, [=]()
        {
            return TestStruct2{1,2,3};
        });
    }
    catch(std::exception &e)
    {
        qDebug() << e.what();
    }

    try
    {
        m_pCore->registerTask(TASK0, test);
    }
    catch(std::exception &e)
    {
        qDebug() << e.what();
    }
    m_pCore->registerTask(TASK8, test2);

//    try
//    {
//        m_pCore->addTask(TASK1, 1,2,3);
//    }
//    catch(const CoreException &ex)
//    {
//        qDebug() << ex.message();
//    }

    connect(m_pCore, &Core::startedTask, this, [=](long id, int type, QVariantList argsList)
    {
        Q_UNUSED(argsList);

        QString t = QString("id %1 / type %2 / group %3").arg(QString::number(id), QString::number(type), QString::number(m_pCore->groupByTask(type)));

        ui->textEdit->append(QString("Task (%1) was started").arg(t));

        ui->listWidget->addItem(t);
    });

    connect(m_pCore, &Core::finishedTask, this, [=](long id, int type, QVariantList argsList, QVariant result)
    {
//        Q_UNUSED(result);
//        Q_UNUSED(argsList);

        if(type == TASK4)
        {
            qDebug() << argsList;
            qDebug() << "Task TASK4 result: "  << result.toInt();
        }
        if(type == TASK5)
        {
            qDebug() << argsList;
            auto r = result.value<TestStruct>();
            qDebug() << "Task TASK5 result: " << r.a << r.b << r.c;
        }
        if(type == TASK6)
        {
            qDebug() << argsList;
            qDebug() << "Task TASK6 result: "  << result.toInt();
        }

        QString t = QString("id %1 / type %2 / group %3").arg(QString::number(id), QString::number(type), QString::number(m_pCore->groupByTask(type)));

        ui->textEdit->append(QString("Task (%1) was finished").arg(t));

        auto listToRemove = ui->listWidget->findItems(t, Qt::MatchExactly);

        foreach(auto elemToRemove, listToRemove) delete elemToRemove;

    });

    connect(m_pCore, &Core::terminatedTask, this, [=](long id, int type, QVariantList argsList)
    {
        Q_UNUSED(argsList);

        QString t = QString("id %1 / type %2 / group %3").arg(QString::number(id), QString::number(type), QString::number(m_pCore->groupByTask(type)));

        ui->textEdit->append(QString("Task (%1) was terminated").arg(t));

        auto listToRemove = ui->listWidget->findItems(t, Qt::MatchExactly);

        foreach(auto elemToRemove, listToRemove) delete elemToRemove;
    });

    connect(ui->pushButtonAddTask0, &QPushButton::clicked, this, [=]()
    {
        m_pCore->addTask(TASK0);
    });

    connect(ui->pushButtonAddTask1, &QPushButton::clicked, this, [=]()
    {
        m_pCore->addTask(TASK1);
    });

    connect(ui->pushButtonAddTask2, &QPushButton::clicked, this, [=]()
    {
        m_pCore->addTask(TASK2, 5);
    });

    connect(ui->pushButtonStopTaskById, &QPushButton::clicked, this, [=]()
    {
        m_pCore->stopTaskById(ui->lineEditId->text().toInt());
    });

    connect(ui->pushButtonStopTaskByType, &QPushButton::clicked, this, [=]()
    {
        m_pCore->stopTaskByType(ui->lineEditType->text().toInt());
    });

    connect(ui->pushButtonStopTaskByGroup, &QPushButton::clicked, this, [=]()
    {
        m_pCore->stopTaskByGroup(ui->lineEditGroup->text().toInt());
    });

    connect(ui->pushButtonStopTasks, &QPushButton::clicked, this, [=]()
    {
        m_pCore->stopTasks();
    });

    try
    {
        m_pCore->addTask(TASK4, 1000, 6, 7);
    }
    catch(std::exception &e)
    {
        qDebug() << e.what();
    }
    try
    {
        m_pCore->addTask(100500, 1000, 6, 7);
    }
    catch(std::exception &e)
    {
        qDebug() << e.what();
    }

    m_pCore->addTask(TASK8, TestStruct{1,2,3});
    m_pCore->addTask(TASK4, 1000, 6);
    m_pCore->addTask(TASK4, 1000, 6);
    m_pCore->addTask(TASK4, 1000, 6);
    m_pCore->addTask(TASK5, 100, 2, 3);
    m_pCore->addTask(TASK6, 77);

//    auto f = std::bind(&Test::doTest, &t, 100, 200);
//    qDebug() << f();

//    auto f = std::bind(&Test::doTest, &t, std::placeholders::_1, std::placeholders::_2);
//    qDebug() << f(100,200);

//    auto f = bind_placeholders1(&Test::doTest, &t, std::make_index_sequence<2>());
//    auto f = std::bind(&Test::doTest, &t, std::integral_constant<int, 1>(), std::integral_constant<int, 2>());

//    auto f1 = std::bind(f, 100, 200);
//    qDebug() << f1();

//    qDebug() << std::is_placeholder<decltype(my_placeholder<1>()) >();
}

MainWindow::~MainWindow()
{
    delete ui;
}
