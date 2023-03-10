#ifndef CORE_H
#define CORE_H

#include <QObject>
#include <QQueue>
#include <QDebug>
#include <QTimer>
#include <QException>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <atomic>
#include <functional>
#include <experimental/any>

#define STOP_ACTIVE_TASK_DEFAULT_TIMEOUT 5000

template<typename R, typename...Args, typename Class, std::size_t ...N>
auto bind_placeholders(R(Class::*taskFunction)(Args...), Class *taskObj, std::index_sequence<N...>) { return std::bind(taskFunction, taskObj, std::integral_constant<int, N+1>()...); }

template<typename T> struct remove_class { };
template<typename C, typename R, typename... Args>
struct remove_class<R(C::*)(Args...)> { using type = R(Args...); };
template<typename C, typename R, typename... Args>
struct remove_class<R(C::*)(Args...) const> { using type = R(Args...); };
template<typename C, typename R, typename... Args>
struct remove_class<R(C::*)(Args...) volatile> { using type = R(Args...); };
template<typename C, typename R, typename... Args>
struct remove_class<R(C::*)(Args...) const volatile> { using type = R(Args...); };

template<typename T>
struct get_signature_impl { using type = typename remove_class<decltype(&std::remove_reference<T>::type::operator())>::type; };
template<typename R, typename... Args>
struct get_signature_impl<R(Args...)> { using type = R(Args...); };
template<typename R, typename... Args>
struct get_signature_impl<R(&)(Args...)> { using type = R(Args...); };
template<typename R, typename... Args>
struct get_signature_impl<R(*)(Args...)> { using type = R(Args...); };
template<typename T> using get_signature = typename get_signature_impl<T>::type;

template<typename F> using make_function_type = std::function<get_signature<F> >;
template<typename F> make_function_type<F> make_function(F &&f) { return make_function_type<F>(std::forward<F>(f)); }

class CoreException: public QException
{
public:
    CoreException(QString message) : m_message(message)
    {
        ;
    }
    void raise() const { throw *this; }
    CoreException *clone() const { return new CoreException(*this); }

    QString message() const
    {
        return m_message;
    }

private:
    QString m_message;
};

class TaskHelper : public QObject
{
    Q_OBJECT
public:
    TaskHelper(std::function<QVariant()> function, QObject *parent = nullptr) : QObject(parent), m_function(function)
    {

    }

#ifdef Q_OS_WIN
    static DWORD WINAPI functionWrapper(void *pTaskHelper)
    {
        TaskHelper *pThisTaskHelper = qobject_cast<TaskHelper *>(reinterpret_cast<QObject *>(pTaskHelper));
        if(pThisTaskHelper) pThisTaskHelper->execute();
        return 0;
    }
#else
    static void *functionWrapper(void *pTaskHelper)
    {
        TaskHelper *pThisTaskHelper = qobject_cast<TaskHelper *>(reinterpret_cast<QObject *>(pTaskHelper));
        if(pThisTaskHelper) pThisTaskHelper->execute();
        return 0;
    }
#endif

private:
    void execute()
    {
        emit finished(m_function());
    }

    std::function<QVariant()> m_function;

signals:
    void finished(QVariant result);
};

class Core : public QObject
{
    Q_OBJECT
public:
    explicit Core(QObject *parent = 0) : QObject(parent), m_blockStartTask(false)
    {

    }

    template <typename R, typename...Args>
    void registerTask(int taskType, std::function<R(Args...)> taskFunction, int taskGroup = 0, int taskStopTimeout = 1000, typename std::enable_if<std::is_same<R,void>::value>::type* = 0)
    {
        std::function<QVariant(Args...)> f = [taskFunction](Args... args)
        {
            taskFunction(args...);
            return QVariant();
        };

        m_taskHash.insert(taskType, TaskInfo{f, taskGroup, taskStopTimeout});
    }

