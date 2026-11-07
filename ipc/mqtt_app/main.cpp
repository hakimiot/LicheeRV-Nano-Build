#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include "log.h"
#include "mqtt_client.h"

std::atomic<bool> g_running(true);

int main()
{
	MqttClient client;

	while (client.Connect() != 0) {
		LOG_W << "MQTT connect failed, retrying...";
		usleep(1000000);
	}
	LOG_I << "MQTT connected.";

	while (g_running) {
		usleep(100000);
	}

	client.Disconnect();
	return 0;
}
