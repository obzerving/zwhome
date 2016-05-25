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
	bool chkptfile_exists(); // switch returns true= chkptfile exists or false= no chkptfile
	string getrulefilename();
	string getchkptfilename();
	string getdefvalue(string lbl);
	string getmonths(string monthtype);
	string getdays(string daynames);
	string getloc(string locname);
	int getsensornode(string sensorname, dvInfo *dv);
	int getdevlist(list<dvInfo> *dvl);
	int getsensorlist(list<dvInfo> *srl);
	int getwifisensorlist(list<dvInfo> *dvl);
	zwPrefs();
	~zwPrefs();
	enum TypeField {SWITCH, SENSOR, DEFINE, FILE, MONTHS, DAYS, LOCATIONS, WIFISENSOR};
private:
	map<string, TypeField> typemap;
	map<string, string> defmap;
	// label everyday sunday|monday|tuesday|wednesday|thursday|friday|saturday
	map<string, string> swmap;
	// switch chandelier (nodeId|valueid.Genre|valueid.CommandClassId|valueid.Instance|valueid.Index|valueid.Type|{canDim||isLight}
	map<string, string> sensormap;
	// sensor front_light (nodeId|valueid.Genre|valueid.CommandClassId|valueid.Instance|valueid.Index|valueid.Type|{battery||motion_sensor||light_sensor||humidity_sensor||temp_sensor||switch}
	map<string, string> filemap;
	// file rules rulefile.txt
	map<string, string> monthmap;
	// months gregorian jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec
	// months hebrew tishrei|cheshvan|kislev|teves|shvat|adar|nissan|iyar|sivan|tamuz|av|elul|adar1|adar2
	map<string, string> daymap;
	// days gregorian sunday|monday|tuesday|wednesday|thursday|friday|saturday
	map<string, string> locmap; // latitude|longitude of home location
	map<string, string> wifisensormap;
	// wifisensor front_light http://sensor_IP:Port/sensor|{battery||motion_sensor||light_sensor||humidity_sensor||temp_sensor||switch}
	ifstream cfgfile;
	string rulefilename;
	string chkptfilename;
};

#endif /* ZWPREFS_H_ */
