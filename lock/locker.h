#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem { //信号量
public:
    // 无参构造
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) { // 信号量初始化
            throw std::exception();
        }
    }
    // 有参构造
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) { 
            throw std::exception();
        }
    }
    // 析构函数
    ~sem() {
        sem_destroy(&m_sem); // 信号量销毁
    }
    // 以原子操作方式将信号量减一,信号量为0时,sem_wait阻塞
    bool wait() {
        return sem_wait(&m_sem) == 0;
    }
    // 以原子操作方式将信号量加一,信号量大于0时,唤醒调用sem_post的线程
    bool post() {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;

};

class locker { // 互斥锁
public:
    // 构造
    locker() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) { //互斥锁初始化
            throw std::exception();
        }
    }
    // 析构
    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }
    // 加锁
    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    // 解锁
    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t * get() {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

class cond { // 条件变量
public:
    // 构造
    cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) { // 条件变量初始化
            throw std::exception();
        }
    }
    // 析构
    ~cond() {
        pthread_cond_destroy(&m_cond); 
    }
    // 等待目标条件变量
    bool wait(pthread_mutex_t *m_mutex) {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        // 函数执行时,先把调用线程放入条件变量的请求队列,然后将互斥锁mutex解锁,当函数成功返回为0时,互斥锁会再次被锁上.即函数内部会有一次解锁和加锁操作
        ret = pthread_cond_wait(&m_cond, m_mutex);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    // 超时等待
    bool timewait(pthread_mutex_t * m_mutex, struct timespec t) {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        // 当在指定时间内有信号传过来时，pthread_cond_timedwait()返回0，否则返回一个非0数
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    // 发送一个信号给另外一个正在处于阻塞等待状态的线程,使其脱离阻塞状态,继续执行(存在多个等待线程时按入队顺序激活第一个)
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }
    // 以广播的方式唤醒所有等待目标条件变量的线程
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0; 
    }
private:
    pthread_cond_t m_cond;
};

#endif