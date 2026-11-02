#include "tracker_client.hpp"
#include "logger.hpp"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

bool announce(const string& tracker_url, const announce_request& req) {
    TrackerClient client(tracker_url);
    return client.announce(req);
}

vector<peer_info_response> get_peers(const string& tracker_url, const string& file_id) {
    TrackerClient client(tracker_url);
    return client.get_peers(file_id);
}

bool health_check(const string& tracker_url) {
    TrackerClient client(tracker_url);
    return client.health_check();
}

TrackerClient::TrackerClient(const string& tracker_url) : tracker_url(tracker_url), port(8080) {
    parse_url(tracker_url);
}

void TrackerClient::parse_url(const string& url) {
    string temp = url;
    if (temp.find("http://") == 0) {
        temp = temp.substr(7);
    } else if (temp.find("https://") == 0) {
        temp = temp.substr(8);
    }

    size_t colon_pos = temp.find(':');
    size_t slash_pos = temp.find('/');

    if (colon_pos != string::npos) {
        host = temp.substr(0, colon_pos);
        if (slash_pos != string::npos) {
            port = stoi(temp.substr(colon_pos + 1, slash_pos - colon_pos - 1));
        } else {
            port = stoi(temp.substr(colon_pos + 1));
        }
    } else {
        if (slash_pos != string::npos) {
            host = temp.substr(0, slash_pos);
        } else {
            host = temp;
        }
        port = 80;
    }
}

string TrackerClient::send_http_request(const string& method, const string& path, const string& body) {
    Logger log;
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    string port_str = to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        log.err("Cannot resolve host: " + host);
        return "";
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        log.err("Cannot create socket");
        freeaddrinfo(res);
        return "";
    }

    // Thiết lập timeout 3 giây để tránh bị treo
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        log.err("Cannot connect to Tracker at " + host + ":" + port_str);
        close(sock);
        freeaddrinfo(res);
        return "";
    }
    freeaddrinfo(res);

    // Xây dựng HTTP Request
    stringstream request_ss;
    request_ss << method << " " << path << " HTTP/1.1\r\n";
    request_ss << "Host: " << host << ":" << port << "\r\n";
    request_ss << "User-Agent: BitTorrentClone/1.0\r\n";
    request_ss << "Connection: close\r\n";
    if (!body.empty()) {
        request_ss << "Content-Type: application/json\r\n";
        request_ss << "Content-Length: " << body.length() << "\r\n";
    }
    request_ss << "\r\n";
    if (!body.empty()) {
        request_ss << body;
    }

    string req_data = request_ss.str();
    if (send(sock, req_data.c_str(), req_data.length(), 0) < 0) {
        log.err("Failed to send request to Tracker");
        close(sock);
        return "";
    }

    // Nhận response
    string response_str;
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        response_str.append(buffer, bytes_read);
    }
    close(sock);

    // Kiểm tra status code trong header (VD: HTTP/1.1 200 OK)
    size_t line_end = response_str.find("\r\n");
    if (line_end == string::npos) {
        return "";
    }
    string status_line = response_str.substr(0, line_end);
    if (status_line.find(" 200 ") == string::npos && status_line.find(" 200\r") == string::npos && status_line.find(" 200") == string::npos) {
        log.err("HTTP request failed with status: " + status_line);
        return "";
    }

    // Tách body sau chuỗi \r\n\r\n
    size_t body_pos = response_str.find("\r\n\r\n");
    if (body_pos != string::npos) {
        return response_str.substr(body_pos + 4);
    }
    return "";
}

bool TrackerClient::announce(const announce_request& req) {
    Logger log;
    try {
        json j = req;
        string body = j.dump();
        string res = send_http_request("POST", "/announce", body);
        if (res.empty()) {
            return false;
        }

        json res_json = json::parse(res);
        if (res_json.contains("status") && res_json["status"] == "ok") {
            log.success("Announce to Tracker succeeded");
            return true;
        }
        return false;
    } catch (const exception& e) {
        log.err("Announce error: " + string(e.what()));
        return false;
    }
}

vector<peer_info_response> TrackerClient::get_peers(const string& file_id) {
    Logger log;
    vector<peer_info_response> peers;
    try {
        string path = "/peers?file_id=" + file_id;
        string res = send_http_request("GET", path);
        if (res.empty()) {
            return peers;
        }

        json res_json = json::parse(res);
        if (res_json.is_array()) {
            for (const auto& item : res_json) {
                peers.push_back(item.get<peer_info_response>());
            }
        }
        log.info("Received " + to_string(peers.size()) + " peers from Tracker");
        return peers;
    } catch (const exception& e) {
        log.err("Get peers error: " + string(e.what()));
        return peers;
    }
}

bool TrackerClient::health_check() {
    Logger log;
    try {
        string res = send_http_request("GET", "/health");
        if (res.empty()) {
            return false;
        }

        json res_json = json::parse(res);
        if (res_json.contains("status") && res_json["status"] == "ok") {
            log.success("Tracker is healthy");
            return true;
        }
        return false;
    } catch (const exception& e) {
        log.err("Health check error: " + string(e.what()));
        return false;
    }
}
