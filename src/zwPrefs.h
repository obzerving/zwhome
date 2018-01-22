/*
 * zwPrefs.h
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#ifndef ZWPREFS_H_
#define ZWPREFS_H_

#include <fstream>
#include <map>
#include <string>
#include "zwDevices.h"

using namespace std;

class zwPrefs
{
public:
	int ingest(string configfile); //parses config file and stores results. Returns 0=okay or error code
	string getrulefilename();
	string getdefvalue(string lbl);
	string getmonths(string monthtype);
	string getdays(string daynames);
	string getloc(string locname);
	string getsim();
    bool readZWcfg(list<dvInfo> *dvl, list<dvInfo> *srl);
    int getwifisensorlist(list<dvInfo> *dvl);
    int getwifiswitchlist(list<dvInfo> *dvl);
    zwPrefs();
	~zwPrefs();
	enum TypeField {DEFINE, FILE, MONTHS, DAYS, LOCATIONS, WIFISENSOR, WIFISWITCH, SIMULATE};
private:
	map<string, TypeField> typemap;
	map<string, string> defmap;
	// define everyday sunday|monday|tuesday|wednesday|thursday|friday|saturday
	map<string, string> filemap;
	// file rules rulefile.txt
	map<string, string> monthmap;
	// months gregorian jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec
	// months hebrew tishrei|cheshvan|kislev|teves|shvat|adar|nissan|iyar|sivan|tamuz|av|elul|adar1|adar2
	map<string, string> daymap;
	// days gregorian sunday|monday|tuesday|wednesday|thursday|friday|saturday
	map<string, string> locmap; // latitude|longitude of home location
	map<string, string> wifisensormap;
	// wifisensor front_light http://sensor_IP:Port/sensor|{battery||light_sensor||humidity_sensor||temp_sensor||motion_sensor}
	map<string, string> wifiswitchmap;
	// wifiswitch bedroom_fan http://sensor_IP:Port/switch|{isLight||canDim||battery||light_sensor||humidity_sensor||temp_sensor||motion_sensor}
	map<string, string> simmap;
	// simulate from 1-jan at 0000 to 31-dec at 2359
	ifstream cfgfile;
	string rulefilename;
	string chkptfilename;
};

#endif /* ZWPREFS_H_ */

