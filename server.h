#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include "curl/curl.h"

using namespace std;

class ServerCmd {
public:
	string url;
	string proxy;
	int connectionalive;
	int connectioncount;
	int howmuchconnect;
	vector<string> proxylist;

	struct ThreadStatus {
		int id;
		std::string proxy;
		std::string status;
		int duration;
		std::chrono::steady_clock::time_point startTime;
	};

	std::vector<ThreadStatus> threadStatuses;
	std::vector<std::thread> workers;
	std::atomic<bool> running{ false };


    void generateProxy();
	void makeconnection();
	void stop();
};