    template <typename R, typename...Args>
    void registerTask(int taskType, std::function<R(Args...)> taskFunction, int taskGroup = 0, int taskStopTimeout = 1000, typename std::enable_if<std::is_convertible<R, QVariant>::value>::type* = 0)
    {
        if(std::is_same<R, QVariant>::value) m_taskHash.insert(taskType, TaskInfo{taskFunction, taskGroup, taskStopTimeout});
        else
        {
            std::function<QVariant(Args...)> f = taskFunction;
            m_taskHash.insert(taskType, TaskInfo{f, taskGroup, taskStopTimeout});
        }
    }

    template <typename R, typename...Args>
    void registerTask(int taskType, std::function<R(Args...)> taskFunction, int taskGroup = 0, int taskStopTimeout = 1000, typename std::enable_if<!std::is_convertible<R, QVariant>::value && !std::is_same<R,void>::value && QMetaTypeId<R>::Defined>::type* = 0)
    {
        std::function<QVariant(Args...)> f = [taskFunction](Args... args)
        {
            return QVariant::fromValue(taskFunction(args...));
        };

        m_taskHash.insert(taskType, TaskInfo{f, taskGroup, taskStopTimeout});
    }

    template <typename R, typename...Args>
    void registerTask(int taskType, std::function<R(Args...)> taskFunction, int taskGroup = 0, int taskStopTimeout = 1000, typename std::enable_if<!std::is_convertible<R, QVariant>::value && !std::is_same<R,void>::value && !QMetaTypeId<R>::Defined>::type* = 0)
    {
        Q_UNUSED(taskType);
        Q_UNUSED(taskFunction);
        Q_UNUSED(taskGroup);
        Q_UNUSED(taskStopTimeout);

        qDebug() << "Core::registerTask - Not convertible return type";
        throw CoreException("Not convertible return type");
    }

    template <typename F>
    void registerTask(int taskType, F &&taskFunction, int taskGroup = 0, int taskStopTimeout = 1000)
    {
        registerTask(taskType, make_function(taskFunction), taskGroup, taskStopTimeout);
    }

    template <typename R, typename...Args>
    void registerTask(int taskType, R(*taskFunction)(Args...), int taskGroup = 0, int taskStopTimeout = 1000)
    {
        registerTask(taskType, std::function<R(Args...)>(taskFunction), taskGroup, taskStopTimeout);
    }

    template <typename Class, typename R, typename...Args>
    void registerTask(int taskType, R(Class::*taskMethod)(Args...), Class *taskObj, int taskGroup = 0, int taskStopTimeout = 1000)
    {
        std::function<R(Args...)> f = bind_placeholders(taskMethod, taskObj, std::make_index_sequence<sizeof...(Args)>());
        registerTask(taskType, f, taskGroup, taskStopTimeout);
    }

    template <typename...Args>
    void addTask(int taskType, Args... args)
    {
        static long id = 0;

        if(!m_taskHash.contains(taskType))
        {
            qDebug() << "Core::addTask - Task not registred";
            throw CoreException("Task not registred");
//            return;
        }
        try
        {
            std::function<QVariant(Args...)> taskFunction = std::experimental::any_cast<std::function<QVariant(Args...)> >(m_taskHash[taskType].m_function);
            auto taskFunctionBound = std::bind(taskFunction, args...);

            QSharedPointer<Task> pTask = QSharedPointer<Task>::create(taskFunctionBound, taskType, m_taskHash[taskType].m_group, QVariantList({ QVariant::fromValue(args)... }));

            pTask->m_id = id;

            id++;

            bool start = std::all_of(m_activeTaskList.begin(), m_activeTaskList.end(), [pTask](QSharedPointer<Task> pActiveTask)
            {
                if(pTask->m_group == pActiveTask->m_group) return false;
                else return true;
            });

            if(start && !m_blockStartTask)
            {
                startTask(pTask);
            }
            else m_queuedTaskList.append(pTask);
        }
        catch(std::experimental::bad_any_cast)
        {
            qDebug() << "Core::addTask - Bad arguments";
            throw CoreException("Bad arguments");
//            return;
        }
    }

