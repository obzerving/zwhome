/*
 * zwRules_run.cpp
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#include "stdafx.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include "zwhome.h"

#define MONGOOSE
#ifdef MONGOOSE
#include "mongoose.h"
#endif

using namespace hdate;
using namespace std;

#define zwDebug 0

int zwRules::run(zwPrefs *zwP, zwDevices *zwD)
{
	if(getexecutemode())
	{ // We're controlling the devices for real
		if(!deviceinit)
		{
			// Start out getting reports every minute
			for(list<dvInfo>::iterator idv = srl.begin(); idv != srl.end(); idv++)
			{
				if((*idv).nodeId > 0)
				{ // zwave sensor ***Assumes AEOTEC multisenor)
                    // Ask for humidity, temperature, luminance, and battery reports to be sent automatically to group 1
                    Manager::Get()->SetConfigParam(zwD->g_homeId, (*idv).nodeId, 101, 225); // 11100001=1+32+64+128=225
                    Manager::Get()->SetConfigParam(zwD->g_homeId, (*idv).nodeId, 111, 60);
                    Manager::Get()->SetConfigParam(zwD->g_homeId, (*idv).nodeId, 4, 1); // Enable the motion sensor
				}
				else
				{ // wifisensor
					// Wifi sensors will be commanded to report every minute when they report in
				}
			}
			// Get initial wifiswitch state
			for(list<dvInfo>::iterator idv = dvl.begin(); idv != dvl.end(); idv++)
			{ // find a wifiswitch in the list
				if(!(*idv).restURL.empty())
				{ // Found one
                    char dvresp[128];
                    string scmd ((*idv).restURL);
                    scmd.append("/state"); // Ask for this variable to get current state of device
                    if(zwD->sendWifiCmd((*idv).restURL, dvresp))
                    {
                        char *sensorv;
                        sensorv = strtok(dvresp,"{\":}, ");
                        sensorv = strtok(NULL,":, ");
                        if(sensorv != NULL)
                        {
                            (*idv).reading.front().lastreading = atof(sensorv);
                        }
                    }
                    else fprintf(zl, "Failed to get initial state from %s\n", (*idv).nodename.c_str());
                }
            }
			deviceinit = true;
		}
	};
	// start the main loop
	bool running = true;
	float svalue;
	int iret = 0;
	time_t rawtime;
	struct tm *current;
	time_t looptime;
	int mincnt = 0; // This is the five minute counter
	srand(time(NULL));
	if(getexecutemode())
	{ // starting time is the current time
		time(&rawtime);
	}
	else
	{ // starting time is start time of the simulation
		rawtime = getsimstarttime();
		fprintf(zl, "Starting simulation time: %s\n",asctime(localtime(&rawtime)) ); fflush(zl);
	};

	while(running)
	{
		if(zwDebug) fprintf(zl, "Time: %s\n",asctime(localtime(&rawtime)) ); fflush(zl);
        bool result2;
        for(list<dvInfo>::iterator it=srl.begin(); it!=srl.end(); it++)
        {
            if(getexecutemode())
            {
                if(zwDebug){fprintf(zl,"zwRules_run->getSensorValue\n"); fflush(zl);}
                result2 = zwD->getSensorValue(&(*it), &svalue); // get a fresh reading every minute
                if(zwDebug){fprintf(zl, "zwRules_run->getSensorValue returns %d\n",result2); fflush(zl);}
            }
            else
            { // simulate readings with random points between min and max values
                for(list<readInfo>::iterator ri = (*it).reading.begin(); ri != (*it).reading.end(); ri++)
                {
                    float fvalue = (*ri).min + rand() % ((*ri).max - (*ri).min + 1);
                    bool foundri = false;
                    int dp = 0; // find the right slot to put this datapoint
                    while(!foundri && (dp < 10))
                    {
                        if((*ri).points[dp] == -100)
                        { // Here's an empty slot to put it
                            (*ri).points[dp] = fvalue;
                            dp++;
                            foundri = true;
                        }
                        else dp++;
                    }
                    if(!foundri)
                    { // No empty slots. Slide the data points
                        for(int j=0; j<dp-1; j++)(*ri).points[j] = (*ri).points[j+1];
                        (*ri).points[dp-1] = fvalue;
                        foundri = true;
                    }
                    float summit = 0.0; // compute the average. dp tells us how many data points to use
                    for(int j=0; j<dp; j++) summit = summit + (*ri).points[j];
                    (*ri).lastreading = summit/dp;
                }
            }
        }
		if(strcmp(zwR->sys_stat, "on")==0)
		{
			current = localtime(&rawtime );
			datestruct currentds;
			currentds.DateTime.push_back(*current);
			currentds.Time.push_back(rawtime);
			currentds.DateType.push_back(0);
			currentds.TimeType.push_back(0);
			looptime = rawtime + 60; // we want to go through this loop every minute
			zwR->sys_pause = false;
            if(!rulelist.empty()) // Skip rule processing if there are no rules.
            {
			for(list<zwRule>::iterator itr=rulelist.begin(); itr!=rulelist.end(); itr++)
			{
				bool process_rule;
				// First, see if the rule has fired.
				if((*itr).firing < 0)
				{	// Rule hasn't fired. Check for conditions under which it would fire.
					// If it meets the conditions, then we'll see process the rule
					switch((*itr).conditionType)
					{
					case 0: // No Conditions
						process_rule = true;
						break;
					case 1: // IF
						{
							int cs = (*itr).fromCondition.Time.size();
							int i = 0;
							process_rule = false; // Here we assume we don't process the rule
							bool met_condition = false;
							while((i < cs) && !met_condition) // Check each condition until we satisfy one
							{
								switch((*itr).conditionDurationType) // what type of condition is it?
								{
								case 1: //FROM-TO
									{
										if(rawtime > (*itr).toCondition.Time[i]) // do we need to update the condition?
										{
											// yes
											update(&((*itr).fromCondition), i);
											update(&((*itr).toCondition),  i);
										};
										// Are we within the condition’s time window?
										if((rawtime >= (*itr).fromCondition.Time[i]) && (rawtime <= (*itr).toCondition.Time[i]))
										{
											// yes, process the rule
											// Note that there might be other conditions that are satisfied or that need to be updated, but
											// we don't care after we meet the first condition
											process_rule = true;
											met_condition = true;
										}
										break;
									}
								case 2: // ON
									{
										process_rule = false;
										if(rawtime >= (*itr).fromCondition.Time[i])
										{
											// yes, process the rule
											process_rule = true;
											met_condition = true;
										}
										break;
									}
								}; // switch conditionDurationType
								i++;
							};
						}
						break;
					case 2: // EXCEPT-IF
						{
							int cs = (*itr).fromCondition.Time.size();
							int i = 0;
							process_rule = true; // assume we process the rule
							bool met_condition = false;
							while((i < cs) && !met_condition)
							{
								switch((*itr).conditionDurationType) // what type of condition is it?
								{
								case 1: //FROM-TO
									{
										if(rawtime > (*itr).toCondition.Time[i]) // do we need to update the condition?
										{
											// yes
											update(&((*itr).fromCondition), i);
											update(&((*itr).toCondition), i);
										};
										// Are we within the condition’s time window?
										if((rawtime >= (*itr).fromCondition.Time[i]) && (rawtime <= (*itr).toCondition.Time[i]))
										{
											// yes, skip the rule
											// Note that there might be other conditions that are satisfied or that need to be updated, but
											// we don't care after we meet the first condition
											met_condition = true;
											process_rule = false;
										}
										break;
									}
								case 2: // ON
									{
										if(rawtime >= (*itr).fromCondition.Time[i])
										{
											// yes, skip the rule
											process_rule = false;
											met_condition = true;
										}
										break;
									}
								}; // switch conditionDurationType
								i++;
							};
						}
						break;
					case 3: // WHEN. We really check this while processing the rule
						process_rule = true;
						break;
					case 4: // EXCEPT-WHEN. We really check this while processing the rule
						process_rule = true;
						break;
					default: // Just forget about processing the rule
						process_rule = false;
						break;
					};
				} // IF rule hasn't fired
				else
				{ // Rule has fired, so skip checking for conditions
					process_rule = true;
				};
				if(process_rule)
				{	// We're here because either (1) the rule hasn't fired and we've met the conditions or
					// (2) the rule is firing and we're checking whether to turn it off
					switch ((*itr).eventDurationType) // what type of rule is it?
					{
					case 1: // FROM-TO rule
						{
							if((*itr).firing < 0) // Is the rule firing?
							{ // No. See if we should fire this rule
								int i = 0;
								int cs = (*itr).fromEvent.Time.size();
								while ((i < cs) && ((*itr).firing < 0))
								{
									if(rawtime > (*itr).toEvent.Time[i]) // do we need to update the event?
									{
										// yes. We passed the event window
										update(&((*itr).fromEvent), i);
										update(&((*itr).toEvent), i);
									};
									if((rawtime >= (*itr).fromEvent.Time[i]) && (rawtime <= (*itr).toEvent.Time[i]))
									{
										// If this depends on a sensor, make sure it's reporting
										if(((*itr).conditionType == 3) || ((*itr).conditionType == 4))
										{
											uint8 usevalue = (*itr).sensorNode->in_use;
											if(usevalue == 0)
											{ // No other rule is using this sensor, so get an immediate reading
												(*itr).sensorNode->in_use = 1;
											}
											else (*itr).sensorNode->in_use = ++usevalue; // More than one rule using this sensor
										}
										else
										{
											// Do the actions
											long iid = itemIDgen();
                                            if(zwDebug){fprintf(zl,"Queue from-to item %ld\n", iid); fflush(zl);}
											q_action((*itr).on_action, (*itr).fromEvent.Time[i], (*itr).device, (*itr).enforce_from, iid);
											q_action((*itr).off_action, (*itr).toEvent.Time[i], (*itr).device, (*itr).enforce_to, iid);
                                            if(zwDebug){fprintf(zl,"Queing complete\n"); fflush(zl);}
										};
										(*itr).firing = i; // Keep track of which clause in rule is firing
									}
									i++;
								};
							}
							else
							{ // Rule is firing
								if(rawtime >= (*itr).toEvent.Time[(*itr).firing]) // Have we reached the end of the event? --was >
								{
									// yes. We passed the event window
									// If this depends on a sensor, mark that this rule doesn't need it anymore
									if(((*itr).conditionType == 3) || ((*itr).conditionType == 4))
									{
										// Then Update the sensor's use
										uint8 usevalue = (*itr).sensorNode->in_use;
										if(usevalue > 0) (*itr).sensorNode->in_use = --usevalue;
									}
									update(&((*itr).fromEvent), (*itr).firing);
									update(&((*itr).toEvent), (*itr).firing);
									(*itr).firing = -1;
									// Note that we haven’t queued up any off-actions. We want the device(s) controlled by a sensor to
									// remain in the current state when the event ends
								}
								else
								{ // If this rule depends on a sensor, check it to see what action to perform
									switch((*itr).conditionType)
									{
									case 3: // When
									case 4: // Except-When
										{
                                            bool idone = false; // True when svalue loaded with last sensor reading
											list<readInfo>::iterator ri = (*itr).sensorNode->reading.begin();
                                            if(((*itr).readinterval > 0) && (rawtime > (*itr).nextread)) // Time to get next reading?
                                            { // Yes. Find the right dataset (might be a multisensor)
                                                while((ri != (*itr).sensorNode->reading.end()) && !idone)
                                                {
                                                    if((*ri).typeindex == (*itr).sensorIndex)
                                                    { // Found the right dataset
                                                        svalue = (*ri).lastreading;
                                                        current->tm_sec += (*itr).readinterval;
                                                        (*itr).nextread = mktime(current);
                                                        idone = true;
                                                    }
                                                    else ri++;
                                                }
                                                if(idone)
                                                { // Found the dataset
                                                    if(svalue != -100.0)
                                                    { // We have a valid reading
                                                        if(!getexecutemode()) fprintf(zl, "Sensor %s reading: %.2f\n", (*itr).sensorNode->nodename.c_str(), svalue);
												        if(opeval(svalue, (*itr).sensorOp, (*itr).sensorValue))
												        { // Met the condition
													        if((*itr).conditionType == 3)
													        { // For a WHEN condition, turn on the device(s)
														        q_action((*itr).on_action, currentds.Time[0], (*itr).device, (*itr).enforce_from, itemIDgen());
                                                                (*itr).readinterval = 1800;
                                                                struct tm * tp = localtime(&rawtime);
                                                                tp->tm_sec += (*itr).readinterval;
                                                                (*itr).nextread = mktime(tp);
													        }
													        else
													        { // For an EXCEPT-WHEN condition, turn off the device(s)
														        q_action((*itr).off_action, currentds.Time[0], (*itr).device, (*itr).enforce_to, itemIDgen());
                                                                (*itr).readinterval = 600;
                                                                struct tm * tp = localtime(&rawtime);
                                                                tp->tm_sec += (*itr).readinterval;
                                                                (*itr).nextread = mktime(tp);
													        }
												        }
												        else
												        { // Didn't meet the condition
													        if((*itr).conditionType == 4)
													        { // For an EXCEPT-WHEN condition, turn on the device(s)
														        q_action((*itr).on_action, currentds.Time[0], (*itr).device, (*itr).enforce_from, itemIDgen());
                                                                (*itr).readinterval = 1800;
                                                                struct tm * tp = localtime(&rawtime);
                                                                tp->tm_sec += (*itr).readinterval;
                                                                (*itr).nextread = mktime(tp);
													        }
													        else
													        { // For a WHEN condition, turn off the device(s)
														        q_action((*itr).off_action, currentds.Time[0], (*itr).device, (*itr).enforce_to, itemIDgen());
                                                                (*itr).readinterval = 600;
                                                                struct tm * tp = localtime(&rawtime);
                                                                tp->tm_sec += (*itr).readinterval;
                                                                (*itr).nextread = mktime(tp);
													        }
												        }
                                                    }
                                                    else
                                                    { // Either the sensor hasn't reported in yet or it's dead. Here's how we handle it.
                                                        if((*itr).conditionType == 4)
                                                        { // For an EXCEPT-WHEN condition, turn on the device(s)
                                                            q_action((*itr).on_action, currentds.Time[0], (*itr).device, (*itr).enforce_from, itemIDgen());
                                                            (*itr).readinterval = 1800;
                                                            struct tm * tp = localtime(&rawtime);
                                                            tp->tm_sec += (*itr).readinterval;
                                                            (*itr).nextread = mktime(tp);
                                                        }
                                                        else
                                                        { // For a WHEN condition, turn off the device(s)
                                                            q_action((*itr).off_action, currentds.Time[0], (*itr).device, (*itr).enforce_to, itemIDgen());
                                                            (*itr).readinterval = 600;
                                                            struct tm * tp = localtime(&rawtime);
                                                            tp->tm_sec += (*itr).readinterval;
                                                            (*itr).nextread = mktime(tp);
                                                        }

                                                    }
                                                }
                                                else
                                                { // didn't find the dataset
                                                    fprintf(zl, "Did not find dataset index %d for sensor %s. Ignoring rule\n%s\n",(*itr).sensorIndex,(*itr).sensorNode->nodename.c_str(),(*itr).ruletext.c_str());
                                                }
                                            }
											break;
										}
									default:
										{
											break;
										}
									} // switch
								}
							}
						};
						break;
					case 2: // ON rule
						{
							if((*itr).firing < 0) // Is the rule firing?
							{ // No. See if we should fire this rule
								int i = 0;
								int cs = (*itr).fromEvent.Time.size();
								while ((i < cs) && ((*itr).firing < 0))
								{
									if(rawtime >= (*itr).fromEvent.Time[i])
									{ // Reached (or passed) ON time
										// Issue an on_action, but there won't be an off_action
										q_action((*itr).on_action, (*itr).fromEvent.Time[i], (*itr).device, (*itr).enforce_from, itemIDgen());
										// Instead, update the rule to the next time
										update(&((*itr).fromEvent), i);
									};
									i++;
								}
							}
							break;
						}
					default:
						break;
					} // switch
				}
			} // for iterator
			}; // if empty rulelist
			if(!q_list.empty())
			{
				int popcnt = 0;
				for(list<q_item>::iterator itq=q_list.begin(); itq != q_list.end(); itq++)
				{
					if(rawtime >= itq->acttime)
					{
						if(getexecutemode())
						{
                            if(zwD->doAction(itq->action, itq->dv, itq->enforce) != 0)
                            {
                                zwD->wrapup();
                                return -170;
                            }
						}
						else // simulate it
						{
							string tstrz(asctime(&currentds.DateTime.back()));
							tstrz.resize(tstrz.length() -1);
                            for(list<string>::iterator sni = itq->dv.begin(); sni != itq->dv.end(); sni++)
                            {
                                for(list<dvInfo>::iterator ni = dvl.begin(); ni != dvl.end(); ni++)
                                {
                                    if((*sni) == (*ni).nodename)
                                    {
                                        if((itq->action > 99) && (itq->action < 255))
                                        {
                                            fprintf(zl, "%s: Remove enforcement from Node %s\n", tstrz.c_str(), (*sni).c_str());
                                            (*ni).enforce = false;
                                        }
                                        else
                                        {
                                            if(itq->action != ((int) (*ni).reading.front().lastreading))
                                            {
                                                fprintf(zl, "%s: Turn %s %s%s", tstrz.c_str(), (itq->action==0)?"off":"on",(*ni).nodename.c_str(), itq->enforce?" and enforce it.\n":".\n");
                                            }
                                            else
                                            {
                                                fprintf(zl, "%s: %s already %s%s", tstrz.c_str(), (*ni).nodename.c_str(), (itq->action==0)?"off":"on", itq->enforce?" and being enforced.\n":".\n");
                                            }
                                            fflush(zl);
                                            (*ni).desired_state = itq->action;
                                            (*ni).reading.front().lastreading = float(itq->action);
                                            (*ni).desired_state_timestamp = rawtime;
                                            (*ni).enforce = itq->enforce;
                                        }
                                    }
                                }
							}
						};
						popcnt++;
					}
					else break;
				}; // for itq...
				if(popcnt > 0)
				{
					for(int i=0; i<popcnt; i++) q_list.pop_front();
				};
			};
			while (looptime >rawtime)
			{
				if(getexecutemode())
				{
					sleep(1);
					time(&rawtime);
				} // if getexecutemode
				else
				{
					rawtime = looptime;
                    if(rawtime >= getsimendtime())
					{
						return 0; // end of simulation
					}
				};
			} // while looptime > rawtime

			if(++mincnt == 5) // Has it been five minutes?
			{ // Yes. Check nodes for enforced states
				for(list<dvInfo>::iterator idv = dvl.begin(); idv != dvl.end(); idv++)
				{ // for each device
                    if((*idv).nodeId > 0)
                    {
                        if(((*idv).bccmap == 37) || ((*idv).bccmap == 38)) // We only care about switches and dimmers for now
                        {
                            if(getexecutemode())
                            {
                                bool foundgn = false;
                                pthread_mutex_lock( &(zwD->g_criticalSection) );
                                list<NodeInfo*>::iterator it = zwD->g_nodes.begin();
                                while ((it != zwD->g_nodes.end()) && !foundgn)
                                { // find it in the node list
                                    NodeInfo* nodeInfo = *it;
                                    if( ( nodeInfo->m_homeId == zwD->g_homeId ) && ( nodeInfo->m_nodeId == (*idv).nodeId ) )
                                    { // this is the node. Find the right value
                                        foundgn = true;
                                        bool foundv = false;
                                        list<ValueID>::iterator itv = nodeInfo->m_values.begin();
                                        while ((itv != nodeInfo->m_values.end()) && !foundv)
                                        { // check each value in the device for the one that changes its state
                                            if((*itv).GetCommandClassId() == (*idv).bccmap)
                                            {
                                                foundv = true; // There should only be one value for this CC, so it must be the right one (right?)
                                                Manager::Get()->RefreshValue((*itv)); // Refresh value from network
                                                if((*idv).enforce)
                                                {
                                                    if((*idv).desired_state_timestamp < rawtime)
                                                    {
                                                        list<string> dvnode;
                                                        dvnode.push_back((*idv).nodename);
                                                        zwD->doAction((*idv).desired_state, dvnode, true);
                                                    }
                                                } // (*idv).enforce
                                            }
                                            itv++;
                                        }
                                        if(!foundv) fprintf(zl, "Did not find the right command class for this node\n");
                                    }
                                    it++;
                                }
                                pthread_mutex_unlock( &(zwD->g_criticalSection) );
                                if(!foundgn) fprintf(zl, "Node not found in g_nodes\n");
                            }
                            else
                            { // Simulating
                                if((*idv).enforce)
                                {
                                    if((*idv).desired_state_timestamp < rawtime)
                                    {
                                        string tstrz(asctime(&currentds.DateTime.back()));
                                        tstrz.resize(tstrz.length() -1);
                                    }
                                }
                            }
                        } // bccmap = 37 or 38
                    }
                    else
                    { // This is a wifi device
                        if((*idv).enforce)
                        {
                            if((*idv).desired_state_timestamp < rawtime)
                            {
                                list<string> dvnode;
                                dvnode.push_back((*idv).nodename);
                                if(getexecutemode())
                                {
                                    zwD->doAction((*idv).desired_state, dvnode, true);
                                }
                                else
                                {
                                    string tstrz(asctime(&currentds.DateTime.back()));
                                    tstrz.resize(tstrz.length() -1);
                                }

                            }
                        } // (*idv).enforce
                    }
				} // for...
                mincnt = 0;
			} // if(++mincnt...
		} // if(sys_stat = on)
		else
		{
			if(!zwR->sys_pause) // Pause the rules
			{ // Things to do upon pausing the rules
				while(!q_list.empty()) q_list.pop_front(); // empty the action queue
				for(list<zwRule>::iterator itr=rulelist.begin(); itr!=rulelist.end(); itr++) (*itr).firing = -1; // reset all rules
				zwR->sys_pause = true;
			}
		}
		fflush(zl);
	}; // while running
	return(iret);
};

