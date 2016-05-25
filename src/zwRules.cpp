/*
 * zwRules.cpp
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#include "stdafx.h"
#include <string>
#include <ctime>
#include <list>
#include <vector>
#include <iostream>
#include <algorithm>
#include "zwPrefs.h"
#include "Tokenizer.h"
#include "zwhome.h"

using namespace hdate;
using namespace std;

static long uuid;

zwRules::zwRules()
{
	home_lat = atof("39.3680556");
	home_long = atof("-76.6941667");
	home_tz = atoi("-5");
	simstarttime = time(NULL);
	simendtime = time(NULL);
	executeflag = false;
	deviceinit = false;
	strcpy(sys_stat, "on");
	sys_pause = false;
	sval = 0.0;
};

long zwRules::itemIDgen()
{
	return (++uuid);
}

bool zwRules::opeval(float sensor_report, int sensor_op, float ref_value)
{
	switch(sensor_op)
	{
	case 1: // <
		if(sensor_report < ref_value) return true;
		else return false;
		break;
	case 2: // =
		if(sensor_report - ref_value < 0.05) return true;
		else return false;
		break;
	case 3: // >
		if(sensor_report > ref_value) return true;
		else return false;
		break;
	default:
		return false;
		break;
	}
	return false;
}

void zwRules::printrule(zwRule *theRule)
{
	fprintf(zl, "\nRuletext: %s\n",(theRule->ruletext).c_str());
	fprintf(zl,"action: ON = %d OFF = %d\n",theRule->on_action,theRule->off_action);
	fprintf(zl,"node(s): ");
	for(vector<dvInfo>::iterator i=theRule->device.begin(); i!=theRule->device.end();i++) fprintf(zl," %s",((*i).nodename).c_str());
	fprintf(zl, "\n");
	switch(theRule->eventDurationType)
	{
	case 1:
		for(vector<struct tm>::iterator i=theRule->fromEvent.DateTime.begin(); i!=theRule->fromEvent.DateTime.end(); i++)
		{
			fprintf(zl,"Event From:\t English: %s",asctime(&(*i)));
		};
		for(vector<Hdate *>::iterator i=theRule->fromEvent.HebDate.begin();i!=theRule->fromEvent.HebDate.end(); i++)
		{
			fprintf(zl,"Event From:\t Hebrew: %s\n",(*i)->get_format_date(false));
		};
		fprintf(zl,"From-event enforced? %s\n",(theRule->enforce_from?"yes":"no"));
		for(vector<struct tm>::iterator i=theRule->toEvent.DateTime.begin();i!=theRule->toEvent.DateTime.end(); i++)
		{
			fprintf(zl,"Event To:\t English: %s",asctime(&(*i)));
		};
		for(vector<Hdate *>::iterator i=theRule->toEvent.HebDate.begin();i!=theRule->toEvent.HebDate.end(); i++)
		{
			fprintf(zl,"Event To:\t Hebrew: %s\n",(*i)->get_format_date(false));
		};
		fprintf(zl,"To-event enforced? %s\n",(theRule->enforce_to?"yes":"no"));
		break;
	case 2:
		for(vector<struct tm>::iterator i=theRule->fromEvent.DateTime.begin();i!=theRule->fromEvent.DateTime.end(); i++)
		{
			fprintf(zl,"Event On:\t English: %s",asctime(&(*i)));
		};
		for(vector<Hdate *>::iterator i=theRule->fromEvent.HebDate.begin();i!=theRule->fromEvent.HebDate.end(); i++)
		{
			fprintf(zl,"Event On:\t Hebrew: %s\n",(*i)->get_format_date(false));
		};
		fprintf(zl,"On/Off-event enforced? %s\n",(theRule->enforce_from?"yes":"no"));
		break;
	default:
		fprintf(zl,"Unknown Event Duration Type = %d\n",theRule->eventDurationType);
		break;
	}; // switch eventDurationType
	switch(theRule->conditionType)
	{
	case 0:
		fprintf(zl,"No Conditions\n");
		break;
	case 1:
	case 2:
		fprintf(zl,"Condition Duration Type = %d\n",theRule->conditionDurationType);
		switch(theRule->conditionDurationType)
		{
		case 1:
			for(vector<struct tm>::iterator i=theRule->fromCondition.DateTime.begin();i!=theRule->fromCondition.DateTime.end(); i++)
			{
				fprintf(zl,"Condition From:\t English: %s",asctime(&(*i)));
			};
			for(vector<Hdate *>::iterator i=theRule->fromCondition.HebDate.begin();i!=theRule->fromCondition.HebDate.end(); i++)
			{
				fprintf(zl,"Condition From:\t Hebrew: %s\n",(*i)->get_format_date(false));
			};
			for(vector<struct tm>::iterator i=theRule->toCondition.DateTime.begin();i!=theRule->toCondition.DateTime.end(); i++)
			{
				fprintf(zl,"Condition To:\t English: %s",asctime(&(*i)));
			};
			for(vector<Hdate *>::iterator i=theRule->toCondition.HebDate.begin();i!=theRule->toCondition.HebDate.end(); i++)
			{
				fprintf(zl,"Condition To:\t Hebrew: %s\n",(*i)->get_format_date(false));
			};
			break;
		case 2:
			for(vector<struct tm>::iterator i=theRule->fromCondition.DateTime.begin();i!=theRule->fromCondition.DateTime.end(); i++)
			{
				fprintf(zl,"Condition On:\t English: %s",asctime(&(*i)));
			};
			for(vector<Hdate *>::iterator i=theRule->fromCondition.HebDate.begin();i!=theRule->fromCondition.HebDate.end(); i++)
			{
				fprintf(zl,"Condition On:\t Hebrew: %s\n",(*i)->get_format_date(false));
			};
			break;
		default:
			fprintf(zl,"Unknown Condition Duration Type = %d\n",theRule->conditionDurationType);
			break;
		}; // switch conditionDurationType
		break;
	case 3:
	case 4:
		fprintf(zl,"Sensor node = %s",((theRule->sensorNode)->nodename).c_str());
		fprintf(zl,"; Sensor operation = %d; Threshold value = %f\n",theRule->sensorOp,theRule->sensorValue);
		break;
	default:
		fprintf(zl,"Unknown Condition Type = %d\n",theRule->conditionType);
			break;
	}; // switch conditionType
	fprintf(zl,"This rule is %s\n",((theRule->firing < 0)?"not firing":"firing"));
};

int zwRules::parsetime(vector<string> *timestr, datestruct *ds)
{
	for(unsigned int i=0; i< timestr->size(); i++)
	{
		int result = -700;
		struct tm tmdate = ds->DateTime[i];
		tmdate.tm_isdst = -1; // let mktime figure out DST
		int rsoffset = ds->RSoffset[i];
		if(is_number((*timestr)[i]))
		{ // absolute 24-hour time
			int evt2400 = atoi((*timestr)[i].c_str());
			tmdate.tm_sec = 0;
			tmdate.tm_min = evt2400 % 100;
			tmdate.tm_hour = evt2400/100;
			ds->TimeType.push_back(1);
			result = 1;
		};
		if(!(*timestr)[i].compare(0, 7, "sunrise"))
		{ // sunise format
			if((*timestr)[i].length() > 7) rsoffset = atoi((*timestr)[i].substr(7, (*timestr)[i].size()-1).c_str());
			else rsoffset = 0;
			tmdate.tm_sec = 0;
			ds->HebDate[i]->set_location(home_lat, home_long, home_tz);
			tmdate.tm_min = rsoffset + ds->HebDate[i]->get_sunrise();
			tmdate.tm_hour = 0;
			ds->TimeType.push_back(2);
			result = 2;
		};
		if(!(*timestr)[i].compare(0, 6, "sunset"))
		{ // sunset format
			if((*timestr)[i].length() > 7) rsoffset = atoi((*timestr)[i].substr(6, (*timestr)[i].size()-1).c_str());
			else rsoffset = 0;
			tmdate.tm_sec = 0;
			ds->HebDate[i]->set_location(home_lat, home_long, home_tz);
			tmdate.tm_min = rsoffset + ds->HebDate[i]->get_sunset();
			tmdate.tm_hour = 0;
			ds->TimeType.push_back(3);
			result = 3;
		};
		if(result <0)
		{
			fprintf(zl, "Error code %d - Bad Format: %s\n",result,((*timestr)[i]).c_str()); fflush(zl);
			return result;
		};
		ds->RSoffset[i] = rsoffset;
		ds->Time[i] = mktime(&tmdate);
		if((result == 2) || (result == 3))
		{
			if(tmdate.tm_isdst == 1)
			{
				tmdate.tm_hour++;
				ds->Time[i] = mktime(&tmdate);
			}
		};
		ds->DateTime[i] = tmdate;
	};
	return 0;
};

void zwRules:: update(datestruct *newdate, int nindex)
{
	newdate->DateTime[nindex].tm_isdst = -1; // let mktime figure out DST
	switch (newdate->DateType[nindex])
	{
	case 1:  // day of week
		newdate->DateTime[nindex].tm_mday +=7;
		newdate->HebDate[nindex]->set_gdate(newdate->DateTime[nindex].tm_mday, newdate->DateTime[nindex].tm_mon + 1, newdate->DateTime[nindex].tm_year + 1900);
		break;
	case 2: // Gregorian date
		newdate->DateTime[nindex].tm_year++;
		newdate->HebDate[nindex]->set_gdate(newdate->DateTime[nindex].tm_mday, newdate->DateTime[nindex].tm_mon + 1, newdate->DateTime[nindex].tm_year + 1900);
		break;
	case 3: // Hebrew date
			newdate->HebDate[nindex]->set_hdate(newdate->HebDate[nindex]->get_hday(), newdate->HebDate[nindex]->get_hmonth(), newdate->HebDate[nindex]->get_hyear()+1);
			newdate->DateTime[nindex].tm_year = newdate->HebDate[nindex]->get_gyear() - 1900;
			newdate->DateTime[nindex].tm_mon = newdate->HebDate[nindex]->get_gmonth() - 1;
			newdate->DateTime[nindex].tm_mday = newdate->HebDate[nindex]->get_gday();
		break;
	};
	int timetype = newdate->TimeType[nindex];
	switch (newdate->TimeType[nindex])
	{
	case 1: // 24 hour time
		break;
	case 2: // sunrise. We use the offset already set in the newdate.
		newdate->DateTime[nindex].tm_sec = 0;
		newdate->DateTime[nindex].tm_min = newdate->RSoffset[nindex] + newdate->HebDate[nindex]->get_sunrise();
		newdate->DateTime[nindex].tm_hour = 0;
		break;
	case 3: //sunset. We use the offset already set in the newdate.
		newdate->DateTime[nindex].tm_sec = 0;
		newdate->DateTime[nindex].tm_min = newdate->RSoffset[nindex] + newdate->HebDate[nindex]->get_sunset();
		newdate->DateTime[nindex].tm_hour = 0;
		break;
	};
	newdate->Time[nindex] = mktime(&newdate->DateTime[nindex]);
	if((timetype == 2) || (timetype == 3))
	{
		if(newdate->DateTime[nindex].tm_isdst == 1)
		{
			newdate->DateTime[nindex].tm_hour++;
			newdate->Time[nindex] = mktime(&newdate->DateTime[nindex]);
		}
	}
};

int zwRules::parsedate(string datestr, datestruct *ds)
{ // returns the date type: 1=day of week; 2=Gregorian date; 3=Hebrew date
	int dgh = 0;
	int dowdiff;
	size_t dashpos;
	datestruct dsupdate;
	while (dgh < 7)
	{
		if(!daynames[dgh].compare(datestr))
		{ // it's a day of the week
			dowdiff = dgh - ds->DateTime.back().tm_wday;
			if(dowdiff < 0)
			{
				dowdiff += 7;
			};
			ds->DateTime.back().tm_mday += dowdiff;
			ds->DateTime.back().tm_isdst = -1; // let mktime figure out DST
			ds->Time.back() = mktime(&(ds->DateTime.back()));
			ds->HebDate.back()->set_gdate(ds->DateTime.back().tm_mday, ds->DateTime.back().tm_mon + 1, ds->DateTime.back().tm_year + 1900);
			ds->DateType.back() = 1; // Day-of-week type
			return 1;
		}
		else dgh++;
	};
	// It wasn't a day-of-week. Assume it's a date in day-month format
	dgh = 0;
	dashpos = datestr.find_first_of("-");
	if(dashpos == string::npos)
	{ // error
		return -501; // incorrect date syntax
	};
	int mday = atoi(datestr.substr(0, dashpos).c_str());
	string mon = datestr.substr(dashpos+1, datestr.size()-(dashpos+1));
	while (dgh < 12)
	{ // It's a Gregorian date
		if(!gmonthnames[dgh].compare(mon))
		{ // just fill in month and day
			ds->DateTime.back().tm_mon = dgh;
			ds->DateTime.back().tm_mday = mday;
			ds->DateTime.back().tm_isdst = -1;
			ds->Time.back() = mktime(&(ds->DateTime.back()));
			ds->HebDate.back()->set_gdate(ds->DateTime.back().tm_mday, ds->DateTime.back().tm_mon + 1,ds->DateTime.back().tm_year + 1900);
			ds->DateType.back() = 2; // Gregorian date type
			return 2;
		}
		else dgh++;

	};
	dgh = 0;
	while (dgh < 14)
	{
		if(!hmonthnames[dgh].compare(mon))
		{ // store the Hebrew month and day
			ds->HebDate.back()->set_hdate(mday, dgh + 1, ds->HebDate.back()->get_hyear());
			ds->DateTime.back().tm_mday = ds->HebDate.back()->get_gday();
			ds->DateTime.back().tm_mon = ds->HebDate.back()->get_gmonth() - 1;
			ds->DateTime.back().tm_year = ds->HebDate.back()->get_gyear() - 1900;
			ds->DateTime.back().tm_isdst = -1;
			ds->Time.back() = mktime(&(ds->DateTime.back()));
			ds->DateType.back() = 3; // Hebrew date type
			return 3;
		}
		else dgh++;

	};
	return -500; // unrecognized date error
};

int zwRules::init(class zwPrefs *zwP)
{
	actionmap["turn-off"] = 0;
	actionmap["turn-on"] = 1;
	revertmap["turn-off"] = 1;
	revertmap["turn-on"] = 0;
	actionmap["simulate"] = 10;
	revertmap["simulate"] = 10;
	conditionmap["if"] = 1;
	conditionmap["except-if"] = 2;
	conditionmap["when"] = 3;
	conditionmap["except-when"] = 4;
	sensoropmap["<"] = 1;
	sensoropmap["="] = 2;
	sensoropmap[">"] = 3;
	setexecutemode(true);
	deviceinit = false;
	uuid = 1;
	if(!zwP->getmonths("gregorian").empty())
	{ // load in the user's names
		if(makeList(zwP->getmonths("gregorian"), &gmonthnames) < 0)
		{ // error
			return -150;
		}
	}
	else
	{ // load in the default names
		if(makeList("jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec", &gmonthnames) < 0)
		{ // error
			return -151;
		}
	}
	if(!zwP->getmonths("hebrew").empty())
	{ // load in the user's names
		if(makeList(zwP->getmonths("hebrew"), &hmonthnames) < 0)
		{ // error
			return -152;
		}
	}
	else
	{ // load in the default names
		if(makeList("tishrei|cheshvan|kislev|teves|shvat|adar|nissan|iyar|sivan|tamuz|av|elul|adar1|adar2", &hmonthnames) < 0)
		{ // error
			return -153;
		}
	}
	if(!zwP->getdays("english").empty())
	{ // load in the user's names
		if(makeList(zwP->getdays("english"), &daynames) < 0)
		{ // error
			return -154;
		}
	}
	else
	{ // load in the default names
		if(makeList("sunday|monday|tuesday|wednesday|thursday|friday|saturday", &daynames) < 0)
		{ // error
			return -155;
		}
	}
	if(!zwP->getloc("home").empty())
	{ // load in the user's home location
		if(makeList(zwP->getloc("home"), &homeloc) < 0)
		{ // error
			return -154;
		}
	}
	else
	{ // load in the default home location
		if(makeList("39.3680556|-76.6941667| -5", &homeloc) < 0)
		{ // error
			return -155;
		}
	};
	if(zwP->getdevlist(&dvl) < 0)
	{ // error
		return -156;
	}
	if(zwP->getsensorlist(&srl) < 0)
	{ // error
		return -157;
	}
	if(zwP->getwifisensorlist(&srl) < 0)
	{ // error
		return -158;
	}
	home_lat = atof(homeloc[0].c_str());
	home_long = atof(homeloc[1].c_str());
	home_tz = atoi(homeloc[2].c_str());
	return 0;
};

zwRules::~zwRules()
{
};

int zwRules::q_action(int action, time_t acttime, vector<dvInfo> dv, bool enforce, long itemID)
{
	// Place the action into a queue item
	q_item qi;
	qi.action = action;
	qi.acttime = acttime;
	qi.dv = dv;
	qi.enforce = enforce;
	qi.itemID = itemID;
	if(q_list.empty())
	{ // It's the smallest (and only) value in the queue
		q_list.push_front(qi);
	}
	else
	{
		bool queued = false;
		bool matched = false;
		// For each queued item
		// check to see if it's action time is later than that of the item to be queued
		// if it is, insert it into the queue
		// While doing that, check the item ID of the queued item is the same as that of the item to be queued
		// if it is, set a flag so we can delete queued items in-between that have the same nodes
		list<q_item>::iterator itq = q_list.begin();
		while ((itq != q_list.end()) && !queued)
		{
			if(!matched)
			{
				// do we have a match?
				if(itq->itemID == itemID)
				{ // yes
//					matched = true;
					itq++;
				}
				else
				{ //no match. See if we should insert the item in this part of the queue
					if(acttime <= itq->acttime)
					{ // yes
						q_list.insert(itq, qi);
						queued = true;
					}
					else itq++;
				}
			}
			else // We have some additional maintenance to do if we’re queuing an item with a matching ID
			{
				// We’re going to delete each reference to any node in any item that matches a node in the item to be queued
				// until we actually queue it. If we get down to zero nodes in an item, we’ll delete it entirely from the queue
				// This is going to be tricky because everytime we delete a node that isn’t at the end of the dv vector,
				// it will reposition the elements of the vector.
				vector<dvInfo>::iterator idv = qi.dv.begin();
				while(idv != qi.dv.end()) // compare each node in item to be queued...
				{
					vector<dvInfo>::iterator qdv = itq->dv.begin();
					while(qdv != itq->dv.end()) // ...to each node in this queued item
					{
						if((*qdv).nodeId == (*idv).nodeId)
						{ // delete the node from itq->dv
							qdv = itq->dv.erase(qdv);
						}
						else qdv++;
					}
					idv++;
				}
				// Here’s our check on whether we deleted all the nodes in the queue item
				if(itq->dv.empty())
				{ // delete this item from q_list
					itq = q_list.erase(itq);
				}
				else itq++;
			}
		} // while ((itq != q_list.end()) && !queued);
		if(!queued)
		{ // It's the largest value in the queue, so it goes at the end
			q_list.push_back(qi);
		}
	};
	return 0;
};

int zwRules::loadchkptfile(string chkptfilename) {return 0;}; // NOT IMPLEMENTED YET

time_t zwRules::getsimstarttime()
{
	return simstarttime;
};
time_t zwRules::getsimendtime()
{
	return simendtime;
};
void zwRules::setsimstarttime(time_t startdatetime)
	{
	simstarttime = startdatetime;
};

void zwRules::setsimendtime(time_t enddatetime)
	{
	simendtime = enddatetime;
};

void zwRules::setexecutemode(bool modeflag)
{
	executeflag = modeflag;
};
bool zwRules::getexecutemode()
{
	return executeflag;
};
