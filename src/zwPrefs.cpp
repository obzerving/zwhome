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
#include "Options.h"
#include "Log.h"
#include "tinyxml.h"
#include <dirent.h>

using namespace std;

#define zwDebug 0

	typedef struct
	{
        string label;
        string min;
        string max;
	} ccvalues;

zwPrefs::zwPrefs()
{
	typemap["define"] = DEFINE;
	typemap["file"] = FILE;
	typemap["months"] = MONTHS;
	typemap["days"] = DAYS;
	typemap["location"] = LOCATIONS;
	typemap["wifisensor"] = WIFISENSOR;
	typemap["wifiswitch"] = WIFISWITCH;
	typemap["simulate"] = SIMULATE;
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
            if(aline[0] != '#')
            {
                Tokenizer atoken(aline);
                if(!atoken.NextToken())
                {
                    return -200; // bad syntax: no type field
                };
                string typetoken = atoken.GetToken();
                transform(typetoken.begin(), typetoken.end(), typetoken.begin(), ::tolower); // convert to lowercase
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
                    // example restURL is http://front-light-sensor
                    wifisensormap[keytoken] = valuetoken; // restURL|minvalue|maxvalue|capabilitybits
                    break;
                case WIFISWITCH:
                    // example restURL is http://bedroom.fan
                    wifiswitchmap[keytoken] = valuetoken; // restURL|capabilities
                    break;
                case SIMULATE:
                    // example simulate run 1-jan|0000|31-dec|2359 (must be in the same year)
                    simmap[keytoken] = valuetoken;
                    break;
                default:
                    return -204; // unknown type
                }
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

string zwPrefs::getdefvalue(string lbl)
{
	if(defmap.empty()) return "";
	if(defmap.find(lbl)!= defmap.end()) return defmap.find(lbl)->second;
	else return "";
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

bool ReadValueXML(TiXmlElement const* _vElement, char *vLabel)
{
	char const* str;
	int32 intVal;

	TiXmlElement const* vElement = _vElement->FirstChildElement();
	while( vElement )
	{
		str = vElement->Value();
		if( str && !strcmp( str, "Value" ) )
		{
			str = vElement->Attribute( "label");
			if( !strcmp(str, vLabel) )
			{
				if( TIXML_SUCCESS == vElement->QueryIntAttribute( "index", &intVal ) )
				{
                    		uint8 index = (uint8) intVal;
                    		if(zwDebug) fprintf(zl, "Found value label %s at index %u\n", vLabel, index);
					return true;
				}
			}
		}
		vElement = vElement->NextSiblingElement();
	}
	return false;
}

bool RtnValuesXML(TiXmlElement const* _vElement, dvInfo *dv)
{
	char const* str;
	int32 intVal;

	TiXmlElement const* vElement = _vElement->FirstChildElement();
	while( vElement )
	{
		str = vElement->Value();
		if( str && !strcmp( str, "Value" ) )
		{
			str = vElement->Attribute( "label");
			if( (!strcmp(str, "Temperature") || !strcmp(str, "Luminance")) || !strcmp(str, "Relative Humidity") )
			{
                if(!strcmp(str, "Temperature")) dv->isTempSensor = true;
                if(!strcmp(str, "Luminance")) dv->isLightSensor = true;
                if(!strcmp(str, "Relative Humidity")) dv->isHumiditySensor = true;
                readInfo *rinfo = new readInfo();
                (*rinfo).lastreading = -100.0;
                int i;
                for(i=0; i<10; i++) (*rinfo).points[i] = -100.0;
                if( TIXML_SUCCESS == vElement->QueryIntAttribute( "index", &intVal ) )
                {
                    (*rinfo).typeindex = (int) intVal;
                    if(zwDebug) fprintf(zl, "Found value label %s at index %d\n", str, (*rinfo).typeindex);
                }
                if( TIXML_SUCCESS == vElement->QueryIntAttribute( "min", &intVal ) )
                {
                    (*rinfo).min = (int) intVal;
                }
                if( TIXML_SUCCESS == vElement->QueryIntAttribute( "max", &intVal ) )
                {
                    (*rinfo).max = (int) intVal;
                    if(intVal == 0)
                    { // If the device didn't provide a rational max value, use the defaults for an Aeotec multisensor
                        if(!strcmp(str, "Temperature"))
                        {
                            (*rinfo).max = 122;
                            (*rinfo).min = 14;
                        }
                        if(!strcmp(str, "Luminance"))
                        {
                            (*rinfo).max = 1000;
                            (*rinfo).min = 0;
                        }
                        if(!strcmp(str, "Relative Humidity"))
                        {
                            (*rinfo).max = 100;
                            (*rinfo).min = 0;
                        }
                    }
                }
                dv->reading.push_back(*rinfo);
			}
		}
		vElement = vElement->NextSiblingElement();
	}
	return true;
}

void ReadCommandClassesXML(TiXmlElement const* _ccsElement, dvInfo *dv)
{
	char const* str;
	int32 intVal;
	uint8 mapping = 0;
    ccvalues values;

	TiXmlElement const* ccElement = _ccsElement->FirstChildElement();
	while( ccElement )
	{
		str = ccElement->Value();
		if( str && !strcmp( str, "CommandClass" ) )
		{
			if( TIXML_SUCCESS == ccElement->QueryIntAttribute( "id", &intVal ) )
			{
				uint8 id = (uint8)intVal;
				if(id == 32) {
					if( TIXML_SUCCESS == ccElement->QueryIntAttribute( "mapping", &intVal ) ){
						mapping = (uint8) intVal;
					}
				}
				else if(id == 49) { // This is a sensor.
                    if(RtnValuesXML(ccElement, dv))
                        if(zwDebug) fprintf(zl, "Processed Command Class %d\n", id);
				}
				if(id == mapping) {
					str = ccElement->Attribute("name");
					// fprintf(zl, "Basic class is mapped to %s\n",str);
					switch(mapping){
						case 37:
							if(ReadValueXML(ccElement, (char *) "Switch"))
							{
								dv->bccmap = mapping;
                                readInfo *rinfo = new readInfo();
                                (*rinfo).typeindex = 0;
                                (*rinfo).lastreading = -100.0;
                                dv->reading.push_back(*rinfo);
							}
							break;
						case 38:
							ReadValueXML(ccElement, (char *) "Level");
							{
								dv->bccmap = mapping;
								dv->canDim = true;
                                readInfo *rinfo = new readInfo();
                                (*rinfo).typeindex = 0;
                                (*rinfo).lastreading = -100.0;
                                dv->reading.push_back(*rinfo);
							}
							break;
						case 48:
                            // For a multilevel sensor, we really want command class 49, so change it here
							ReadValueXML(ccElement, (char *) "Sensor");
							{
								dv->bccmap = 49;
							}
							break;
						default:
							break;
					}
				}
			}
		}

		ccElement = ccElement->NextSiblingElement();
	}
}

void ReadXML(TiXmlElement const* _node, dvInfo *dv)
{
	char const* str;
	int intVal;
	uint8 m_version;
	bool m_listening, m_frequentListening, m_beaming, m_routing, dum;
	bool m_security, m_secured, m_nodeInfoSupported, m_refreshonNodeInfoFrame;
	uint32 m_maxBaudRate;

	str = _node->Attribute( "query_stage" );
	if( str ){}
	str = _node->Attribute( "name" );
	if( str )
	{
		if(zwDebug)fprintf(zl,"Name = %s\n", str);
		dv->name.append(str);
	}
	else
	{
		dv->name.append("noname");
	}
	str = _node->Attribute( "location" );
	if( str )
	{
		if(zwDebug)fprintf(zl,"Location = %s\n", str);
		dv->location.append(str);
	}
	else
	{
		dv->location.append("nolocation");
	}
	if( TIXML_SUCCESS == _node->QueryIntAttribute( "basic", &intVal ) )
	{
        if((intVal ==1) || (intVal ==2)) dv->isController = true;
	}
	if( TIXML_SUCCESS == _node->QueryIntAttribute( "generic", &intVal ) )
	{
	}
	if( TIXML_SUCCESS == _node->QueryIntAttribute( "specific", &intVal ) )
	{
	}
	if( TIXML_SUCCESS == _node->QueryIntAttribute( "roletype", &intVal ) )
	{
	}
	if( TIXML_SUCCESS == _node->QueryIntAttribute( "devicetype", &intVal ) )
	{
	}
	if (TIXML_SUCCESS == _node->QueryIntAttribute ( "nodetype", &intVal ) )
	{
	}
	str = _node->Attribute( "type" );
	if( str )
	{
		if(zwDebug)fprintf(zl,"Type = %s\n", str);
	}
	m_listening = true;
	str = _node->Attribute( "listening" );
	if( str )
	{
        if(m_listening) dum = true;
	}
	m_frequentListening = false;
	str = _node->Attribute( "frequentListening" );
	if( str )
	{
        if(m_frequentListening) dum = false;
	}
	m_beaming = false;
	str = _node->Attribute( "beaming" );
	if( str )
	{
        if(m_beaming) dum = false;
	}
	m_routing = true;
	str = _node->Attribute( "routing" );
	if( str )
	{
        if(m_routing) dum = true;
	}
	m_maxBaudRate = 0;
	if( TIXML_SUCCESS == _node->QueryIntAttribute( "max_baud_rate", &intVal ) )
	{
        if(m_maxBaudRate) dum=true;
	}
	m_version = 0;
	if( TIXML_SUCCESS == _node->QueryIntAttribute( "version", &intVal ) )
	{
        if(m_version) dum=true;
	}
	m_security = false;
	str = _node->Attribute( "security" );
	if( str )
	{
        if(m_security) dum = false;
	}
	m_secured = false;
	str = _node->Attribute( "secured" );
	if( str )
	{
        if(m_secured) dum = false;
	}
	m_nodeInfoSupported = true;
	str = _node->Attribute( "nodeinfosupported" );
	if( str )
	{
        if(m_nodeInfoSupported) dum = true;
	}
	m_refreshonNodeInfoFrame = true;
	str = _node->Attribute( "refreshonnodeinfoframe" );
	if ( str )
	{
		m_refreshonNodeInfoFrame = !strcmp (str, "true" );
		if(m_refreshonNodeInfoFrame) dum = true;
    }
	// Read the manufacturer info and create the command classes
	TiXmlElement const* child = _node->FirstChildElement();
	while( child )
	{
		str = child->Value();
		if( str )
		{
			if( !strcmp( str, "CommandClasses" ) )
			{
				ReadCommandClassesXML( child, dv );
			}
			else if( !strcmp( str, "Manufacturer" ) )
			{
				str = child->Attribute( "id" );
				if( str )
				{
				}
				str = child->Attribute( "name" );
				if( str )
				{
				}
				TiXmlElement const* product = child->FirstChildElement();
				if( !strcmp( product->Value(), "Product" ) )
				{
					str = product->Attribute( "type" );
					if( str )
					{
					}
					str = product->Attribute( "id" );
					if( str )
					{
					}
					str = product->Attribute( "name" );
					if( str )
					{
						if(zwDebug)fprintf(zl,"Product Name = %s\n", str);
					}
				}
			}
		}
		// Move to the next child node
		child = child->NextSiblingElement();
	}
}

bool getZWcfgFileName(string *zname)
{
	DIR *dir;

	dir = opendir(zname->c_str());
	if(dir != NULL)
	{
		struct dirent *ent;
		while((ent = readdir(dir)) != NULL)
		{
			if(!strncmp(ent->d_name, "ozwcache_", 6))
			{
				if(strstr(ent->d_name, ".xml"))
				{
					*zname = *zname + string("/") + string(ent->d_name);
					closedir(dir);
					return true;
				}
			}
		}
		closedir(dir);
	}
	return false;
}

bool zwPrefs::readZWcfg(list<dvInfo> *dvl, list<dvInfo> *srl)
{
	int32 intVal, m_pollInterval, m_bIntervalBetweenPolls;
	uint32 c_configVersion = 3;
	uint8 m_Controller_nodeId = 1;
	uint8 m_initCaps, m_controllerCaps;
	bool dum;

	string filename =  string(".");
    if(!getZWcfgFileName(&filename))
    {
        fprintf(zl, "Could not find zwcfg file in %s\n",filename.c_str());
        return false;
    }
	if(zwDebug) fprintf(zl, "Opening file %s\n",filename.c_str());

	TiXmlDocument doc;
	if( !doc.LoadFile( filename.c_str(), TIXML_ENCODING_UTF8 ) )
	{
        printf("Can not open %s\n",filename.c_str()); fflush(stdout);
		return false;
	}
	TiXmlElement const* driverElement = doc.RootElement();
	// Version
	if( TIXML_SUCCESS != driverElement->QueryIntAttribute( "version", &intVal ) || (uint32)intVal != c_configVersion )
	{
        printf("Can not get version\n"); fflush(stdout);
		Log::Write( LogLevel_Warning, "WARNING: Driver::ReadConfig - %s is from an older version of OpenZWave and cannot be loaded.", filename.c_str() );
		return false;
	}
	// Home ID
	char const* homeIdStr = driverElement->Attribute( "home_id" );
	if( homeIdStr )
	{
		char* p;
		uint32 homeId = (uint32)strtoul( homeIdStr, &p, 0 );
		if(zwDebug) fprintf(zl,"Home ID found: %X\n", homeId);
	}
	else
	{
		Log::Write( LogLevel_Warning, "WARNING: Driver::ReadConfig - Home ID is missing from file %s", filename.c_str() );
		return false;
	}
	// Node ID
	if( TIXML_SUCCESS == driverElement->QueryIntAttribute( "node_id", &intVal ) )
	{
		if(zwDebug)fprintf(zl, "Controller node number is %u\n", intVal);
		if( (uint8)intVal != m_Controller_nodeId )
		{
			Log::Write( LogLevel_Warning, "WARNING: Driver::ReadConfig - Controller Node ID in file %s is incorrect", filename.c_str() );
			return false;
		}
	}
	else
	{
		Log::Write( LogLevel_Warning, "WARNING: Driver::ReadConfig - Node ID is missing from file %s", filename.c_str() );
		return false;
	}
	// Capabilities
	if( TIXML_SUCCESS == driverElement->QueryIntAttribute( "api_capabilities", &intVal ) )
	{
		m_initCaps = (uint8)intVal;
		if(m_initCaps) dum=true;
	}
	if( TIXML_SUCCESS == driverElement->QueryIntAttribute( "controller_capabilities", &intVal ) )
	{
		m_controllerCaps = (uint8)intVal;
		if(m_controllerCaps) dum=true;
	}
	// Poll Interval
	if( TIXML_SUCCESS == driverElement->QueryIntAttribute( "poll_interval", &intVal ) )
	{
		m_pollInterval = intVal;
		if(m_pollInterval) dum = true;
	}
	// Poll Interval--between polls or period for polling the entire pollList?
	char const* cstr = driverElement->Attribute( "poll_interval_between" );
	if( cstr )
	{
		m_bIntervalBetweenPolls = !strcmp( cstr, "true" );
		if(m_bIntervalBetweenPolls) dum = true;
	}
	// Read the nodes
	TiXmlElement const* nodeElement = driverElement->FirstChildElement();
	while( nodeElement )
	{
		char const* str = nodeElement->Value();
        dvInfo *dv = new dvInfo();
        dv->isController = false;
		if( str && !strcmp( str, "Node" ) )
		{
			// Get the node Id from the XML
			if( TIXML_SUCCESS == nodeElement->QueryIntAttribute( "id", &intVal ) )
			{
				if(zwDebug)fprintf(zl, "Processing Node number %u\n", intVal);
				dv->nodeId = (uint8)intVal;

				// Read the rest of the node configuration from the XML
				ReadXML( nodeElement, dv );
			}
		}
		if(!dv->name.empty() && !dv->location.empty())
		{
			dv->nodename.append(dv->location);
			dv->nodename.append(".");
			dv->nodename.append(dv->name);
		}
		dv->restURL.clear(); // zwave devices have an empty restURL
		dv->enforce = false;
		dv->desired_state = 0;
		dv->desired_state_timestamp = 0;
        if((dv->isHumiditySensor || dv->isLightSensor) || (dv->isMotionSensor || dv->isTempSensor)) srl->push_back(*dv);
		else dvl->push_back(*dv);
		nodeElement = nodeElement->NextSiblingElement();
	}
	return true;
}

string zwPrefs::getsim()
{
    string result;
    if(simmap.empty()) printf("Simmap is empty\n");
	if(simmap.empty()) return result;
	return simmap.find("run")->second;
};

int zwPrefs::getwifisensorlist(list<dvInfo> *dvl)
{
// NOTE: At this time, a wifi sensor can't be a multisensor
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
		dv->restURL = slist[0]; // The URI for the sensor;
		int caps = atoi(slist[3].c_str());
		dv->reportsBattery = (caps & 0x10)?true:false;
        readInfo *ri = new readInfo();
// TODO: fix this code to allow wifi sensors to be multisensors
		if((caps & 0x08))
		{
            dv->isLightSensor = true;
            (*ri).typeindex = 3;
		}
		if((caps & 0x04))
		{
            dv->isTempSensor = true;
            (*ri).typeindex = 1;
		}
		if((caps & 0x02))
		{
            dv->isHumiditySensor = true;
            (*ri).typeindex = 5;
		}
		if((caps & 0x01))
		{
            dv->isMotionSensor = true;
            (*ri).typeindex = 0;
		}
        (*ri).lastreading = -100.0; // This indicates that the device hasn't reported yet
        for(int i=0; i<10; i++) (*ri).points[i] = -100.0;
        (*ri).min = atoi(slist[1].c_str());
        (*ri).max = atoi(slist[2].c_str());
        dv->reading.push_back(*ri);
		dv->in_use = 0;
		dv->isController = false;
		(*dvl).push_back(*dv);
	}
	return (*dvl).size();
}

int zwPrefs::getwifiswitchlist(list<dvInfo> *dvl)
{
	for(map<string, string>::iterator itm=wifiswitchmap.begin(); itm != wifiswitchmap.end(); itm++)
	{
		dvInfo *dv = new dvInfo();
		vector<string> slist;
		if(makeList(itm->second, &slist) < 0)
		{ // error
			return -194; // malformed device spec
		}
		dv->nodename = itm->first;
		dv->nodeId = 0; // wifi switches don’t have node IDs
		dv->restURL = slist[0]; // The URI for the switch;
		int caps = atoi(slist[1].c_str());
		if((caps & 0x080))
		{
            dv->isFan = true;
            readInfo *ri = new readInfo();
            (*ri).typeindex = 0;
            (*ri).lastreading = -100.0;
            dv->reading.push_back(*ri);
		}
		if((caps & 0x040))
		{
            dv->isLight = true;
            readInfo *ri = new readInfo();
            (*ri).typeindex = 0;
            (*ri).lastreading = -100.0;
            dv->reading.push_back(*ri);
		}
		if((caps & 0x020))
		{
            dv->canDim = true;
            readInfo *ri = new readInfo();
            (*ri).typeindex = 0;
            (*ri).lastreading = -100.0;
            dv->reading.push_back(*ri);
		}
		dv->reportsBattery = (caps & 0x10)?true:false;
		dv->enforce = false;
		dv->isController = false;
		dv->desired_state = 0;
		dv->desired_state_timestamp = 0;
		dv->in_use = 0;
		(*dvl).push_back(*dv);
	}
	return (*dvl).size();
}
