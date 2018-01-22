/*
 * zwDevices.cpp
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#include "stdafx.h"
#include <ctime>
#include <iostream>
#include <list>
#include <cstdlib>
#include <stdio.h>
#include <string>

#include <curl/curl.h>

struct MemoryStruct {
  char *memory;
  size_t size;
};
#include "Options.h"
#include "Log.h"
#include "zwhome.h"
using namespace OpenZWave;
using namespace std;
pthread_mutex_t zwDevices::g_criticalSection;
pthread_cond_t zwDevices::initCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t zwDevices::initMutex = PTHREAD_MUTEX_INITIALIZER;

#define zwDebug 0

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = static_cast<char*> (realloc(mem->memory, mem->size + realsize + 1));
  if(mem->memory == NULL) {
    /* out of memory! */
    fprintf(zl, "not enough memory (realloc returned NULL)\n"); fflush(zl);
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

bool zwDevices::sendWifiCmd(string wificmd, char *resp)
{
		CURL *curl_handle;
		CURLcode res;
		bool result = false;

		struct MemoryStruct chunk;

		chunk.memory = static_cast<char*> (malloc(1));  /* will be grown as needed by the realloc above */
        if(chunk.memory == NULL) {fprintf(zl, "SendWifiCmd: malloc returned NULL\n"); fflush(zl);}
		chunk.size = 0;    /* no data at this point */

		curl_global_init(CURL_GLOBAL_ALL);

		/* init the curl session */
		curl_handle = curl_easy_init();
        if(curl_handle == NULL) fprintf(zl, "SendWifiCmd: curl_easy_init failed\n"); fflush(zl);

		/* specify URL to get */
		curl_easy_setopt(curl_handle, CURLOPT_URL, wificmd.c_str());

		/* send all data to this function  */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

		/* we pass our 'chunk' struct to the callback function */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

		/* some servers don't like requests that are made without a user-agent
		field, so we provide one */
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		/* get it! */
		res = curl_easy_perform(curl_handle);

		/* check for errors */
		if(res != CURLE_OK) {
			fprintf(zl, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			fflush(zl);
			result = false;
		}
		else
		{
			/*
			* Now, our chunk.memory points to a memory block that is chunk.size
			* bytes big and contains the response from the device.
			*/

			// fprintf(zl, "%lu bytes retrieved\n", (long)chunk.size);
			if(chunk.size > 0)
			{
				strncpy(resp, chunk.memory, chunk.size);
				resp[chunk.size] = '\0';
				result = true;
			}
			else result = false; // didn't work, so we'll try again on another call
		}

		/* cleanup curl stuff */
		curl_easy_cleanup(curl_handle);

		free(chunk.memory);

		/* we're done with libcurl, so clean it up */
		curl_global_cleanup();
	return result;
}

int zwDevices::init(bool executeflag)
{
	g_homeId = 0;
	g_nodesQueried = false;
	 strcpy(SensorType[0], "Switch");
	 strcpy(SensorType[1], "Temperature");
	 strcpy(SensorType[2], "General");
	 strcpy(SensorType[3], "Luminance");
	 strcpy(SensorType[4], "Power");
	 strcpy(SensorType[5], "Relative Humidity");
	 strcpy(fanSpeeds[0], "off");
	 strcpy(fanSpeeds[1], "low");
	 strcpy(fanSpeeds[2], "medium");
	 strcpy(fanSpeeds[3], "high");
	 strcpy(fanSpeeds[4], "lights");

			pthread_mutexattr_t mutexattr;
			pthread_mutexattr_init(&mutexattr);
			pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&g_criticalSection, &mutexattr);
			pthread_mutexattr_destroy(&mutexattr);
			pthread_mutex_lock(&initMutex);
			Options::Create("/home/pi/Documents/open-zwave-1.5/config/", "", "" );
			Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Detail );
			Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
			Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );
			Options::Get()->AddOptionInt( "PollInterval", 500 );
			Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
			Options::Get()->AddOptionBool("ValidateValueChanges", true);
			Options::Get()->Lock();
			if(executeflag)
			{
			    Manager::Create();
                Manager::Get()->AddWatcher( OnNotification, NULL );
                Manager::Get()->AddDriver( "/dev/ttyUSB0" );
                pthread_cond_wait(&initCond, &initMutex);
                while( !g_homeId )
                {
                    sleep(1);
                }
                while( !g_nodesQueried )
                {
                    sleep(1);
                }
                Manager::Get()->WriteConfig( g_homeId );
                sleep(1);
                Log::SetLoggingState(false); // turn off open-zwave logging
			}
			return 0;
}

