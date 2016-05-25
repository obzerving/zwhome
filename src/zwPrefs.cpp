/*
 * zwPrefs.cpp
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#include "stdafx.h"
#include <iostream>
#include <cstdio>
#include <list>
#include <algorithm>
#include "Tokenizer.h"
#include "zwhome.h"

using namespace std;

zwPrefs::zwPrefs()
{
	typemap["switch"] = SWITCH;
	typemap["sensor"] = SENSOR;
	typemap["define"] = DEFINE;
	typemap["file"] = FILE;
	typemap["months"] = MONTHS;
	typemap["days"] = DAYS;
	typemap["location"] = LOCATIONS;
	typemap["wifisensor"] = WIFISENSOR;
};

zwPrefs::~zwPrefs()
{
};

int zwPrefs::ingest(string configfile)
{

	cfgfile.open(configfile.c_str());
	string aline;
	while(cfgfile.good())
	{
		getline(cfgfile, aline);
		if(!aline.empty())
		{
			transform(aline.begin(), aline.end(), aline.begin(), ::tolower); // convert to lowercase
			Tokenizer atoken(aline);
			if(!atoken.NextToken())
			{
				return -200; // bad syntax: no type field
			};
			string typetoken = atoken.GetToken();
			if(!atoken.NextToken())
			{
				return -201; // bad syntax: no key field
			};
			string keytoken = atoken.GetToken();
			if(!atoken.NextToken())
			{
				return -202; // bad syntax: no value field
			};
			string valuetoken = atoken.GetToken();
			switch (typemap.find(typetoken)->second)
			{
			case SWITCH:
				swmap[keytoken] = valuetoken; // nodeId|valueid.Genre|valueid.CommandClassId|valueid.Instance|valueid.Index|valueid.Type
				break;
			case SENSOR:
				sensormap[keytoken] = valuetoken; // nodeId|valueid.Genre|valueid.CommandClassId|valueid.Instance|valueid.Index|valueid.Type
				break;
			case DEFINE:
				defmap[keytoken] = valuetoken;
				break;
			case FILE:
				filemap[keytoken] = valuetoken;
				break;
			case MONTHS:
				monthmap[keytoken] = valuetoken;
				break;
			case DAYS:
				daymap[keytoken] = valuetoken;
				break;
			case LOCATIONS:
				locmap[keytoken] = valuetoken;
				break;
			case WIFISENSOR:
				// example restURL is http://front-light-sensor/luminance
				wifisensormap[keytoken] = valuetoken; // restURL|capabilities
				break;
			default:
				return -204; // unknown type
			}
		}
	};
	if(getrulefilename().empty())
	{ // error by omission
		return -205; // Didn't specify a rules file
	};
	return 0;
};

string zwPrefs::getrulefilename()
{
	if(filemap.empty()) return "";
	if(filemap.find("rules")!= filemap.end()) return filemap.find("rules")->second;
	else return "";
};

string zwPrefs::getchkptfilename()
{
	if(filemap.empty()) return "";
	if(filemap.find("checkpoint")!= filemap.end()) return filemap.find("checkpoint")->second;
	else return "";
};

string zwPrefs::getdefvalue(string lbl)
{
	if(defmap.empty()) return "";
	if(defmap.find(lbl)!= defmap.end()) return defmap.find(lbl)->second;
	else return "";
};

bool zwPrefs::chkptfile_exists()
{
	if(filemap.empty()) return "";
	if(filemap.find("checkpoint")!= filemap.end()) return true;
	else return false;
};
string zwPrefs::getmonths(string monthtype)
{
	if(monthmap.find(monthtype)!= monthmap.end()) return monthmap.find(monthtype)->second;
	else return "";
};

string zwPrefs::getdays(string daynames)
{
	if(daymap.empty()) return "";
	if(daymap.find(daynames)!= daymap.end()) return daymap.find(daynames)->second;
	else return "";
};

string zwPrefs::getloc(string locname)
{
	if(locmap.empty()) return "";
	if(locmap.find(locname)!= locmap.end()) return locmap.find(locname)->second;
	else return "";
};

int zwPrefs::getdevlist(list<dvInfo> *dvl)
{
	if(swmap.empty()) return 0;
	string homeid = getdefvalue("home-id");
	uint32 uhomeID;
	if(homeid.empty())
	{ // error
		fprintf(zl, "Error: No HomeID assigned\n"); fflush(zl);
		return -191;
	}
	sscanf(homeid.c_str(), "%x", &uhomeID);
	for(map<string, string>::iterator itm=swmap.begin(); itm != swmap.end(); itm++)
	{
		dvInfo *dv = new dvInfo();
		vector<string> slist;
		if(makeList(itm->second, &slist) < 0)
		{ // error
			fprintf(zl,"Error: malformed device spec\n"); fflush(zl);
			return -192; // malformed device spec
		}
		dv->nodename = itm->first;
		dv->nodeId = atoi(slist[0].c_str());
		// home_id, node_id, genre, commandclass, instance, valueindex, type, capabilities
		dv->vId.push_back(ValueID(uhomeID, (uint8)dv->nodeId, (ValueID::ValueGenre)atoi(slist[1].c_str()), (uint8)atoi(slist[2].c_str()),
			(uint8)atoi(slist[3].c_str()), (uint8)atoi(slist[4].c_str()), (ValueID::ValueType)atoi(slist[5].c_str()) ));
		int caps = atoi(slist[6].c_str());
		dv->isLight = (caps & 0x01)?true:false; // This device is being used with a light
		dv->canDim = (caps & 0x02)?true:false; // This device can be dimmed
		dv->in_use = 0;
		(*dvl).push_back(*dv);
	}
	return (*dvl).size();
}

int zwPrefs::getsensorlist(list<dvInfo> *dvl)
{
	if(sensormap.empty()) return 0;
	string homeid = getdefvalue("home-id");
	uint32 uhomeID;
	if(homeid.empty())
	{ // error
		fprintf(zl, "Error: No HomeID assigned\n"); fflush(zl);
		return -193;
	}
	sscanf(homeid.c_str(), "%x", &uhomeID);
	for(map<string, string>::iterator itm=sensormap.begin(); itm != sensormap.end(); itm++)
	{
		dvInfo *dv = new dvInfo();
		vector<string> slist;
		if(makeList(itm->second, &slist) < 0)
		{ // error
			return -194; // malformed device spec
		}
		dv->nodename = itm->first;
		dv->nodeId = atoi(slist[0].c_str());
		// home_id, node_id, genre, commandclass, instance, valueindex, type
		dv->vId.push_back(ValueID(uhomeID, (uint8)dv->nodeId, (ValueID::ValueGenre)atoi(slist[1].c_str()), (uint8)atoi(slist[2].c_str()),
			(uint8)atoi(slist[3].c_str()), (uint8)atoi(slist[4].c_str()), (ValueID::ValueType)atoi(slist[5].c_str()) ));
		dv->in_use = 0;
		(*dvl).push_back(*dv);
	}
	return (*dvl).size();
}

int zwPrefs::getwifisensorlist(list<dvInfo> *dvl)
{
	if(wifisensormap.empty()) return 0;
	string homeid = getdefvalue("home-id");
	uint32 uhomeID;
	if(homeid.empty())
	{ // error
		fprintf(zl, "Error: No HomeID assigned\n"); fflush(zl);
		return -103;
	}
	sscanf(homeid.c_str(), "%x", &uhomeID);
	for(map<string, string>::iterator itm=wifisensormap.begin(); itm != wifisensormap.end(); itm++)
	{
		dvInfo *dv = new dvInfo();
		vector<string> slist;
		if(makeList(itm->second, &slist) < 0)
		{ // error
			return -194; // malformed device spec
		}
		dv->nodename = itm->first;
		dv->nodeId = 0; // wifi sensors don’t have node IDs
		dv->vId.push_back( ValueID(uhomeID, (uint8)dv->nodeId, (ValueID::ValueGenre)0, (uint8)0,
			(uint8)0, (uint8)0, (ValueID::ValueType)0 ));
		dv->restURL = slist[0]; // The URI for the sensor;
		int caps = atoi(slist[1].c_str());
		dv->isLightSensor = (caps & 0x08)?true:false;
		dv->lastreading = -1.0; // This indicates that the device hasn't reported yet
		dv->in_use = 0;
		(*dvl).push_back(*dv);
	}
	return (*dvl).size();
}

int zwPrefs::getsensornode(string sensorname, dvInfo *dv)
{
	if(sensormap.empty()) return 0;
	if(sensormap.find(sensorname)!= sensormap.end())
	{
		vector<string> slist;
		if(makeList(sensormap.find(sensorname)->second, &slist) < 0)
		{ // error
			return -102; // malformed sensor specification
		};
		dv->nodename = sensorname;
		dv->nodeId = atoi(slist[0].c_str());
		string homeid = getdefvalue("home-id");
		uint32 uhomeID;
		if(homeid.empty())
		{ // error
			fprintf(zl, "Error: No HomeID assigned\n"); fflush(zl);
			return -103;
		}
		sscanf(homeid.c_str(), "%x", &uhomeID);
		// home_id, node_id, genre, commandclass, instance, valueindex, type
		dv->vId.push_back(ValueID(uhomeID, (uint8)dv->nodeId, (ValueID::ValueGenre)atoi(slist[1].c_str()), (uint8)atoi(slist[2].c_str()),
			(uint8)atoi(slist[3].c_str()), (uint8)atoi(slist[4].c_str()), (ValueID::ValueType)atoi(slist[5].c_str()) ));
		return dv->nodeId;
	}
	else return 0;
};
