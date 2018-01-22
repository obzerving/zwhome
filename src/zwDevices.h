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
        int typeindex;
        int min;
        int max;
		float points[10]; // used by sensors
		float lastreading; // used by sensors and wifi switches
    } readInfo;

	typedef struct
	{
		string nodename; // used by zwRules_ingest
		int nodeId; // used by doAction, q_action, zwRules_ingest
		uint8 bccmap; // command class mapped to Basic command class
		string name; // device name
		string location; // device location
		string restURL; // used by wifi sensors and switches
		bool isController; // designates a zwave controller
        bool isFan; // Capability = 128
		bool isLight; // Capability = 64
		bool canDim; // Capability = 32
		bool reportsBattery; // Capability = 16
		bool isLightSensor; // Capability = 8
		bool isTempSensor; // Capability = 4
		bool isHumiditySensor; // Capability = 2
		bool isMotionSensor; // Capability = 1
		float voltage; // used by sensors and devices
		list<readInfo> reading;
		bool enforce;
		uint8			desired_state;
		time_t			desired_state_timestamp;
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

/*
enum  	SensorType {
  SensorType_Temperature = 1, SensorType_General, SensorType_Luminance, SensorType_Power,
  SensorType_RelativeHumidity, SensorType_Velocity, SensorType_Direction, SensorType_AtmosphericPressure,
  SensorType_BarometricPressure, SensorType_SolarRadiation, SensorType_DewPoint, SensorType_RainRate,
  SensorType_TideLevel, SensorType_Weight, SensorType_Voltage, SensorType_Current,
  SensorType_CO2, SensorType_AirFlow, SensorType_TankCapacity, SensorType_Distance,
  SensorType_AnglePosition, SensorType_Rotation, SensorType_WaterTemperature, SensorType_SoilTemperature,
  SensorType_SeismicIntensity, SensorType_SeismicMagnitude, SensorType_Ultraviolet, SensorType_ElectricalResistivity,
  SensorType_ElectricalConductivity, SensorType_Loudness, SensorType_Moisture, SensorType_MaxType
};
*/

#define SENSOR_TYPE_CNT 6
char SensorType[SENSOR_TYPE_CNT][20];

char fanSpeeds[5][8];

	static pthread_mutex_t g_criticalSection;
	static pthread_cond_t initCond;
	static pthread_mutex_t initMutex;
	uint32 g_homeId;
	bool g_nodesQueried;
	list<NodeInfo*> g_nodes;
    bool sendWifiCmd(string wificmd, char *resp);
	int init(bool executeflag);
	void wrapup();
//	int doAction(uint8 action, vector<dvInfo> nodeId, bool enforce);
	int doAction(uint8 action, list<string> nodenames, bool enforce);
	bool getSensorValue(dvInfo *dv, float *fvalue);
};

#endif /* ZWDEVICES_H_ */

