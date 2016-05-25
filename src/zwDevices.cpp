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
// #include <stdlib.h> not needed if cstdlib is included?
#include <string.h>

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
#ifdef linux
pthread_mutex_t zwDevices::g_criticalSection;
pthread_cond_t zwDevices::initCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t zwDevices::initMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#define zwDebug 1

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

int zwDevices::init()
{
	g_homeId = 0;
	g_nodesQueried = false;
	nodetypemap["switch"] = 1;
	nodetypemap["temp_sensor"] = 2;
	nodetypemap["humidity_sensor"] = 4;
	nodetypemap["light_sensor"] = 8;
	nodetypemap["motion_sensor"] = 16;
	nodetypemap["battery"] = 32;

#ifdef WIN32
			InitializeCriticalSection( &g_criticalSection );
			Options::Create("../../ozw758/config/", "", "" );
#endif
#ifdef linux
			pthread_mutexattr_t mutexattr;
			pthread_mutexattr_init(&mutexattr);
			pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&g_criticalSection, &mutexattr);
			pthread_mutexattr_destroy(&mutexattr);
			pthread_mutex_lock(&initMutex);
			Options::Create("/home/pi/Documents/open-zwave-1.4/config/", "", "" );
#endif
			Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Detail );
			Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
			Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );
			Options::Get()->AddOptionInt( "PollInterval", 500 );
			Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
			Options::Get()->AddOptionBool("ValidateValueChanges", true);
			Options::Get()->Lock();
			Manager::Create();
			Manager::Get()->AddWatcher( OnNotification, NULL );
#ifdef WIN32
			Manager::Get()->AddDriver( "\\\\.\\COM4" );
#endif
#ifdef linux
			Manager::Get()->AddDriver( "/dev/ttyUSB0" );
			pthread_cond_wait(&initCond, &initMutex);
#endif
			while( !g_homeId )
			{
#ifdef WIN32
				Sleep( 1000 );
#endif
#ifdef linux
				sleep(1);
#endif
			}
			while( !g_nodesQueried )
			{
#ifdef WIN32
				Sleep( 1000 );
#endif
#ifdef linux
				sleep(1);
#endif
			}
			Manager::Get()->WriteConfig( g_homeId );
#ifdef WIN32
			Sleep(1000);
#endif
#ifdef linux
			sleep(1);
#endif
			Log::SetLoggingState(false); // turn off open-zwave logging
			return 0;
}

void zwDevices::wrapup()
{
#ifdef WIN32
	DeleteCriticalSection( &g_criticalSection );
#endif
#ifdef linux
	pthread_mutex_destroy( &g_criticalSection );
#endif
};

int zwDevices::doAction(int action, uint32 homeId, vector<dvInfo> nodeId, bool enforce)
{
// Action values: zero = off; positive = on; negative = toggle
	for(vector<dvInfo>::iterator itd=nodeId.begin(); itd!=nodeId.end(); itd++) // for each node
	{
		if(zwDebug)
		{
			time_t rt;
			time(&rt);
			fprintf(zl, "%sdoAction: Node = %s", ctime(&rt), ((*itd).nodename).c_str());  fflush(zl);
		};

#ifdef WIN32
		EnterCriticalSection( &g_criticalSection );
#endif
#ifdef linux
		pthread_mutex_lock( &g_criticalSection );
#endif
		for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it ) // find node in nodelist
		{
			NodeInfo* nodeInfo = *it;
			if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == (*itd).nodeId ) )
			{ // this is the node. Find the right value
				list<ValueID>::iterator itv=nodeInfo->m_values.begin();
				list<valInfo>::iterator vi=nodeInfo->v_info.begin();
				bool foundv = false;
				while((itv != nodeInfo->m_values.end()) && !foundv)
				{
					if((*itd).vId.front() == (*itv) )
					{ // this is the right value. See if the device is already in the right state
						bool devBool;
						Manager::Get()->GetValueAsBool( (*itv), &devBool ); // actual state of device
						if(action < 0) action = (int) (devBool?0:1);
						(*vi).desired_state = action;
						(*vi).enforce = enforce;
						if(action != (int) (devBool?1:0))
						{ // actual state is not the desired state, so change it
							if(zwDebug)
							{
								fprintf(zl, ", Change state: action = %d, report state = %d\n", action, (int) ((*vi).reported_state)); fflush(zl);							};
// Fix the hardcoding of 0 for TURN-OFF
							if(action==0)
							{
								Manager::Get()->SetValue((*itv), false);
							}
							else
							{
								Manager::Get()->SetValue((*itv), true);
							};
						}
						else
						{ // no action necessary
							if(zwDebug)
							{
								fprintf(zl, ", already at state = %d\n", action); fflush(zl);							}
						};
						foundv = true;
					}
					else
					{
						vi++;
						itv++;
					}
				}
			}
		};
