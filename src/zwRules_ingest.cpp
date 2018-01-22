/*
 * zwRules_ingest.cpp
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#include "stdafx.h"
#include <string>
#include <ctime>
#include <algorithm>
#include "zwPrefs.h"
#include "Tokenizer.h"
#include "zwhome.h"

using namespace std;

#define zwDebug 0

int zwRules::ingest(zwPrefs *zwP)
{
	string aline;
	string subtoken;
	int bstart, bend;
	string rulefrag;
	string kw;
	// Open the rules file
	rulefile.open(zwP->getrulefilename().c_str());
	if(zwDebug) fprintf(zl, "Opened Rule file %s\n", zwP->getrulefilename().c_str());
	list<string> tline;
	zwRule *current_rule;	// For each rule
	while (rulefile.good())
	{
        // Read it into a string
		getline(rulefile, aline);
		transform(aline.begin(), aline.end(), aline.begin(), ::tolower); // convert to lowercase
        // Substitute labels in line
		while((bstart = aline.find('['))!= (int) string::npos)
		{ // found start of label
			if((bend = aline.find(']'))== (int) string::npos)
			{ // error
				return -100; // syntax error: [ without ]
			};
			kw = aline.substr(bstart+1, bend-bstart-1); // this is the label without brackets
			aline.erase(bstart, bend-bstart+1); // delete the label
			aline.insert(bstart, zwP->getdefvalue(kw)); // insert the corresponding value
		};
		if(zwDebug)fprintf(zl, "Processing rule: %s\n", aline.c_str());
		// tokenize the line
		Tokenizer atoken(aline);
		while(atoken.NextToken())
		{
			tline.push_back(atoken.GetToken());
		};
		tline.push_back("#"); // force the line to end with a comment symbol
		// init the parser
		int parse_state = 0; // current state number

		while (!tline.empty())
		{
            if(zwDebug)fprintf(zl, "Parse state %d: Token = %s\n",parse_state, tline.front().c_str());
            struct tm *tmdate;
            time_t thistime;
            if(getexecutemode())
            {
                time(&thistime); // start with today's date
            }
            else
            {
                thistime = getsimstarttime(); // start with beginning of simulation
            };
            tmdate = localtime(&thistime);
			switch(parse_state)
			{
			case 0: // parse action or comment
				if(tline.front()[0]=='#') // also checking for comment lines
				{ // don't process it
					tline.clear(); // empty the token buffer
					parse_state = 100; // not a valid rule
				}
				else
				{
					vector<string> actlist;
					if((makeList(tline.front(), &actlist) < 0) || (actlist.size() != 3))
					{ // error
						return -101; // malformed action list
					}
					else
					{  // store the action
						current_rule = new zwRule();
						current_rule->ruletext = aline;
						current_rule->eventDurationType = 0;
						current_rule->conditionDurationType = 0;
						current_rule->conditionType = 0;
						if(!strcmp(actlist[1].c_str(),"on")) current_rule->on_action = 99;
						else  if(!strcmp(actlist[1].c_str(),"off")) current_rule->on_action = 0;
							else if(is_number(actlist[1])) current_rule->on_action = atoi(actlist[1].c_str());
								else return -1011; // Invalid on_action
						if(!strcmp(actlist[2].c_str(),"on")) current_rule->off_action = 99;
						else  if(!strcmp(actlist[2].c_str(),"off")) current_rule->off_action = 0;
							else if(is_number(actlist[2])) current_rule->off_action = atoi(actlist[2].c_str());
								else return -1012; // Invalid off_action
						current_rule->firing = -1;
						parse_state = 1;
					}
				}
				break;
			case 1: // parse device
				{
					vector<string> devlist;
					if(makeList(tline.front(), &devlist) < 0)
					{ // error
						return -102; // malformed device list
					};
					for(unsigned int i=0; i<devlist.size(); i++)
					{ // for each device in the rule
						bool foundd = false;
						list<dvInfo>::iterator idvl = dvl.begin();
						while((idvl != dvl.end()) && !foundd)
						{ // See if the device is in the list
                            // Is it a fully qualified switch name (e.g. bedroom.fan_low)?
                            string cap;
                            string dname;
                            size_t foundi = devlist[i].find('_');
                            if(foundi != std::string::npos)
                            { // Yes
                                cap = devlist[i].substr(foundi+1); // Here's the capability
                                if((*idvl).isFan)
                                {
                                    for(int j=0; j<5;j++)
                                    {
                                        if(!strcmp(cap.c_str(), zwD->fanSpeeds[j])) current_rule->on_action = j;
                                    }
                                }
                                dname = devlist[i].substr(0, foundi); // Here's the name that should be in the device list
                            }
                            else dname = devlist[i]; // No. Was just a regular device name
							if(dname == (*idvl).nodename)
							{ // found the device
//								current_rule->device.push_back(*idvl);
								current_rule->device.push_back(dname);
								foundd = true;
							}
							else idvl++;
						}
						if(!foundd)
						{ // error
							return -103; // unknown device name
						};
					}
					parse_state = 2;
					}
				break;
			case 2: // check for FROM or ON keywords
				{
					string tl = tline.front();
					current_rule->eventDurationType = 1;
					if(tl[tl.size()-1] == '!') current_rule->enforce_from = true;
					else current_rule->enforce_from = false;
					if(!tl.compare("from") || !tl.compare("from!"))
					{
						current_rule->eventDurationType = 1;
						parse_state = 3;
					};
					if(!tl.compare("on") || !tl.compare("on!"))
					{
						current_rule->eventDurationType = 2;
						parse_state = 3;
					};
					if(parse_state == 2)
					{ // error
						return -104; // unrecognized keyword
					};
					break;
				}
			case 3: //Process FROM or FROM! event date
				{
					vector<string> datelist;
					if(makeList(tline.front(), &datelist) < 0)
					{ // error
						return -105; // malformed event list
					};
					int datetype;
					for(unsigned int i=0; i<datelist.size(); i++)
					{
						current_rule->fromEvent.Time.push_back(thistime);
						current_rule->fromEvent.DateTime.push_back(*tmdate);
						current_rule->fromEvent.DateType.push_back(0); // Don't know what it is yet
						current_rule->fromEvent.HebDate.push_back(new Hdate());
						current_rule->fromEvent.HebDate.back()->set_gdate(tmdate->tm_mday, tmdate->tm_mon+1, tmdate->tm_year+1900);
						if((datetype = parsedate(datelist[i], &(current_rule->fromEvent))) < 0)
						{ // error
							return -106; // unknown datetype
						};
					};
					parse_state = 4;
					break;
				}
			case 4: // parse the AT keyword
				{
					if(tline.front().compare("at"))
					{ // Did we forget to specify the from-event-time?
						if(current_rule->eventDurationType == 1)
						{ // This is a from-type event
							if((!tline.front().compare("to"))||(!tline.front().compare("to!")))
							{ // from-event-time not specified deliberately, so assume 0000 hours
								list<string>::iterator itline;
								itline = tline.begin();
								tline.insert(itline, "at"); // This token won't really be processed
								tline.insert(itline, "0000");
							}
						}
						else if(current_rule->eventDurationType == 2)
						{ // This is an on-type event
							if(!tline.front().compare("#"))
							{ // from-event-time not specified deliberately, so assume 0000 hours
								list<string>::iterator itline;
								itline = tline.begin();
								tline.insert(itline, "at"); // This token won't really be processed
								tline.insert(itline, "0000");
							}
						}
						else
						{ // error
							return -107; // from-event-time not specified
						}
					};
					parse_state = 5;
					break;
				}
			case 5: // parse the from-event-time
				{
					vector<string> timelist;
					if(makeList(tline.front(), &timelist) < 0)
					{ // error
						return -108; // malformed time list
					};
					if((timelist.size() != 1)&&(timelist.size() != current_rule->fromEvent.DateTime.size()))
					{ // error
						return -109; // from-event-time must be 1 or equal to number of from-event-date items
					};
					if(timelist.size() == 1)
					{
						for(unsigned int i=0; i< current_rule->fromEvent.DateTime.size()-1; i++)
						{
							timelist.push_back(tline.front());
						}
					};
					int result;
					// size RSoffset to the same number of items as from-event-date and init them to 0
					current_rule->fromEvent.RSoffset.resize(current_rule->fromEvent.DateTime.size(), 0);
					if((result = parsetime(&timelist, &current_rule->fromEvent)) < 0)
					{ // error
							return -110; // bad time format
					};
					if(current_rule->eventDurationType == 1)
					{
						parse_state = 6; // Parse the TO part of the event
					}
					else
					{ // There is no end to an ON rule
						parse_state = 10;
					};
					break;
				}
			case 6: // check for TO or TO! or TO@ keyword
				if(!tline.front().compare("to"))
				{
					current_rule->enforce_to = false;
					parse_state = 7;
				};
				if(!tline.front().compare("to!"))
				{
					current_rule->enforce_to = true;
					parse_state = 7;
				}
				if(!tline.front().compare("to@")) // @ is only valid for the end of events
				{ // At the end of the event, instead of turning off the device, we just turn off enforcement
					current_rule->enforce_to = true;
					if(current_rule->off_action < 155) current_rule->off_action += 100;
					parse_state = 7;
				}
				if(parse_state == 6)
				{ // error
					return -111; // unrecognized keyword
				};
				break;
			case 7: // parse to-event-date
				{
					vector<string> datelist;
					if(makeList(tline.front(), &datelist) < 0)
					{ // error
						return -112; // malformed event list
					};
					if(datelist.size() != current_rule->fromEvent.DateTime.size())
					{ // error
						return -113; // number of TO dates must equal number of FROM dates
					};
					struct tm *tmdate;
					tmdate = new(struct tm);
					int datetype;
					for(unsigned int i=0; i<datelist.size(); i++)
					{
						tmcopy(&(current_rule->fromEvent.DateTime[i]), tmdate); // start with from-event-date
						tmdate->tm_isdst = -1;
						current_rule->toEvent.Time.push_back(mktime(tmdate));
						current_rule->toEvent.DateTime.push_back(*tmdate);
						current_rule->toEvent.DateType.push_back(0);
						current_rule->toEvent.HebDate.push_back(new Hdate());
						current_rule->toEvent.HebDate.back()->set_gdate(tmdate->tm_mday, tmdate->tm_mon+1, tmdate->tm_year+1900);
						if((datetype = parsedate(datelist[i], &(current_rule->toEvent))) < 0)
						{ // error
							return -114; // unknown datetype
						};
						if(datetype != current_rule->fromEvent.DateType[i])
						{ // error
							return -115; // TO datetype must equal FROM datetype
						};
					};
					parse_state = 8;
					break;
				}
			case 8: // check for AT keyword
				{
					if(tline.front().compare("at"))
					{ // Did we forget to specify the to-event-time?
						// check if this word is a condition keyword or a forced comment
						if((conditionmap.find(tline.front()) == conditionmap.end()) && (tline.front().compare("#")))
						{ // no. error
							return -116; // bad rule syntax
						};
						// yes. assume a to-event-time of 2359
						list<string>::iterator itline;
						itline = tline.begin();
						tline.insert(itline, "at");
						tline.insert(itline, "2359");
					};
					parse_state = 9;
					break;
				}
			case 9: // parse the to-event-time
				{
					vector<string> timelist;
					if(makeList(tline.front(), &timelist) < 0)
					{ // error
						return -117; // malformed time list
					};
					if((timelist.size() != 1)&&(timelist.size() != current_rule->toEvent.DateTime.size()))
					{ // error
						return -118; // to-event-time must be 1 or equal to number of to-event-date items
					};
					if(timelist.size() == 1)
					{
						for(unsigned int i=0; i< current_rule->toEvent.DateTime.size()-1; i++)
						{
							timelist.push_back(tline.front());
						}
					};
					int result;
					// size RSoffset to the same number of items as to-event-date and init them to 0
					current_rule->toEvent.RSoffset.resize(current_rule->toEvent.DateTime.size(), 0);
					if((result = parsetime(&timelist, &current_rule->toEvent)) < 0)
					{ // error
						return -119; // bad time format
					};
					parse_state = 10;
					break;
				}
			case 10: // check for a condition keyword or end of rule
				{
					if(!tline.front().compare("#"))
					{ // this rule has no conditions
						tline.push_back("#"); // add a token to continue the while loop
						parse_state = 80;
					}
					else {
						if(conditionmap.find(tline.front()) == conditionmap.end())
						{ // error
							return -120; // unrecognized keyword;
						};
						int ct = conditionmap[tline.front()];
						current_rule->conditionType = ct;
						if((ct == 1) || (ct == 2))
						{
							parse_state = 11; // process IF and EXCEPT_IF condition
						}
						else
						{
							parse_state = 20; // process WHEN and EXCEPT_WHEN condition
						}
					};
					break;
				}
			case 11: // check for FROM or ON keywords
				{
					if(!tline.front().compare("from"))
					{
						current_rule->conditionDurationType = 1;
						parse_state = 12;
					};
					if(!tline.front().compare("on"))
					{
						current_rule->conditionDurationType = 2;
						parse_state = 12;
					};
					if(parse_state == 11)
					{ // error
						return -111; // unrecognized keyword
					};
					break;
				}
			case 12: // process from-condition-date
				{
					vector<string> datelist;
					if(makeList(tline.front(), &datelist) < 0)
					{ // error
						return -105; // malformed event list
					};
					int datetype;
					for(unsigned int i=0; i<datelist.size(); i++)
					{
						current_rule->fromCondition.DateTime.push_back(*tmdate);
						current_rule->fromCondition.Time.push_back(thistime);
						current_rule->fromCondition.DateType.push_back(0); // Don't know what it is yet
						current_rule->fromCondition.HebDate.push_back(new Hdate());
						current_rule->fromCondition.HebDate.back()->set_gdate(tmdate->tm_mday, tmdate->tm_mon+1, tmdate->tm_year+1900);
						if((datetype = parsedate(datelist[i], &(current_rule->fromCondition))) < 0)
						{ // error
							return -106; // unknown datetype
						};
					};
					parse_state = 13;
					break;
				}
			case 13: // check for AT keyword
				{
					if(tline.front().compare("at"))
					{ // Did we forget to specify the from-condition-time?
						if(current_rule->conditionDurationType == 1)
						{
							if((!tline.front().compare("to"))||(!tline.front().compare("to!")))
							{ // from-event-time not specified deliberately, so assume 0000 hours
								list<string>::iterator itline;
								itline = tline.begin();
								tline.insert(itline, "at");
								tline.insert(itline, "0000");
							}
						}
						else if(current_rule->conditionDurationType == 2)
						{
							if(!tline.front().compare("#"))
							{
								list<string>::iterator itline;
								itline = tline.begin();
								tline.insert(itline, "at");
								tline.insert(itline, "0000");
							}
						}
						else
						{ // error
							return -123; // from-condition-time not specified
						}
					};
					parse_state = 14;
					break;
				}
			case 14: // process from-condition-time
				{
					vector<string> timelist;
					if(makeList(tline.front(), &timelist) < 0)
					{ // error
						return -124; // malformed time list
					};
					if((timelist.size() != 1)&&(timelist.size() != current_rule->fromCondition.DateTime.size()))
					{ // error
						return -125; // from-event-time must be 1 or equal to number of from-event-date items
					};
					if(timelist.size() == 1)
					{
						for(unsigned int i=0; i< current_rule->fromCondition.DateTime.size()-1; i++)
						{
							timelist.push_back(tline.front());
						}
					};
					int result;
					// size RSoffset to the same number of items as from-condition-date and init them to 0
					current_rule->fromCondition.RSoffset.resize(current_rule->fromCondition.DateTime.size(), 0);
					if((result = parsetime(&timelist,&current_rule->fromCondition)) < 0)
					{ // error
						return -126; // bad time format
					};
					if(current_rule->conditionDurationType == 1)
					{
						parse_state = 15; // Parse the TO part of the condition
					}
					else
					{ // There is no end to an ON condition
						tline.push_back("#"); // add a token to continue while loop
						parse_state = 80;
					};
					break;
				}
			case 15: // check for TO keyword
				if(!tline.front().compare("to"))
				{
					parse_state = 16;
				}
				else
				{ // error
					return -127; // unrecognized keyword
				};
				break;
			case 16: // parse to-condition-date
				{
					vector<string> datelist;
					if(makeList(tline.front(), &datelist) < 0)
					{ // error
						return -112; // malformed event list
					};
					if(datelist.size() != current_rule->fromCondition.DateTime.size())
					{ // error
						return -113; // number of TO dates must equal number of FROM dates
					};
					struct tm *tmdate;
					tmdate = new(struct tm);
					int datetype;
					for(unsigned int i=0; i<datelist.size(); i++)
					{
						tmcopy(&(current_rule->fromCondition.DateTime[i]), tmdate); // start with from-event-date
						tmdate->tm_isdst = -1;
						current_rule->toCondition.Time.push_back(mktime(tmdate));
						current_rule->toCondition.DateTime.push_back(*tmdate);
						current_rule->toCondition.DateType.push_back(0);
						current_rule->toCondition.HebDate.push_back(new Hdate());
						current_rule->toCondition.HebDate.back()->set_gdate(tmdate->tm_mday, tmdate->tm_mon+1, tmdate->tm_year+1900);
						if((datetype = parsedate(datelist[i], &(current_rule->toCondition))) < 0)
						{ // error
							return -114; // unknown datetype
						};
						if(datetype != current_rule->fromCondition.DateType[i])
						{ // error
							return -115; // TO datetype must equal FROM datetype
						};
					};
					parse_state = 17;
					break;
				}
			case 17: // check for AT keyword
				{
					if(tline.front().compare("at"))
					{ // Did we forget to specify the to-condition-time?
						// check if this token is a (forced) comment
// TODO: allow for the possibility of a WHEN or EXCEPT-WHEN condition following this one
						if(tline.front().compare("#"))
						{ // no. error
							return -132; // bad rule syntax
						};
						// yes. assume a to-condition-time of 2359
						list<string>::iterator itline;
						itline = tline.begin();
						tline.insert(itline, "at");
						tline.insert(itline, "2359");
					};
					parse_state = 18;
					break;
				}
			case 18: // parse to-condition-time
				{
					vector<string> timelist;
					if(makeList(tline.front(), &timelist) < 0)
					{ // error
						return -133; // malformed time list
					};
					if((timelist.size() != 1)&&(timelist.size() != current_rule->toCondition.DateTime.size()))
					{ // error
						return -134; // to-condition-time must be 1 or equal to number of to-condition-date items
					};
					if(timelist.size() == 1)
					{
						for(unsigned int i=0; i< current_rule->toCondition.DateTime.size()-1; i++)
						{
							timelist.push_back(tline.front());
						}
					};
					int result;
					// size RSoffset to the same number of items as to-event-date and init them to 0
					current_rule->toCondition.RSoffset.resize(current_rule->toCondition.DateTime.size(), 0);
					if((result = parsetime(&timelist,&current_rule->toCondition)) < 0)
					{ // error
						return -135; // bad time format
					};
					tline.push_back("#"); // add a token to continue while loop
					parse_state = 80;
					break;
				}
			case 20: // handle WHEN and EXCEPT-WHEN
				{
                    // Check for fully qualified sensor name in rule (e.g. bedroom.window_temperature)
                    size_t foundi = tline.front().find('_');
                    if(foundi == std::string::npos)
                    {
                        return -120; // Not a fully qualified sensor name
                    };
                    string cap = tline.front().substr(foundi+1); // Here's the capability
                    string sname = tline.front().substr(0, foundi); // Here's the name that should be in the sensor list
					bool founds = false;
					list<dvInfo>::iterator idv = srl.begin();
					while((idv != srl.end()) && !founds)
					{ // search the sensor list for it
						if(sname == (*idv).nodename)
						{ // found the sensor
						// TODO: Need a more elegant way to check that the sensor has the capability called out by the rule
                            current_rule->sensorIndex = -1;
                            if(!strcmp(cap.c_str(), "temperature") && (*idv).isTempSensor) current_rule->sensorIndex = 1;
                            if(!strcmp(cap.c_str(), "luminance") && (*idv).isLightSensor) current_rule->sensorIndex = 3;
                            if(!strcmp(cap.c_str(), "relativehumidity") && (*idv).isHumiditySensor) current_rule->sensorIndex = 5;
                            if(current_rule->sensorIndex == -1) return -120; // The sensor doesn't have the capability called out by the rule
                            else
                            {
                                current_rule->sensorNode = &(*idv);
                                current_rule->readinterval = 60;
                                current_rule->nextread = thistime;
                                founds = true;
                            }
						}
						else idv++;
					}
					if(!founds)
					{ // error
						return -121; // unrecognized keyword;
					};
					if(zwDebug) fprintf(zl, "%s\n",(current_rule->sensorNode)->nodename.c_str());
					parse_state = 21;
					break;
				}
			case 21: // Get sensor operation
				{
					if(sensoropmap.find(tline.front()) == sensoropmap.end())
					{ // error
						return -120; // unrecognized keyword;
					};
					int ct = sensoropmap [tline.front()];
					current_rule->sensorOp = ct;
					parse_state = 22;
					break;
				}
			case 22: // Get sensor value
				{
					if(is_number(tline.front()))
					{
						current_rule->sensorValue = atof(tline.front().c_str());
// TODO: allow for the possibility of an IF or EXCEPT-IF condition following this one
						tline.push_back("#"); // add a token to continue while loop
						parse_state = 80;
					}
					else
					{
						return -129; // Not a number
					};
					break;
				}
			case 80: // Store the rule
                rulelist.push_back(*current_rule);
				parse_state = 100;
				// reclaim storage before going on to the next rule
				current_rule->ruletext.clear();
				free(current_rule);
				tline.clear();
				break;
			}; // switch parse_state
			if(!tline.empty()) tline.pop_front(); // get next token
		}; // while !tline.empty
		if(parse_state != 100)
		{ // something went wrong in parsing the rule
			return -139;
		}
	}; // while rulefile.good
	rulefile.close();
	fprintf(zl, "\nRulelist contains %d rules\n", rulelist.size()); fflush(zl);
	if(zwDebug) for(list<zwRule>::iterator rl = rulelist.begin(); rl != rulelist.end(); rl++) printrule(&(*rl)); fflush(zl);
	return 0;
};

