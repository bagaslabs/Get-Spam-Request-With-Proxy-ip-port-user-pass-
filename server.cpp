#include "server.h"
#include <iostream>

void ServerCmd::generateProxy() {
    proxylist.clear();

    std::cout << "[DEBUG] Raw proxy: " << proxy << std::endl;

    size_t pos1 = proxy.find(":");
    if (pos1 == std::string::npos) {
        std::cout << "[ERROR] Tidak menemukan ':' pertama" << std::endl;
        return;
    }
    size_t pos2 = proxy.find(":", pos1 + 1);
    if (pos2 == std::string::npos) {
        std::cout << "[ERROR] Tidak menemukan ':' kedua" << std::endl;
        return;
    }
    size_t pos3 = proxy.find(":", pos2 + 1);
    if (pos3 == std::string::npos) {
        std::cout << "[ERROR] Tidak menemukan ':' ketiga" << std::endl;
        return;
    }

    std::string host = proxy.substr(0, pos1);
    std::string portStr = proxy.substr(pos1 + 1, pos2 - pos1 - 1);
    int basePort = 0;

    try {
        basePort = std::stoi(portStr);
    }
    catch (...) {
        std::cout << "[ERROR] Gagal parse port: " << portStr << std::endl;
        return;
    }

    std::string key1 = proxy.substr(pos2 + 1, pos3 - pos2 - 1);
    std::string key2 = proxy.substr(pos3 + 1);

    std::cout << "[DEBUG] host=" << host
        << " port=" << basePort
        << " key1=" << key1
        << " key2=" << key2 << std::endl;

    for (int i = 0; i < connectioncount; i++) {
        int port = basePort + i;
        std::string gen = host + ":" + std::to_string(port) + ":" + key1 + ":" + key2;
        proxylist.push_back(gen);
        std::cout << "[DEBUG] Generated proxy[" << i << "]: " << gen << std::endl;
    }

    std::cout << "[INFO] Total proxy generated: " << proxylist.size() << std::endl;
}

#include <thread>
#include <chrono>

// fungsi untuk melakukan koneksi GET
void doConnection(const string& url, const string& proxy, int howmuchconnect, int connectionalive) {
    for (int i = 0; i < howmuchconnect; i++) {
        CURL* curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // pakai proxy kalau ada
            if (!proxy.empty())
                curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());

            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cout << "[ERROR] CURL failed (" << proxy << "): "
                    << curl_easy_strerror(res) << std::endl;
            }
            else {
                std::cout << "[INFO] Success GET " << url
                    << " via proxy " << proxy << std::endl;
            }

            curl_easy_cleanup(curl);
        }

        // biar koneksi "hidup" sesuai waktu
        std::this_thread::sleep_for(std::chrono::seconds(connectionalive));
    }
}

void ServerCmd::makeconnection() {
    if (proxylist.empty()) {
        std::cout << "[WARN] Proxylist kosong\n";
        return;
    }

    running = true;
    threadStatuses.clear();
    workers.clear();

    int id = 1;
    for (const auto& proxyItem : proxylist) {  
        threadStatuses.push_back({ id, proxyItem, "WAITING", 0, {} });
        id++;
    }

    for (size_t i = 0; i < proxylist.size(); i++) {
        workers.emplace_back([this, i]() {
            auto& st = threadStatuses[i];
            std::string proxyStr = proxylist[i];

            size_t p1 = proxyStr.find(":");
            size_t p2 = proxyStr.find(":", p1 + 1);
            size_t p3 = proxyStr.find(":", p2 + 1);

            std::string hostPort = proxyStr.substr(0, p2);
            std::string userPass;
            if (p3 != std::string::npos) {
                std::string user = proxyStr.substr(p2 + 1, p3 - (p2 + 1));
                std::string pass = proxyStr.substr(p3 + 1);
                userPass = user + ":" + pass;
            }

            for (int attempt = 1; attempt <= howmuchconnect && running; attempt++) {
                st.status = "CONNECTING TO PROXY (" + std::to_string(attempt) + "/" + std::to_string(howmuchconnect) + ")";

                CURL* curl = curl_easy_init();
                if (!curl) {
                    st.status = "CURL INIT FAILED";
                    return;
                }

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_PROXY, hostPort.c_str());
                if (!userPass.empty()) {
                    curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, userPass.c_str());
                }
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

                st.startTime = std::chrono::steady_clock::now();
                CURLcode res = curl_easy_perform(curl);

                if (res != CURLE_OK) {
                    st.status = "PROXY FAILED (" + std::string(curl_easy_strerror(res)) + ")";
                    curl_easy_cleanup(curl);
                    continue; // coba lagi kalau masih ada attempt tersisa
                }

                st.status = "CONNECTED (" + std::to_string(attempt) + ")";
                for (int sec = 1; sec <= connectionalive && running; sec++) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    st.duration = sec;
                }

                st.status = "DISCONNECTED (" + std::to_string(attempt) + ")";
                curl_easy_cleanup(curl);

                // jeda sejenak sebelum konek ulang
                if (attempt < howmuchconnect) {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
            });
    }
}




void ServerCmd::stop() {
    running = false;
    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
    workers.clear();
}