    std::atomic_bool *stopTaskFlag()
    {
        foreach(auto task, m_activeTaskList)
        {
#ifdef Q_OS_WIN
            if(task->m_threadId == GetCurrentThreadId()) return &task->m_stopFlag;
#else
            if(pthread_equal(pthread_self(), task->m_threadHandle)) return &task->m_stopFlag;
#endif
        }
        return nullptr;
    }

    void terminateTaskById(long id)
    {
        auto pTask = activeTaskById(id);

        if(pTask.isNull()) return;

        terminateTask(pTask);
    }

    void stopTaskById(long id)
    {
        auto pTask = activeTaskById(id);

        if(pTask.isNull()) return;

        stopTask(pTask);
    }

    void stopTaskByType(int type)
    {
        auto pTask = activeTaskByType(type);

        if(pTask.isNull()) return;

        stopTask(pTask);
    }

    void stopTaskByGroup(int group)
    {
        auto pTask = activeTaskByGroup(group);

        if(pTask.isNull()) return;

        stopTask(pTask);
    }

    void stopTasks()
    {
        m_blockStartTask = true;
        QTimer *pTimer = new QTimer();
        connect(pTimer, &QTimer::timeout, this, [=]()
        {
            if(m_activeTaskList.isEmpty())
            {
                m_blockStartTask = false;
                pTimer->stop();
                pTimer->deleteLater();
            }
        });

        int stopTasksTimeout = 0;
        foreach(auto pTask, m_activeTaskList)
        {
            int stopTaskTimeout = m_taskHash[pTask->m_type].m_stopTimeout;
            if(stopTaskTimeout > stopTasksTimeout) stopTasksTimeout = stopTaskTimeout;
            stopTask(pTask);
        }

        pTimer->start(stopTasksTimeout);
    }

    bool isTaskRegistred(int type)
    {
        if(m_taskHash.contains(type)) return true;
        else return false;
    }

    int groupByTask(int type, bool *ok = nullptr)
    {
        if(isTaskRegistred(type))
        {
            if(ok) *ok = true;
            return m_taskHash[type].m_group;
        }
        else
        {
            if(ok) *ok = false;
            return -1;
        }
    }

private:
    enum TaskState
    {
        INACTIVE,
        ACTIVE,
        FINISHED,
        TERMINATED
    };

    struct TaskInfo
    {
        std::experimental::any m_function;
        int m_group;
        int m_stopTimeout;
    };

    struct Task
    {
        Task(std::function<QVariant()> functionBound, int type, int group, QVariantList argsList = QVariantList())
        {
            m_id = -1;
            m_functionBound = functionBound;
            m_type = type;
            m_group = group;
            m_argsList = argsList;
            m_stopFlag.store(false);
            m_state = INACTIVE;
        }

        long m_id;
        std::function<QVariant()> m_functionBound;
        int m_type;
        int m_group;
        QVariantList m_argsList;
#ifdef Q_OS_WIN
        HANDLE m_threadHandle;
        DWORD m_threadId;
#else
        pthread_t m_threadHandle;
#endif
        std::atomic_bool m_stopFlag;

        TaskState m_state;
    };

    QSharedPointer<Task> activeTaskById(long id)
    {
        foreach(auto pTask, m_activeTaskList)
        {
            if(pTask->m_id == id) return pTask;
        }

        return nullptr;
    }

    QSharedPointer<Task> activeTaskByType(int type)
    {
        foreach(auto pTask, m_activeTaskList)
        {
            if(pTask->m_type == type) return pTask;
        }

        return nullptr;
    }

    QSharedPointer<Task> activeTaskByGroup(int group)
    {
        foreach(auto pTask, m_activeTaskList)
        {
            if(pTask->m_group == group) return pTask;
        }

        return nullptr;
    }

