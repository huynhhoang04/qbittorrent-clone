#pragma once
#include <bits/stdc++.h>
#include <mutex>
using namespace std;

class Logger{
    private:
        static mutex mtx;
    public:
        void info(const string& msg);
        void success(const string& msg);
        void warnning(const string& msg);
        void err(const string& msg);
};  