void zwDevices::wrapup()
{
	pthread_mutex_destroy( &g_criticalSection );
};

int zwDevices::doAction(uint8 action, list<string> nodenames, bool enforce)
{
// Action values: zero = off; 1-99 = ON; 255 = on at previous level; 100-254 = remove enforcement
    bool boolRpt, devWant;
    uint8 byteRpt;
	for(list<string>::iterator isd=nodenames.begin(); isd!=nodenames.end(); isd++) // for each node
	{
        time_t rt;
        time(&rt);
        char etime[32];
        strncpy(etime,ctime(&rt),sizeof(etime));
        etime[strlen(etime)-1] = ' '; // Replace newline with a space
        bool foundd = false; // flag to indicate when we've found the right device entry in the dvl
        list<dvInfo>::iterator itd = zwR->dvl.begin();
        while((itd != zwR->dvl.end()) && !foundd)
        {
            if((*itd).nodename == (*isd)) foundd = true;
            else itd++;
        }
        if(foundd)
        {
            if((action > 99) && (action < 255))
            { // Just removing enforcement
                (*itd).enforce = false;
                fprintf(zl, "%sdoAction: Node = %s, Remove enforcement\n", etime, (*isd).c_str()); fflush(zl);
            }
            else
            {
                if((*itd).nodeId  != 0)
                { // It's a zwave device
                    bool foundn = false;  // flag to indicate when we've found the right entry in g_nodes
                    pthread_mutex_lock( &g_criticalSection );
                    list<NodeInfo*>::iterator it = g_nodes.begin();
                    while((it != g_nodes.end()) && !foundn)
                    {
                        NodeInfo* nodeInfo = *it;
                        if( ( nodeInfo->m_homeId == g_homeId ) && ( nodeInfo->m_nodeId == (*itd).nodeId ) )
                        { // this is the node. Find the right value
                            foundn = true;
                            list<ValueID>::iterator itv=nodeInfo->m_values.begin();
                            bool foundv = false; // flag to indicate that we've found the right value
                            while((itv != nodeInfo->m_values.end()) && !foundv)
                            {
                                if((*itv).GetCommandClassId() == (*itd).bccmap)
                                {
                                    if((*itv).GetIndex() == ((*itd).reading.front()).typeindex)
                                    { // this is the right (and only) value. See if the device is already in the right state
                                        foundv = true;
                                        Manager::Get()->RefreshValue((*itv)); // Refresh value from network first
                                        (*itd).desired_state = action;
                                        (*itd).enforce = enforce;
                                        switch((*itv).GetType())
                                        {
                                            case ValueID::ValueType_Bool:
                                                Manager::Get()->GetValueAsBool((*itv),&boolRpt); // Actual state of device
                                                devWant = (action>0)?true:false;
                                                if(boolRpt != devWant)
                                                {
                                                    Manager::Get()->SetValue((*itv), devWant);
                                                    fprintf(zl, "%sdoAction: Node = %s, Change state: desired = %s, reported = %s\n", etime, (*isd).c_str(), devWant?"on":"off", boolRpt?"on":"off");
                                                    fflush(zl);
                                                }
                                                else
                                                { // no action necessary
                                                    if(zwDebug) fprintf(zl, "%sdoAction: Node = %s, already at state = %s\n", etime, (*isd).c_str(), devWant?"on":"off");
                                                    fflush(zl);
                                                }
                                                break;
                                            case ValueID::ValueType_Byte:
                                                Manager::Get()->GetValueAsByte((*itv),&byteRpt); // Actual state of device
                                                if(byteRpt != action)
                                                {
                                                    Manager::Get()->SetValue((*itv), action);
                                                    fprintf(zl, "%sdoAction: Node = %s, Change state: desired = %u, reported = %u\n", etime, (*isd).c_str(), action, byteRpt);
                                                    fflush(zl);
                                                }
                                                else
                                                { // no action necessary
                                                    if(zwDebug) fprintf(zl, "%sdoAction: Node = %s, already at state = %u\n", etime, (*isd).c_str(), action);
                                                    fflush(zl);
                                                }
                                                break;
                                            case ValueID::ValueType_Decimal:
                                                break;
                                            case ValueID::ValueType_Int:
                                                break;
                                            case ValueID::ValueType_List:
                                                break;
                                            case ValueID::ValueType_Schedule:
                                                break;
                                            case ValueID::ValueType_Short:
                                                break;
                                            case ValueID::ValueType_String:
                                                break;
                                            case ValueID::ValueType_Button:
                                                break;
                                            case ValueID::ValueType_Raw:
                                                break;
                                        } // switch GetType
                                    } // if GetIndex
                                } // if cc == bccmap
                                itv++;
                            }
                            if(!foundv) fprintf(zl, "%sdoAction: Node = %s, Didn't find the value to perform action\n", etime, (*isd).c_str());
                        }
                        it++;
                    }; //while !foundn ...
                    pthread_mutex_unlock( &g_criticalSection );
                    if(!foundn) fprintf(zl, "%sdoAction: Node = %s, NodeID not in g_nodes\n", etime, (*isd).c_str());
                } // if nodeId ...
                else
                { // It's a wifi device
                    char dvresp[128];
                    string scmd ((*itd).restURL);
                    (*itd).desired_state = action;
                    (*itd).enforce = enforce;
                    if(((*itd).reading.front()).lastreading != float(action))
                    {
                        scmd.append("/zwh?params=");
                        if(action == 0) scmd.append("off");
                        else
                        {
                            scmd.append("on,");
                            char astr[10];
                            snprintf(astr, 9, "%u", action);
                            scmd.append(string(astr));
                        }
                        if(sendWifiCmd(scmd, dvresp))
                        { // For now, we're not checking the response.
                            fprintf(zl, "%sdoAction: Node = %s", etime, (*isd).c_str());
                            if(action == 0) fprintf(zl, ", Command sent to turn off\n");
                            else fprintf(zl, ", Command sent to turn on to level (%d)\n", action);
                            if(zwDebug)
                            {
                                fprintf(zl, "Sent: %s\n", scmd.c_str());
                                fprintf(zl, "Response: %s\n",dvresp);
                            }
                            fflush(zl);
                            ((*itd).reading.front()).lastreading = float(action);
                        }
                        else fprintf(zl, "Failed sending action to wifiswitch %s\n", (*isd).c_str());
                    }
                    else
                    {
                        if(zwDebug) fprintf(zl, "%sdoAction: Node = %s, already at state = %.1f\n", etime, (*isd).c_str(), float(action));
                        fflush(zl);
                    }
                } // if not nodeId
            } // not removing enforcement
        } // not foundd
        else
        {
            fprintf(zl,"%sdoAction: Node = %s, Node name not found in device list\n", etime, (*isd).c_str());
        }
	};
	return 0;
};

