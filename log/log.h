#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log
{
public:
    // C++11以后,使用局部静态变量懒汉不用加锁
    static Log *get_instance() {
        static Log instance;
        return &instance;
    }
    // 异步写日志公有方法，内部调用私有方法async_write_log
    static void *flush_log_thread(void *args) {
        Log::get_instance()->async_write_log();
    }
    // 日志创建及初始化，可选择的参数有：日志文件名、是否关闭日志、日志缓冲区大小、最大行数、最大阻塞队列大小(如果设置为0,表示同步写入，否则表示异步写入)
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
    // 完成写入日志文件中的具体内容，主要实现日志分级、分文件、格式化输出内容
    void write_log(int level, const char *format, ...);
    // 强制刷新缓冲区
    void flush(void);

private:
    Log();
    virtual ~Log();
    // 异步写日志方法
    void *async_write_log() {
        string single_log;
        // 从阻塞队列中取出一个日志string，写入文件
        while (m_log_queue->pop(single_log)) {
            m_mutex.lock();
            // 第一个参数是要写入的以空字符终止的字符序列，第二个参数是指向FILE对象的指针，该FILE对象标识了要被写入字符串的流
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; // 路径名
    char log_name[128]; // log文件名
    int m_split_lines;  // 日志最大行数
    int m_log_buf_size; // 日志缓冲区大小
    long long m_count;  // 日志行数记录
    int m_today;        // 因为按天分类,记录当前时间是那一天
    FILE *m_fp;         // 打开log的文件指针
    char *m_buf;        // 要输出的内容
    block_queue<string> *m_log_queue; // 阻塞队列
    bool m_is_async; // 是否异步标志位
    locker m_mutex;  // 互斥锁
    int m_close_log; // 是否关闭日志
};

// 这四个宏定义在其他文件中使用，主要用于不同类型的日志输出
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif