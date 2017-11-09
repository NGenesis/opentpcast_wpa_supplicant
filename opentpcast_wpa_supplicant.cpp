// opentpcast_wpa_supplicant
// Description: Generate a wpa_supplicant compatible list of TPCast network profiles.
// Author: Genesis (https://github.com/NGenesis/)

// Parsing format reference:
// MAC: XX:XX:XX:XX:XX:XX
// SSID: tpcastXXXX
// Passphrase: XXXXXXXX

#include <string>
#include <tuple>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

auto getStateGPIO(int pin) {
	auto pinstr = std::to_string(pin);
	system(("sudo echo " + pinstr + " > /sys/class/gpio/export").c_str());
	system(("sudo echo in > /sys/class/gpio/gpio" + pinstr + "/direction").c_str());

	std::ifstream gpio("/sys/class/gpio/gpio" + pinstr + "/value");
	if(!gpio.is_open()) return 0;

	std::string value;
	gpio >> value;
	gpio.close();

	system(("sudo echo " + pinstr + " > /sys/class/gpio/unexport").c_str());

	return std::stoi(value);
}

int main() {
	// Determine network profile priority using GPIO pin configuration
	std::string priority;
	auto gpio3 = getStateGPIO(3), gpio11 = getStateGPIO(11), gpio17 = getStateGPIO(17), gpio27 = getStateGPIO(27);
	//if(gpio3 == 1 && gpio11 == 0 && gpio17 == 0 && gpio27 == 0) priority = "tpcastXXXX"; // MAC-unique (CE)
	if(gpio3 == 0 && gpio11 == 0 && gpio17 == 0 && gpio27 == 0) priority = "TPCast_AP1"; // Factory GPIO3 (& Legacy Factory GPIO3, previously used for TPCast_AP)
	else if(gpio3 == 1 && gpio11 == 1 && gpio17 == 0 && gpio27 == 0) priority = "TPCast_AP2"; // Factory GPIO11
	else if(gpio3 == 1 && gpio11 == 0 && gpio17 == 1 && gpio27 == 0) priority = "TPCast_AP3"; // Factory GPIO17
	else if(gpio3 == 1 && gpio11 == 0 && gpio17 == 0 && gpio27 == 1) priority = "TPCast_AP4"; // Factory GPIO27

	std::vector<std::tuple<std::string, std::string, int>> networks;

	// Factory network profiles
	for(const auto &ssid : { "TPCast_AP1", "TPCast_AP2", "TPCast_AP3", "TPCast_AP4" }) networks.push_back({ ssid, "12345678", (priority == ssid ? 7 : 0) });

	// Legacy factory network profile (PRE)
	networks.push_back({ "TPCast_AP", "12345678", (priority == "TPCast_AP1" ? 5 : 3) });

	// MAC-unique network profile (CE)
	std::ifstream wlan0address("/sys/class/net/wlan0/address");
	if(wlan0address.is_open()) {
		// Parse MAC address
		std::vector<std::string> macparts;
		for(std::string token; std::getline(wlan0address, token, ':');) {
			token.erase(std::remove_if(token.begin(), token.end(), std::not1(std::ptr_fun(::isxdigit))), token.end());
			std::transform(token.begin(), token.end(), token.begin(), ::tolower);
			macparts.push_back(token);
		}

		if(macparts.size() == 6) {
			// Generate passphrase
			std::string passphrase;
			auto key = (std::stoi(macparts[3], nullptr, 16) << 16) | (std::stoi(macparts[4], nullptr, 16) << 8) | std::stoi(macparts[5], nullptr, 16);
			for(auto i = 0; i < 8; ++i, key /= 8) passphrase.insert(0, std::to_string(key % 8));

			networks.push_back({ "tpcast" + macparts[4] + macparts[5], passphrase, (priority.empty() ? 5 : 0) });
		}
	}

	// User-defined network profile
	std::ifstream manualwlan0("/boot/opentpcast.txt");
	if(manualwlan0.is_open()) {
		std::string ssid, passphrase;
		for(std::string line; std::getline(manualwlan0 >> std::ws, line);) {
			if(line.empty() || line[0] == '#') continue;

			std::istringstream lineparser(line);
			std::string key, value;
			if(std::getline(lineparser, key, '=') && std::getline(lineparser, value)) {
				if(key == "ssid") ssid = value;
				else if(key == "passphrase") {
					passphrase = value;
					if(!ssid.empty()) break;
				}
			}
		}

		if(!ssid.empty()) networks.push_back({ ssid, passphrase, 9 });
	}

	// Sort network profiles by priority
	std::sort(networks.begin(), networks.end(), [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

	// Generate wpa_supplicant output for network profiles
	for(const auto &network : networks) std::cout << "\nnetwork={\n\tssid=\"" << std::get<0>(network) << "\"\n\tpsk=\"" << std::get<1>(network) << "\"\n\tscan_ssid=1\n\tpriority=" << std::get<2>(network) << "\n}\n";
}