bool zwDevices::getSensorValue(dvInfo *dv, float *fvalue)
{
	char dvresp[128];
	bool result = false; // returns true only if the sensor has a new value to report
	time_t current_time;
	time(&current_time);

	if(dv->nodeId > 0)
	{ // The sensor is a z-wave device, whose value is being updated in zwhome::OnNotification::Type_ValueChanged
        *fvalue = (dv->reading.front()).lastreading;
        if(zwDebug) fprintf(zl, "\nSensor %s reports %.2f on %s\n", dv->nodename.c_str(), *fvalue, ctime(&current_time)); fflush(zl);
        result = true;
	}
	else
	{ // The sensor is a Wifi device. We update the value here before returning it.
        if(!strcmp(dv->restURL.c_str(), "client"))
        { // This wifi sensor sends periodic reports to our server
            for(list<readInfo>::iterator ri = dv->reading.begin(); ri != dv->reading.end(); ri++)
            {
                if((*ri).lastreading != -100)
                {
                        *fvalue = (*ri).lastreading;
                        result = true;
                }
            }
        }
        else
        { // This wifi sensor receives requests from us
            string sURL(dv->restURL);
            int stype = 0; // ask for the right reading
            if(dv->isTempSensor) {sURL.append("/temperature"); stype = 1;}
            if(dv->isLightSensor) {sURL.append("/luminance"); stype = 3;}
            if(dv->isHumiditySensor) {sURL.append("/relativehumidity"); stype = 5;}
            if(stype == 0)
            {
                fprintf(zl, "Sensor has no type specified\n"); fflush(zl);
            }
            dvresp[0] = '\0';
            if(zwDebug) fprintf(zl, "zwDevices->SendWifiCmd: URL = %s\n", sURL.c_str()); fflush(zl);
			if(sendWifiCmd(sURL, dvresp))
			{ // Received the reading in JSON. Extract it.
                if(zwDebug) {fprintf(zl, "zwDevices: length of dvresp = %d\n", strlen(dvresp)); fflush(zl);}
                if(zwDebug) {fprintf(zl, "zwDevices: dvresp = %s\n", dvresp); fflush(zl);}
				char *sensorv;
				sensorv = strtok(dvresp,"{\":}, ");
				sensorv = strtok(NULL,":, ");
				if(sensorv != NULL)
				{
                    if(zwDebug){ fprintf(zl, "zwDevices: sensorv = %s\n", sensorv); fflush(zl);}
					*fvalue = atof(sensorv); // Convert it to decimal
					if(zwDebug) fprintf(zl, "\nSensor %s reports %.2f on %s\n", dv->nodename.c_str(), *fvalue, ctime(&current_time)); fflush(zl);
					bool foundri = false;
					list<readInfo>:: iterator ri=dv->reading.begin(); // Put the reading in the right place
					while((ri != dv->reading.end()) && !foundri)
					{
                        if((*ri).typeindex == stype)
                        { // Belongs in this readInfo
                            foundri = true;
                            bool founds = false;
                            if((*fvalue >= (*ri).min) && (*fvalue <= (*ri).max))
                            {
                                int dp = 0; // find the right slot to put this datapoint
                                while(!founds && (dp < 10))
                                {
                                    if((*ri).points[dp] == -100)
                                    { // Here's an empty slot to put it
                                        (*ri).points[dp] = *fvalue;
                                        dp++;
                                        founds = true;
                                    }
                                    else dp++;
                                }
                                if(!founds)
                                { // No empty slots. Slide the data points
                                    for(int j=0; j<dp-1; j++)(*ri).points[j] = (*ri).points[j+1];
                                    (*ri).points[dp-1] = *fvalue;
                                }
                                float summit = 0.0; // compute the average. dp tells us how many data points to use
                                for(int j=0; j<dp; j++) summit = summit + (*ri).points[j];
                                (*ri).lastreading = summit/dp;
                                *fvalue = (*ri).lastreading;
                            }
                            else
                            { // Value outside of range, so drop it
                                if(zwDebug) fprintf(zl, "Reading from Sensor %s outside of range: %f\n", (dv->nodename).c_str(), *fvalue);
                            }
                            result = true;
                        }
                        ri++;
					}
					if(!foundri) fprintf(zl, "Did not find typeindex %d", stype);
				}
                else fprintf(zl, "Mal formed response %s from %s\n", dvresp, sURL.c_str());
			}
            else fprintf(zl, "Failed sending action to wifisensor %s\n", dv->nodename.c_str());
        }
	}
	return result;
}
