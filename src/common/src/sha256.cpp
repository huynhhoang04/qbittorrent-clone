#include <bits/stdc++.h>
#include <cstddef>
#include <cstdio>
#include <ios>
#include <openssl/evp.h>
#include "sha256.hpp"
#include <string>
#include <fstream>
#include <unistd.h>
#include "logger.hpp"
using namespace std;

string SHA256::hash_file(const string &path){
    Logger log;
    log.info("Start hashsing file :" + path);

    EVP_MD_CTX* newcontext = EVP_MD_CTX_new();
    if(!newcontext or newcontext==NULL){
        log.err("Fail to create hash context!");
        return "";
    }

    if(EVP_DigestInit_ex(newcontext, EVP_sha256(), nullptr) != 1){
        EVP_MD_CTX_free(newcontext);
        log.err("Fail to init digest!");
        return "";
    }

    fstream file(path, ios_base::in | ios_base::binary);
    char buffer[4096];
    if (!file.is_open()){
        log.err("File is open!");
        EVP_MD_CTX_free(newcontext);
        return "";
    }
    else {
        while (file.read(buffer, sizeof(buffer))) {
            streamsize bytesread = file.gcount();
            EVP_DigestUpdate(newcontext, buffer, bytesread);
        }
        if (file.gcount() > 0) {
            EVP_DigestUpdate(newcontext, buffer, file.gcount());
        }
    }
    file.close();

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    int rethash = EVP_DigestFinal_ex(newcontext, hash, &len);
    if (rethash != 1) {
        EVP_MD_CTX_free(newcontext);
        log.err("Hash fail!");
        return "";
    }

    string ret;
    for (unsigned int idx = 0; idx<len; ++idx) {
        char hashbuf[3];
        sprintf(hashbuf, "%02x", hash[idx]);
        ret.append(hashbuf);
    }
    EVP_MD_CTX_free(newcontext);
    log.success("Success");
    return ret;
}

string SHA256::hash_buffer(const string &data){
    Logger log;
    EVP_MD_CTX* newoffset = EVP_MD_CTX_new();
    if (!newoffset or newoffset == NULL) {
        EVP_MD_CTX_free(newoffset);
        log.err("Fail to create hash context!");
        return "";
    }

    if(EVP_DigestInit_ex(newoffset, EVP_sha256(), nullptr) != 1){
        EVP_MD_CTX_free(newoffset);
        log.err("Fail to init digest!");
        return "";
    }

    int updhash = EVP_DigestUpdate(newoffset, data.data(), data.size());
    if (updhash < 1) {
        log.err("Fail to update digest!");
        EVP_MD_CTX_free(newoffset);
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    int finalhash = EVP_DigestFinal_ex(newoffset, hash, &len);
    if (finalhash < 1) {
        log.err("Hash fail!");
        EVP_MD_CTX_free(newoffset);
        return "";
    }

    string ret;
    for (int idx = 0; idx < len; ++idx) {
        char bufhash[3];
        sprintf(bufhash, "%02x", hash[idx]);
        ret.append(bufhash);
    }
    log.success("Success");
    EVP_MD_CTX_free(newoffset);
    return ret;
}

