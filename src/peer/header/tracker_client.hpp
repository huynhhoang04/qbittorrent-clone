#pragma once
#include <bits/stdc++.h>
#include "protocol.hpp"

using namespace std;

bool announce(const string& tracker_url, const announce_request& req);
vector<peer_info_response> get_peers(const string& tracker_url, const string& file_id);
bool health_check(const string& tracker_url);

class TrackerClient {
    private:
        string tracker_url;
        string host;
        int port;

        void parse_url(const string& url);
        string send_http_request(const string& method, const string& path, const string& body = "");

    public:
        TrackerClient(const string& tracker_url);
        bool announce(const announce_request& req);
        vector<peer_info_response> get_peers(const string& file_id);
        bool health_check();
        string get_host() const { return host; }
        int get_port() const { return port; }
        string get_tracker_url() const { return tracker_url; }
};
