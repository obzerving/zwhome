/*
 * zwDevices.h
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#ifndef ZWDEVICES_H_
#define ZWDEVICES_H_

#ifdef WIN32
#define _USE_MATH_DEFINES
#include "Windows.h"
#endif
#ifdef linux
#include <unistd.h>
#include <pthread.h>
#endif
#include <vector>
#include "Manager.h"
#include "ValueID.h"

using namespace OpenZWave;
using namespace std;

	typedef struct
	{
		string nodename;
		int nodeId;
		list<ValueID> vId;
		string restURL;
		bool isLightSensor;
		bool isTempSensor;
		bool isHumiditySensor;
		bool isMotionSensor;
		bool isLight;
		bool canDim;
		int readinterval; // in seconds
		time_t nextread;
		float lastreading;
		uint8 in_use; // for sensor nodes: number of currently firing rules using sensor
	}dvInfo;

	typedef struct
	{
		bool			f_value;
		uint8			desired_state;
		time_t			desired_state_timestamp;
		uint8			reported_state;
		bool			enforce;
	}valInfo;

	typedef struct
	{
		uint32			m_homeId;
		uint8			m_nodeId;
		bool			m_polled;
		list<ValueID>	m_values;
		list<valInfo>	v_info;
	}NodeInfo;

class zwDevices
{

public:

#ifdef WIN32
	CRITICAL_SECTION g_criticalSection;
#endif
#ifdef linux
	static pthread_mutex_t g_criticalSection;
	static pthread_cond_t initCond;
	static pthread_mutex_t initMutex;
#endif
	uint32 g_homeId;
	bool g_nodesQueried;
	list<NodeInfo*> g_nodes;
	int init();
	void wrapup();
	int doAction(int action, uint32 homeId, vector<dvInfo> nodeId, bool enforce);
	int getNodetype(uint8 nodeId);
	bool getSensorValue(dvInfo *dv, float *fvalue);
	map<string, int> nodetypemap;
};

#endif /* ZWDEVICES_H_ */
