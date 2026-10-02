#include "logger.hpp"
#include <bits/stdc++.h>
#include <mutex>
using namespace std;

mutex Logger::mtx;

void Logger::info(const string &msg){
    lock_guard<mutex> lock(mtx);
    cout<< "\033[34m" << "[Info]" << msg << endl;
}

void Logger::success(const string &msg){
    lock_guard<mutex> lock(mtx);
    cout<< "\033[32m" << "[Sucess" << msg << endl;
}

void Logger::warnning(const string &msg){
    lock_guard<mutex> lock(mtx);
    cout<< "\033[33m" << "[Warning]" << msg << endl;
}

void Logger::err(const string &msg){
    lock_guard<mutex> lock(mtx);
    cout<< "\033[31m" << "[Err]" << msg << endl;
}