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
		{ // We have to init the device driver first
			if(zwD->init()!=0)
			{
				return -600; // couldn't init devices
			};
			// Start out not getting any reports from sensors (Do we need to do anything for wifi sensors?)
// TODO: fix this mess. We don't want to set config params multiple times for one node
			for(list<dvInfo>::iterator idv = srl.begin(); idv != srl.end(); idv++)
			{
				if((*idv).nodeId > 0)
				{ // zwave device
					Manager::Get()->SetConfigParam(zwD->g_homeId, (*idv).nodeId, 101, 0);
				}
				else
				{ // wifi device
					(*idv).readinterval = 0;
				}
			}
			deviceinit = true;
		}
	};
	// start the main loop
	bool running = true;
//	bool useLastReading; // We use this flag to determine whether to use the last reading of a sensor
	float svalue;
	int iret = 0;
	time_t rawtime;
	struct tm *current;
	time_t looptime;
	int mincnt = 0; // This is the five minute counter
	int simsensorcnt = 0; // This is used to simulate the sensor reporting every ten minutes
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
		if(strcmp(zwR->sys_stat, "on")==0)
		{
			list<dvInfo>::iterator idv = srl.begin();
			while(idv != srl.end())
			{ // clear the last reading of every sensor before going through the rule loop
				(*idv).lastreading = -1.0;
				idv++;
			}
			if(++simsensorcnt > 10) simsensorcnt = 1;
			current = localtime(&rawtime );
			datestruct currentds;
			currentds.DateTime.push_back(*current);
			currentds.Time.push_back(rawtime);
			currentds.DateType.push_back(0);
			currentds.TimeType.push_back(0);
			looptime = rawtime + 60; // we want to go through this loop every minute
			zwR->sys_pause = false;
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
											{ // No other rule is using this sensor, so start it up
												if(getexecutemode())
												{
													if((*itr).sensorNode->nodeId > 0)
													{
														// Ask for humidity, temperature, luminance, and battery reports to be sent automatically to group 1
														Manager::Get()->SetConfigParam(zwD->g_homeId, (*itr).sensorNode->nodeId, 101, 225); // 11100001=1+32+64+128=225
														Manager::Get()->SetConfigParam(zwD->g_homeId, (*itr).sensorNode->nodeId, 111, 600);  // Send the reports every 10 minutes
														Manager::Get()->SetConfigParam(zwD->g_homeId, (*itr).sensorNode->nodeId, 4, 1); // Enable the motion sensor
													}
													else
													{ // Set the time interval and timestamp for the wifi sensor
														(*itr).sensorNode->readinterval = 600;
														(*itr).sensorNode->nextread = time(NULL);
													}
												};
												(*itr).sensorNode->in_use = 1;
											}
											else (*itr).sensorNode->in_use = ++usevalue; // More than one rule using this sensor
										}
										else
										{
											// Do the actions
											long iid = itemIDgen();
											q_action((*itr).on_action, (*itr).fromEvent.Time[i], (*itr).device, (*itr).enforce_from, iid);
											q_action((*itr).off_action, (*itr).toEvent.Time[i], (*itr).device, (*itr).enforce_to, iid);
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
										if(usevalue == 0)
										{ // No other rule using this sensor, so stop sending reports
											if(getexecutemode())
											{
												if((*itr).sensorNode->nodeId > 0) // for zwave sensor
												{
													Manager::Get()->SetConfigParam(zwD->g_homeId, (*itr).sensorNode->nodeId, 101, 0);
												}
												else
												{ // for wifi sensor
													(*itr).sensorNode->readinterval = 0; // This stops the reports
													(*itr).sensorNode->lastreading = -1.0; // This forces a new report. Here's why:
													// If hours passed between the last reading before turning off reports, the
													// reading might be totally inaccurate. Better to get an updated reading.
												}
											}
										}
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
											bool result = false;
											if(getexecutemode())
											{
												result = zwD->getSensorValue((*itr).sensorNode, &svalue);
												if(result) // We have a valid report
												{
													(*itr).sensorNode->lastreading = svalue;
													// If we didn't do this, we would get a sensor report for one rule, but not for
													// the other rules in the loop. Lastreading is -1.0 when we turn on the sensor
													// until it issues its first report
												}
												else // Didn't get a report but...
												{
													if((*itr).sensorNode->lastreading > -1.0)
													{ // We can use the last reading for the rest of the rules
														svalue = (*itr).sensorNode->lastreading;
														result = true;
													}
												}
											}
											else // We're simulating the sensor
											{
												if(simsensorcnt == 10)
												{
													if(svalue < 0.0) svalue = (float)(rand() % 2000);
													result = true;
												}
												else
												{
													result = false;
												}
											}
											if(result)
											{
												if(opeval(svalue, (*itr).sensorOp, (*itr).sensorValue))
												{ // Met the condition
													if((*itr).conditionType == 3)
													{ // For a WHEN condition, turn on the device(s)
														q_action((*itr).on_action, currentds.Time[0], (*itr).device, (*itr).enforce_from, itemIDgen());
														if(getexecutemode())
														{
															if((*itr).sensorNode->nodeId > 0) // for zwave sensor
																Manager::Get()->SetConfigParam(zwD->g_homeId, (*itr).sensorNode->nodeId, 111, 1800);  // Send the reports every 30 minutes
															else // for wifi sensor
															{
																(*itr).sensorNode->readinterval = 1800;
																struct tm * tp = localtime(&rawtime);
																tp->tm_sec += (*itr).sensorNode->readinterval;
																(*itr).sensorNode->nextread = mktime(tp);
															}
														}
													}
													else
													{ // For an EXCEPT-WHEN condition, turn off the device(s)
														q_action((*itr).off_action, currentds.Time[0], (*itr).device, (*itr).enforce_to, itemIDgen());
														if(getexecutemode())
														{
															if((*itr).sensorNode->nodeId > 0) // for zwave sensor
																Manager::Get()->SetConfigParam(zwD->g_homeId, (*itr).sensorNode->nodeId, 111, 600);  // Send the reports every 10 minutes
															else // for wifi sensor
															{
																(*itr).sensorNode->readinterval = 600;
																struct tm * tp = localtime(&rawtime);
																tp->tm_sec += (*itr).sensorNode->readinterval;
																(*itr).sensorNode->nextread = mktime(tp);
															}
														}
													}
												}
												else
												{ // Didn't meet the condition
													if((*itr).conditionType == 4)
													{ // For an EXCEPT-WHEN condition, turn on the device(s)
														q_action((*itr).on_action, currentds.Time[0], (*itr).device, (*itr).enforce_from, itemIDgen());
														if(getexecutemode())
														{
															if((*itr).sensorNode->nodeId > 0) // for zwave sensor
																Manager::Get()->SetConfigParam(zwD->g_homeId, (*itr).sensorNode->nodeId, 111, 1800);  // Send the reports every 30 minutes
															else // for wifi sensor
															{
																(*itr).sensorNode->readinterval = 1800;
																struct tm * tp = localtime(&rawtime);
																tp->tm_sec += (*itr).sensorNode->readinterval;
																(*itr).sensorNode->nextread = mktime(tp);
															}
														}
													}
													else
													{ // For a WHEN condition, turn off the device(s)
														q_action((*itr).off_action, currentds.Time[0], (*itr).device, (*itr).enforce_to, itemIDgen());
														if(getexecutemode())
														{
															if((*itr).sensorNode->nodeId > 0) // for zwave sensor
																Manager::Get()->SetConfigParam(zwD->g_homeId, (*itr).sensorNode->nodeId, 111, 600);  // Send the reports every 10 minutes
															else // for wifi sensor
															{
																(*itr).sensorNode->readinterval = 600;
																struct tm * tp = localtime(&rawtime);
																tp->tm_sec += (*itr).sensorNode->readinterval;
																(*itr).sensorNode->nextread = mktime(tp);
															}
														}
													}
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
			}; // for iterator
			if(!q_list.empty())
			{
				int popcnt = 0;
				for(list<q_item>::iterator itq=q_list.begin(); itq != q_list.end(); itq++)
				{
					if(rawtime >= itq->acttime)
					{
						if(getexecutemode())
						{
							if(itq->action > 99)
							{ // We leave the device in its current state and just turn off enforcement
								for(vector<dvInfo>::iterator ni = itq->dv.begin(); ni != itq->dv.end(); ni++)
								{ // For each device
									list<NodeInfo*>::iterator it = zwD->g_nodes.begin();
									while (it != zwD->g_nodes.end())
									{ // find it in the node list
										NodeInfo* nodeInfo = *it;
										if((*it)->m_nodeId == (*ni).nodeId)
										{ // Found the device
											list<ValueID>::iterator itv = nodeInfo->m_values.begin();
											list<valInfo>::iterator ivv = nodeInfo->v_info.begin();
											while (itv != nodeInfo->m_values.end())
											{ // check each value in the device for the one that changes its state
												if((*itv) == (*ni).vId.front())
												{
													(*ivv).enforce = false; // Turn off enforcement
													fprintf(zl, "Remove enforcement: Node = %s\n", ((*ni).nodename).c_str()); fflush(zl);
												}
												ivv++;
												itv++;
											}
										}
										it++;
									}
								}
							}
							else
								if(zwD->doAction(itq->action, zwD->g_homeId, (vector<dvInfo>) itq->dv, itq->enforce) != 0)
								{
									zwD->wrapup();
									return -170;
								}
						}
						else // simulate it
						{
							string tstrz(asctime(&currentds.DateTime.back()));
							tstrz.resize(tstrz.length() -1);
							if(itq->action > 99)
							{
								fprintf(zl, "%s: Removing enforcement from Node(s)", tstrz.c_str());
								for(vector<dvInfo>::iterator ni = itq->dv.begin(); ni != itq->dv.end(); ni++)
								{
									fprintf(zl, " %s\n",(*ni).nodename.c_str()); fflush(zl);
									itq->enforce = false;
								}
							}
							else
							{
								fprintf(zl, "%s: Do action %d on Node(s)", tstrz.c_str(), itq->action);
								for(vector<dvInfo>::iterator ni = itq->dv.begin(); ni != itq->dv.end(); ni++) fprintf(zl, " %s",(*ni).nodename.c_str());
								fprintf(zl, "%s\n", itq->enforce?" and enforce it.\n":".\n"); fflush(zl);
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

#ifdef WIN32
					Sleep(1000);
#endif
#ifdef linux
					sleep(1);
#endif
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
					list<NodeInfo*>::iterator it = zwD->g_nodes.begin();
					while (it != zwD->g_nodes.end())
					{ // find it in the node list
						NodeInfo* nodeInfo = *it;
						list<ValueID>::iterator itv = nodeInfo->m_values.begin();
						list<valInfo>::iterator ivv = nodeInfo->v_info.begin();
						while (itv != nodeInfo->m_values.end())
						{ // check each value in the device for the one that changes its state
							if((*itv) == (*idv).vId.front())
							{
								Manager::Get()->RefreshValue((*itv)); // Refresh value from network
								if((*ivv).enforce)
								{
									if((*ivv).desired_state_timestamp < time(NULL))
									{
										vector<dvInfo> dvnode;
										dvnode.push_back(*idv);
										if(getexecutemode())
										{
											zwD->doAction((*ivv).desired_state, nodeInfo->m_homeId, dvnode, true);
										}
										else
										{
											string tstrz(asctime(&currentds.DateTime.back()));
											tstrz.resize(tstrz.length() -1);
											fprintf(zl, "\n%s: Enforcing action %d on %s\n", tstrz.c_str(), (*ivv).desired_state,(*idv).nodename.c_str()); fflush(zl);
										}

									}
								} // (*ivv).enforce
							}
							itv++;
							ivv++;
						}
						it++;
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