#ifdef WIN32
		LeaveCriticalSection( &g_criticalSection );
#endif
#ifdef linux
		pthread_mutex_unlock( &g_criticalSection );
#endif

	};
	return 0;
};

bool zwDevices::getSensorValue(dvInfo *dv, float *fvalue)
{
	bool result = false; // returns true only if the sensor has a new value to report
	time_t current_time;

	if(dv->nodeId > 0)
	{ // The sensor is a z-wave device
#ifdef WIN32
		EnterCriticalSection( &g_criticalSection );
#endif
#ifdef linux
		pthread_mutex_lock( &g_criticalSection );
#endif
		for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
		{
			NodeInfo* nodeInfo = *it;
			if( nodeInfo->m_nodeId != dv->nodeId) continue;
			list<valInfo>::iterator bitr = nodeInfo->v_info.begin();
			list<ValueID>::iterator itd = nodeInfo->m_values.begin();
			bool foundv = false;
			while((itd != nodeInfo->m_values.end()) && !foundv)
			{
				if(dv->vId.front() == (*itd))
				{
					if((*bitr).f_value)
					{
						Manager::Get()->GetValueAsFloat( (*itd), fvalue );
						(*bitr).f_value = false; // Reset the new value flag
						fprintf(zl, "\nSensor %s reports %.2f on %s\n", dv->nodename.c_str(), *fvalue, ctime(&current_time)); fflush(zl);
						dv->lastreading = *fvalue;
						result = true;

					}
					foundv = true;
				}
				else
				{
					bitr++;
					itd++;
				}
			}
		};
#ifdef WIN32
		LeaveCriticalSection( &g_criticalSection );
#endif
#ifdef linux
		pthread_mutex_unlock( &g_criticalSection );
#endif
	}
	else
	{ // The sensor is a Wifi device
		CURL *curl_handle;
		CURLcode res;

		struct MemoryStruct chunk;

		time(&current_time);
		if((dv->readinterval > 0) && (current_time > dv->nextread) )
		{ // ready for next read, so call the device's restful interface
			struct tm * timeptr;
			chunk.memory = static_cast<char*> (malloc(1));  /* will be grown as needed by the realloc above */
			chunk.size = 0;    /* no data at this point */

			curl_global_init(CURL_GLOBAL_ALL);

			/* init the curl session */
			curl_handle = curl_easy_init();

			/* specify URL to get */
			curl_easy_setopt(curl_handle, CURLOPT_URL, dv->restURL.c_str());

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
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res)); fflush(zl);
			}
			else
			{
				/*
				* Now, our chunk.memory points to a memory block that is chunk.size
				* bytes big and contains the response from the sensor.
				*
				* {“luminance”: 00, “id”: “1”, name: “front-light-sensor”, “connected”: true}
				*/

				// fprintf(zl, "%lu bytes retrieved\n", (long)chunk.size);
				if(chunk.size > 0)
				{
					char *sensorv;
					sensorv = strtok(chunk.memory,"{\":, ");
					sensorv = strtok(NULL,":, ");
					if(sensorv == NULL) result = false;
					else
					{
						*fvalue = atof(sensorv);
						fprintf(zl, "\nSensor %s reports (%s) %.2f on %s\n", dv->nodename.c_str(), sensorv, *fvalue, ctime(&current_time)); fflush(zl);
						result = true;
						timeptr = localtime(&current_time); // set up for next read
						timeptr->tm_sec += dv->readinterval;
						dv->nextread = mktime(timeptr);
					}
				}
				else result = false; // didn't work, so we'll try again on another call
			}

			/* cleanup curl stuff */
			curl_easy_cleanup(curl_handle);

			free(chunk.memory);

			/* we're done with libcurl, so clean it up */
			curl_global_cleanup();
		}
	}
	return result;
};