    void terminateTask(QSharedPointer<Task> pTask)
    {
#ifdef Q_OS_WIN
        TerminateThread(pTask->m_threadHandle, 0);
#else
        pthread_cancel(pTask->m_threadHandle);
#endif
        pTask->m_state = TERMINATED;

        emit terminatedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);

        m_activeTaskList.removeOne(pTask);

        startQueuedTask();
    }

    void stopTask(QSharedPointer<Task> pTask)
    {
        pTask->m_stopFlag.store(true);
        QTimer::singleShot(m_taskHash[pTask->m_type].m_stopTimeout, this, [=]()
        {
            switch(pTask->m_state)
            {
            case FINISHED:
                qDebug() << QString("Task %1 was sucessfully stopped").arg(QString::number(pTask->m_id));
                break;
            case TERMINATED:
                qDebug() << QString("Task %1 was terminated").arg(QString::number(pTask->m_id));
                break;
            case ACTIVE:
                qDebug() << QString("Task %1 was not stopped, terminating").arg(QString::number(pTask->m_id));
                terminateTask(pTask);
                break;
            default:
                qDebug() << QString("Task %1 unexpected state").arg(QString::number(pTask->m_id));
                break;
            }
        });
    }

    void startTask(QSharedPointer<Task> pTask)
    {
        m_activeTaskList.append(pTask);

        pTask->m_state = ACTIVE;

        TaskHelper *pTaskHelper = new TaskHelper(pTask->m_functionBound);

        connect(pTaskHelper, &TaskHelper::finished, this, [=](QVariant result)
        {
            pTask->m_state = FINISHED;

            emit finishedTask(pTask->m_id, pTask->m_type, pTask->m_argsList, result);

            m_activeTaskList.removeOne(pTask);

            startQueuedTask();

            if(!isActive()) {
                emit emptyTasks();
            }

            pTaskHelper->deleteLater();
        });

#ifdef Q_OS_WIN
        pTask->m_threadHandle = CreateThread(NULL, 0, &TaskHelper::functionWrapper, reinterpret_cast<void *>(pTaskHelper), 0, &pTask->m_threadId);
#else
        pthread_create(&pTask->m_threadHandle, NULL, &TaskHelper::functionWrapper, pTaskHelper);
        pthread_detach(pTask->m_threadHandle);
#endif

        emit startedTask(pTask->m_id, pTask->m_type, pTask->m_argsList);
    }

    void startQueuedTask()
    {
        foreach(auto pQueuedTask, m_queuedTaskList)
        {
            bool start = std::all_of(m_activeTaskList.begin(), m_activeTaskList.end(), [pQueuedTask](QSharedPointer<Task> pActiveTask)
            {
                if(pQueuedTask->m_group == pActiveTask->m_group) return false;
                else return true;
            });

            if(start)
            {
                m_queuedTaskList.removeOne(pQueuedTask);
                startTask(pQueuedTask);
            }
        }
    }

    QHash<int, TaskInfo> m_taskHash;

    QList<QSharedPointer<Task> > m_activeTaskList;
    QList<QSharedPointer<Task> > m_queuedTaskList;

    bool m_blockStartTask;

signals:
    void finishedTask(long id, int type, QVariantList argsList = QVariantList(), QVariant result = QVariant());
    void startedTask(long id, int type, QVariantList argsList = QVariantList());
    void terminatedTask(long id, int type, QVariantList argsList = QVariantList());
    void emptyTasks();

public slots:

    bool isTaskAdded(int taskType) {
        foreach(QSharedPointer<Task> pTask, m_activeTaskList)
            if(pTask->m_type == taskType)
                return true;
        foreach(QSharedPointer<Task> pTask, m_queuedTaskList)
            if(pTask->m_type == taskType)
                return true;
        return false;
    }
    bool isActive() {
        return (m_activeTaskList.size() > 0 || m_queuedTaskList.size() > 0);
    }

};

#endif // CORE_H
