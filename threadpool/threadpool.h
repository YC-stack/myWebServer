#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

// 线程池类
template <typename T>
class threadpool
{
public:
    /*actor_model是反应堆模型，0-Proactor(默认)，1-Reactor; connPool是数据库连接池指针; thread_number是线程池中线程的数量，默认为8;
     max_requests是请求队列中最多允许的、等待处理的请求的数量, 默认为10000*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T *request, int state); // 加入请求队列
    bool append_p(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;         // 线程池中的线程数
    int m_max_requests;          // 请求队列中允许的最大请求数
    pthread_t *m_threads;        // 描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue;  // 请求队列
    locker m_queuelocker;        // 保护请求队列的互斥锁
    sem m_queuestat;             // 信号量，是否有任务需要处理
    connection_pool *m_connPool; // 数据库连接池
    int m_actor_model;           // 模型切换
};

// 构造函数实现
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests) : m_actor_model(actor_model),
m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL), m_connPool(connPool) {
    if (thread_number <= 0 || max_requests <= 0) {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number]; // 创建大小为m_thread_number的数组存放线程
    if (!m_threads) {
        throw std::exception();
    }
    // 创建线程池,pthread_create函数中将类的对象作为参数传递给静态函数(worker),在静态函数中引用这个对象,并调用其动态方法(run)。
    // 具体的，类对象传递时用this指针，传递给静态函数(worker)后，将其转换为线程池类，并调用私有成员函数run。
    // pthread_create函数原型：
        // int pthread_create (pthread_t *thread_tid,              //返回新生成的线程的id
        //                     const pthread_attr_t *attr,         //指向线程属性的指针,通常设置为NULL
        //                     void * (*start_routine) (void *),   //处理线程函数的地址
        //                     void *arg);                         //start_routine()中的参数
        // 其中第三个参数，为函数指针，指向处理线程函数的地址。该函数，要求为静态函数。如果处理线程函数为类成员函数时，需要将其设置为静态成员函数。
    for (int i=0; i<thread_number; ++i) {
        // 循环创建线程，并将工作线程按要求进行运行
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
        // 将线程进行分离后，不用单独对工作线程进行回收
        if (pthread_detach(m_threads[i])) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

// 析构函数实现
template <typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
}

// append函数实现
template <typename T>
bool threadpool<T>::append(T *request, int state) {
    m_queuelocker.lock(); // 通过互斥锁保证线程安全
    if (m_workqueue.size() >= m_max_requests) { // 如果请求队列的大小 > 允许的最大请求数，则不能加入请求队列
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request); // 成功加入请求队列
    m_queuelocker.unlock();
    // 添加完成后通过信号量提醒有任务要处理
    m_queuestat.post();
    return true; 
}

// append_p函数实现
template <typename T>
bool threadpool<T>::append_p(T *request) {
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// worker函数实现
template <typename T>
void *threadpool<T>::worker(void *arg) {
    threadpool *pool = (threadpool *) arg; // void* 转化为 threadpool*
    pool->run(); // 访问私有成员函数run，完成线程处理要求
    return pool;
}

// run函数实现
template <typename T>
void threadpool<T>::run() { // 主要实现，工作线程从请求队列中取出某个任务进行处理，注意线程同步。
    while (true) {
        m_queuestat.wait(); // 信号量等待
        m_queuelocker.lock(); // 被唤醒后先加互斥锁
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        // 从请求队列中取出第一个任务，将任务从请求队列删除
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        if (1 == m_actor_model) { // 模型为1-Proactor
            if (0 == request->m_state) {
                if (request->read_once()) {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool); // 连接数据库
                    request->process(); // process(模板类中的方法,这里是http类)进行处理
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            } else {
                if (request->write()) {
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        } else { // 模型为0-Reactor
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}

#endif