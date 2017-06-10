#include <iostream>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/keysym.h>
#include <X11/keysymdef.h>

#include <X11/extensions/XTest.h>

#include <unistd.h>
#include <string>
#include <cstdio>
#include <cstdlib>

#include <chrono>
#include <thread>
#include <libconfig.h++>

#include "remote.h"
#include "hack.h"
#include "logger.h"

using namespace std;
using namespace libconfig;

#define SHOW_CSGO_LINUX_TITLE true
#define CSGO_LINUX_TITLE_LOCATION "csgo-external-title"

Config cfg;

string getConfigValue(string property) {
	try {
		string name = cfg.lookup(property);
		return name;
	} catch (const SettingNotFoundException &nfex) {
		stringstream ss;
		ss << "Cannot find property: '" << property << "' in settings.cfg file";
		Logger::error(ss.str());
	}

	return NULL;
}

int main() {
	Logger::init();

	if (getuid() != 0) {
		Logger::error(string("Cannot start linux-csgo-external as ") + UNDERLINE + "NON ROOT" RESET RED " user.");
		return 0;
	}

#if SHOW_CSGO_LINUX_TITLE
	system ("cat " CSGO_LINUX_TITLE_LOCATION);
#endif

	cout << UNDERLINE "\n                                                   " RESET << endl;
	cout << UNDERLINE "---------------[linux-csgo-external]---------------\n" RESET << endl;
	cout << BOLD RED << "█ AUTHOR: " UNDERLINE WHITE "\ts0beit" RESET << endl;
	cout << BOLD YELLOW << "█ MAINTAINER:" UNDERLINE WHITE "\tMcSwaggens" RESET << endl;
	cout << BOLD GREEN << "█ MAINTAINER:" UNDERLINE WHITE "\towerosu" RESET << endl;
	cout << BOLD CYAN << "█ PROGRAMMER:" UNDERLINE WHITE "\tluk1337" RESET << endl;
	cout << BOLD BLUE << "█ HELP: " UNDERLINE WHITE "\tCommunity" RESET << endl;
	cout << BOLD MAGENTA << "█ Github:" UNDERLINE WHITE "\thttps://github.com/McSwaggens/linux-csgo-external" RESET << endl;
	cout << UNDERLINE "                                                   " RESET << endl;
	cout << UNDERLINE "---------------[linux-csgo-external]---------------\n" RESET << endl;

	Display* display = XOpenDisplay(0);

	try {
		cfg.readFile("settings.cfg");
	} catch (const FileIOException &fioex) {
		Logger::error("I/O error while reading settings.cfg.");
	} catch (const ParseException &pex) {
		stringstream ss;
		ss << "Parse error at " << pex.getFile() << ":" << pex.getLine() << " - " << pex.getError();
		Logger::error(ss.str());
	}

	int keycodeGlow =  XKeysymToKeycode(display, XStringToKeysym(getConfigValue("keycodeGlow").c_str()));


	double enemyRed = ::atof(getConfigValue("enemyRed").c_str());
	double enemyGreen = ::atof(getConfigValue("enemyGreen").c_str());
	double enemyBlue = ::atof(getConfigValue("enemyBlue").c_str());
	double enemyAlpha = ::atof(getConfigValue("enemyAlpha").c_str());

	double enemyInCrosshairRed = ::atof(getConfigValue("enemyInCrosshairRed").c_str());
	double enemyInCrosshairGreen = ::atof(getConfigValue("enemyInCrosshairGreen").c_str());
	double enemyInCrosshairBlue = ::atof(getConfigValue("enemyInCrosshairBlue").c_str());
	double enemyInCrosshairAlpha = ::atof(getConfigValue("enemyInCrosshairAlpha").c_str());

	double allyRed = ::atof(getConfigValue("allyRed").c_str());
	double allyGreen = ::atof(getConfigValue("allyGreen").c_str());
	double allyBlue = ::atof(getConfigValue("allyBlue").c_str());
	double allyAlpha = ::atof(getConfigValue("allyAlpha").c_str());

	double colors[12] = {
		enemyRed, enemyGreen, enemyBlue, enemyAlpha,
		enemyInCrosshairRed, enemyInCrosshairGreen, enemyInCrosshairBlue, enemyInCrosshairAlpha,
		allyRed, allyGreen, allyBlue, allyAlpha
	};

	remote::Handle csgo;

	while (true) {
		if (remote::FindProcessByName("csgo_linux64", &csgo))
			break;

		usleep(500);
	}

	stringstream ss;
	ss << "\t  CSGO Process ID:\t [";
	ss << csgo.GetPid ();
	ss << "]";

	Logger::normal (ss.str());

	remote::MapModuleMemoryRegion client;

	client.start = 0;

	while (client.start == 0) {
		if (!csgo.IsRunning()) {
			Logger::error("The game was closed before I could find the client library inside of csgo");
			return 0;
		}

		csgo.ParseMaps();

		for (auto region : csgo.regions) {
			if (region.filename.compare("client_client.so") == 0 && region.executable) {
				client = region;
				break;
			}
		}

		usleep(500);
	}

	client.client_start = client.start;


	Logger::address ("client_client.so:\t", client.start);

	void* foundGlowPointerCall = client.find(csgo,
		"\xE8\x00\x00\x00\x00\x48\x8B\x3D\x00\x00\x00\x00\xBE", // 2017-10-06
		"x????xxx????x");

	unsigned long call = csgo.GetCallAddress(foundGlowPointerCall);

	Logger::address ("Glow function sig",(unsigned long)foundGlowPointerCall);
	Logger::address ("Glow function:\t", call);

	csgo.m_addressOfGlowPointer = csgo.GetCallAddress((void*)(call+0xF));
	Logger::address ("Glow array pointer:\t", csgo.m_addressOfGlowPointer);

	unsigned long foundLocalPlayerLea = (long)client.find(csgo,
		"\x48\x89\xe5\x74\x0e\x48\x8d\x05\x00\x00\x00\x00", //27/06/16
		"xxxxxxxx????");

	csgo.m_addressOfLocalPlayer = csgo.GetCallAddress((void*)(foundLocalPlayerLea+0x7));

	unsigned long foundAttackMov = (long)client.find(csgo,
		"\x44\x89\xe8\x83\xe0\x01\xf7\xd8\x83\xe8\x03\x45\x84\xe4\x74\x00\x21\xd0", //10/07/16
		"xxxxxxxxxxxxxxx?xx");
	csgo.m_addressOfForceAttack = csgo.GetCallAddress((void*)(foundAttackMov+19));

	unsigned long foundAlt1Mov = (long)client.find(csgo,
		"\x44\x89\xe8\xc1\xe0\x11\xc1\xf8\x1f\x83\xe8\x03\x45\x84\xe4\x74\x00\x21\xd0", //10/07/16
		"xxxxxxxxxxxxxxxx?xx");

	csgo.m_addressOfAlt1 = csgo.GetCallAddress((void*)(foundAlt1Mov+20));

	Logger::address ("ForceAttack address:\t", csgo.m_addressOfForceAttack);
	Logger::address ("Atl1 address:\t", csgo.m_addressOfAlt1);
	Logger::address ("LocalPlayer address:\t", csgo.m_addressOfLocalPlayer);


	csgo.m_bShouldGlow = true;

	char keys[32];
	char lastkeys[32];

	while (csgo.IsRunning()) {
		XQueryKeymap(display, keys);

		for (unsigned i = 0; i < sizeof(keys); ++i) {
			if (keys[i] != lastkeys[i]) {
				// check which key got changed
				for (unsigned j = 0, test = 1; j < 8; ++j, test *= 2) {
					// if the key was pressed, and it wasn't before, print this
					if ((keys[i] & test) &&
							((keys[i] & test) != (lastkeys[i] & test))) {
						const int code = i * 8 + j;

						if (code == keycodeGlow) {
							csgo.m_bShouldGlow = !csgo.m_bShouldGlow;
							Logger::toggle("ESP\t\t", csgo.m_bShouldGlow);
						}   
					}
				}
			}

			lastkeys[i] = keys[i];
		}

		try {
			hack::Glow(colors, &csgo, &client);
		} catch (int exception) {
			Logger::error("An error occurred, closing...");
			break;
		}
	}

	return 0;
}
