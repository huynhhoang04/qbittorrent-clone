#include <string>
#pragma once
#include <bits/stdc++.h>
using namespace std;

string hash_buffer(const string& data);
string hash_file(const string& path);

class SHA256{
    public:
        string hash_buffer(const string& data);
        string hash_file(const string& path);
};