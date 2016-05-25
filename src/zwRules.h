/*
 * zwRules.h
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#ifndef ZWRULES_H_
#define ZWRULES_H_

#include <stdlib.h>
#include "hdatepp.h"
#include <fstream>
#include "zwDevices.h"

#define MONGOOSE
#ifdef MONGOOSE
#include "mongoose.h"
#endif

using namespace hdate;
using namespace std;

class zwRules
{

public:
	struct datestruct
	{
		vector<int> DateType; /* 1=day of week; 2=Gregorian date; 3=Hebrew date */
		vector<struct tm> DateTime; /* structure for holding the list of FROM Event dates and times */
		vector<Hdate *> HebDate;
		vector<int> TimeType; // 1=24 hour time; 2=sunrise+/-offset; 3=sunset+/-offset
		vector<int> RSoffset; // holds the FROM offset for sunrise and sunset
		vector<time_t> Time; // list of FROM Event date/time in seconds
	};

	zwRules(); //constructor
	~zwRules(); //destructor
	bool opeval(float sensor_report, int sensor_op, float ref_value);
	int init(class zwPrefs *zwP);
	int ingest(zwPrefs *zwP); // parses rule, expands it, and stores result in zwRules. Returns 0=Okay or error code
	void update(datestruct *newdate, int nindex); // updates part of the rule for the next event to trigger
	int loadchkptfile(string chkptfilename); // load a checkpoint file into a rule list
	int run(zwPrefs *zwP, zwDevices *zwD);
	char sys_stat[8];
	list<dvInfo> dvl;
	time_t getsimstarttime();
	time_t getsimendtime();
	void setsimstarttime(time_t startdatetime);
	void setsimendtime(time_t enddatetime);
	void setexecutemode(bool modeflag);
	bool getexecutemode();
	string getdayname(int daynum);

private:
		typedef struct
	{
		int on_action; /* The action to perform on the device(s) when the rule fires */
		int off_action; /* The action to perform on the device(s) when the rule goes inactive */
		vector<dvInfo> device; /* node id */
		int eventDurationType; // 0=unknown; 1=FROM/TO 2=ON
		datestruct fromEvent;
		datestruct toEvent;
		datestruct fromCondition;
		datestruct toCondition;

		int conditionType; // 0=NoConditions 1=IF; 2=EXCEPT-IF; 3=WHEN; 4=EXCEPT-WHEN
		dvInfo *sensorNode;
		int sensorIndex;
		int sensorOp; // 0= “=”; 1= “<”; 2= “>”
		float sensorValue;
		int conditionDurationType; // 0=unknown; 1=FROM/TO 2=ON
		int firing; /* contains the index of the event currently firing; otherwise = 0 */
		bool enforce_from;
		bool enforce_to;
		string ruletext; // Original rule text
	} zwRule;

	list<zwRule> rulelist;

	typedef struct
	{
		int action;
		time_t acttime;
		vector<dvInfo> dv;
		bool enforce;
		long itemID;
	} q_item;

	float sval;
	list<q_item> q_list;
	ifstream rulefile;
	map<string, int> actionmap;
	map<string, int> revertmap;
	map<string, int> conditionmap;
	map<string, int> sensortypemap;
	map<string, int> sensoropmap;
	map<string, int> sensorvaluemap;
	bool executeflag;
	bool deviceinit;
	time_t simstarttime;
	time_t simendtime;
	vector<string> daynames;
	vector<string> gmonthnames;
	vector<string> hmonthnames;
	vector<string> homeloc;
	double home_lat, home_long;
	int home_tz;
	bool sys_pause;
	list<dvInfo> srl;
	long itemIDgen();
	int parsedate(string datestr, datestruct *ds);
	int parsetime(vector<string> *timestr, datestruct *ds);
	int q_action(int action, time_t acttime, vector<dvInfo> nodeId, bool enforce, long iid);
	void printrule(zwRule *theRule);
};

#endif /* ZWRULES_H_ */
