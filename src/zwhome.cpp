/*
 * zwhome.cpp
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#include "stdafx.h"
#include <time.h>
#include <sys/types.h>
#include <iostream>
#include <sys/stat.h>
#undef UNICODE
#include "Options.h"
#include "Manager.h"
#include "Node.h"
#include "Group.h"
#include "ValueStore.h"
#include "Value.h"
#include "ValueBool.h"
#include "Log.h"
#include "zwhome.h"
using namespace OpenZWave;
using namespace std;

#define zwDebug 0

#define MONGOOSE
#ifdef MONGOOSE
#include "mongoose.h"
#endif

zwPrefs *zwP;
zwRules *zwR;
zwDevices *zwD;
FILE *zl; // Log file

#ifdef MONGOOSE
/* The API
	http://zwhomeserver:8000/devices/switches/[<nodename>, all-lights]?state=[on,off]
	http://zwhomeserver:8000/devices/dimmers/<nodename>?state=[on,off,${intensity.percent}]
	http://zwhomeserver:8000/system?state=[on,off]
*/
char switchcat[20] = {"/devices/switches"};
char dimmercat[20] = {"/devices/dimmers"};
char systemcat[20] = {"/system"};

static int event_handler(struct mg_connection *conn, enum mg_event ev) {
	switch (ev)
	{
		case MG_AUTH: return MG_TRUE;
		case MG_REQUEST:
			fprintf(zl,"Request=%s, URI=%s, Query=%s\n", conn->request_method, conn->uri, conn->query_string); fflush(zl);
			if(conn->content_len > 0)
			{
				string cbuff = string(conn->content, conn->content_len);
				fprintf(zl,"content =%s\n", cbuff.c_str()); fflush(zl);
			}
			if(!strncmp(conn->uri, systemcat, strlen(systemcat)))
			{
				char sysstate[4];
				mg_get_var(conn, "state", sysstate, sizeof(sysstate));
				if(!strcmp(sysstate, "on")) strcpy(zwR->sys_stat,"on");
				if(!strcmp(sysstate, "off")) strcpy(zwR->sys_stat,"off");
				mg_printf_data(conn, "Turning System %s\n", sysstate);
			}
			else if (!strncmp(conn->uri, switchcat, strlen(switchcat)))
			{
#ifdef WIN32
				EnterCriticalSection( &(zwD->g_criticalSection) );
#endif
#ifdef linux
				pthread_mutex_lock( &(zwD->g_criticalSection) );
#endif
				bool idone = false;
				char switchname[100];
				strcpy(switchname, conn->uri);
				char *key;
				key = strrchr(switchname, '/'); // This should be the nodename
				if(key == NULL) return MG_FALSE;
				key++; // Get rid of leading ‘/’
				char swstate[4];
				mg_get_var(conn, "state",swstate, sizeof(swstate));
				list<dvInfo>::iterator idv = zwR->dvl.begin();
				while(idv != zwR->dvl.end() && !idone)
				{
					if(!strcmp((*idv).nodename.c_str(), key) || ((*idv).isLight && !strcmp("all-lights", key)))
					{
						vector<dvInfo> vbtn;
						vbtn.push_back(*idv);
						if((strcmp(swstate, "on") == 0))
						{
							zwD->doAction(1, zwD->g_homeId, vbtn, false);
							mg_printf_data(conn, "Turning %s %s\n", key, swstate);
						}
						else if((strcmp(swstate, "off") == 0))
						{
							zwD->doAction(0, zwD->g_homeId, vbtn, false);
							mg_printf_data(conn, "Turning %s %s\n", key, swstate);
						}
						if(strcmp("all-lights", key) != 0)idone = true; // We only need to do one switch
					}
					idv++;
				} // while
#ifdef WIN32
				LeaveCriticalSection( &(zwD->g_criticalSection) );
#endif
#ifdef linux
				pthread_mutex_unlock( &(zwD->g_criticalSection) );
#endif
				return MG_TRUE;
			} // if (!strncmp(conn->uri, switchcat, strlen(switchcat))
			else if (!strncmp(conn->uri, dimmercat, strlen(dimmercat)))
			{
#ifdef WIN32
				EnterCriticalSection( &(zwD->g_criticalSection) );
#endif
#ifdef linux
				pthread_mutex_lock( &(zwD->g_criticalSection) );
#endif
				bool idone = false;
				char dimmername[100];
				strcpy(dimmername, conn->uri);
				char *key;
				key = strrchr(dimmername, '/'); // This should be the nodename
				if(key == NULL) return MG_FALSE;
				key++; // Get rid of leading ‘/’
				char dimmerstate[4];
				mg_get_var(conn, "state",dimmerstate, sizeof(dimmerstate));
				list<dvInfo>::iterator idv = zwR->dvl.begin();
				while(idv != zwR->dvl.end() && !idone)
				{
// TODO: Implement dimmer logic
					if(!strcmp((*idv).nodename.c_str(), key))
					{
						vector<dvInfo> vbtn;
						vbtn.push_back(*idv);
						if((strcmp(dimmerstate, "on") == 0))
						{
							zwD->doAction(1, zwD->g_homeId, vbtn, false);
							mg_printf_data(conn, "Turning %s %s\n", key, dimmerstate);
						}
						else if((strcmp(dimmerstate, "off") == 0))
						{
							zwD->doAction(0, zwD->g_homeId, vbtn, false);
							mg_printf_data(conn, "Turning %s %s\n", key, dimmerstate);
						}
						idone = true;
					}
					idv++;
				} // while
#ifdef WIN32
				LeaveCriticalSection( &(zwD->g_criticalSection) );
#endif
#ifdef linux
				pthread_mutex_unlock( &(zwD->g_criticalSection) );
#endif
				return MG_TRUE;
			} // if (!strncmp(conn->uri, switchcat, strlen(switchcat))
			break;
		default: return MG_FALSE;
	}
	return 0; // Just here to satisfy the compiler
}

static void* serve(void *server)
    	{
    		fprintf(zl,"Starting server poll\n"); fflush(zl);
    		for(;;) mg_poll_server((struct mg_server *) server, 1000);
    		return NULL;
    	}
#endif

NodeInfo* GetNodeInfo(Notification const* _notification)
{
	uint32 const homeId = _notification->GetHomeId();
	uint8 const nodeId = _notification->GetNodeId();
	for( list<NodeInfo*>::iterator it = zwD->g_nodes.begin(); it != zwD->g_nodes.end(); ++it )
	{
		NodeInfo* nodeInfo = *it;
		if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
		{
			return nodeInfo;
		}
	}
	return NULL;
};

void OnNotification(Notification const* _notification, void* _context)
{
	// Must do this inside a critical section to avoid conflicts with the main thread
#ifdef WIN32
	EnterCriticalSection( &(zwD->g_criticalSection) );
#endif
#ifdef linux
	pthread_mutex_lock( &(zwD->g_criticalSection) );
#endif
	switch( _notification->GetType() )
	{
		case Notification::Type_AllNodesQueried:
		{
			if(zwDebug) fprintf(zl, "Notificaton:  AllNodesQueried\n" );
			zwD->g_nodesQueried = true;
#ifdef linux
			pthread_cond_broadcast(&(zwD->initCond));
#endif
			break;
		}
		case Notification::Type_AwakeNodesQueried:
		{
			if(zwDebug) fprintf(zl, "Notificaton:  AwakeNodesQueried\n" );
			zwD->g_nodesQueried = true;
#ifdef linux
			pthread_cond_broadcast(&(zwD->initCond));
#endif
			break;
		}
		case Notification::Type_DriverReady:
		{
			if(zwDebug) fprintf(zl, "Notification:  DriverReady\n" );
			zwD->g_homeId = _notification->GetHomeId();
			break;
		}
		case Notification::Type_DriverReset:
		{
			if(zwDebug) fprintf(zl, "Notification:  DriverReset\n" );
			break;
		}
		case Notification::Type_DriverFailed:
		{
			if(zwDebug) fprintf(zl, "Notification:  DriverFailed\n" );
#ifdef linux
			pthread_cond_broadcast(&(zwD->initCond));
#endif
			break;
		}
		case Notification::Type_Group:
		{
			if(zwDebug) fprintf(zl, "Notification:  Group\n" );
			break;
		}
		case Notification::Type_NodeAdded:
		{
			if(zwDebug)
			{
				cout << "Added Node " << (int) _notification->GetNodeId() << endl;
				fprintf(zl, "Notification:  NodeAdded\n" );
			};
			// Add the new node to our list
			NodeInfo* nodeInfo = new NodeInfo();
			nodeInfo->m_homeId = _notification->GetHomeId();
			nodeInfo->m_nodeId = _notification->GetNodeId();
			nodeInfo->m_polled = false;
			zwD->g_nodes.push_back( nodeInfo );
			break;
		}
		case Notification::Type_NodeEvent:
		{
			if(zwDebug) fprintf(zl, "Notification:  Event\n" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				if(zwDebug) cout << "Received Event from Node " << (int) nodeInfo->m_nodeId << endl;
				// We have received an event from the node, caused by a
				// basic_set or hail message.
				// TBD...
			}
			break;
		}
		case Notification::Type_NodeNaming:
		{
			if(zwDebug) fprintf(zl, "Notification:  NodeNaming\n" );
			break;
		}
		case Notification::Type_NodeProtocolInfo:
		{
			if(zwDebug) fprintf(zl, "Notification:  NodeProtocolInfo\n" );
			break;
		}
		case Notification::Type_NodeQueriesComplete:
		{
			if(zwDebug) fprintf(zl, "Notification:  NodeQueriesComplete\n" );
			break;
		}
		case Notification::Type_NodeRemoved:
		{
			if(zwDebug) fprintf(zl, "Notification:  NodeRemoved\n" );
			// Remove the node from our list
			uint32 const homeId = _notification->GetHomeId();
			uint8 const nodeId = _notification->GetNodeId();
			for( list<NodeInfo*>::iterator it = zwD->g_nodes.begin(); it != zwD->g_nodes.end(); ++it )
			{
				NodeInfo* nodeInfo = *it;
				if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
				{
					if(zwDebug) cout << "Removed Node " << (int) nodeId << endl;
					zwD->g_nodes.erase( it );
					break;
				}
			}
			break;
		}
		case Notification::Type_PollingDisabled:
		{
			if(zwDebug) fprintf(zl, "Notification:  PollingDisabled\n" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo->m_polled = false;
			}
			break;
		}
		case Notification::Type_PollingEnabled:
		{
			if(zwDebug) fprintf(zl, "Notification:  PollingEnabled\n" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo->m_polled = true;
			}
			break;
		}
		case Notification::Type_ValueAdded:
		{
			if(zwDebug) fprintf(zl, "\nNotification:  ValueAdded" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				// Add the new value to our list
				nodeInfo->m_values.push_back( _notification->GetValueID() );
				valInfo *vf = new valInfo;
				vf->f_value = false;
				vf->desired_state = 0;
				vf->desired_state_timestamp = 0;
				vf->reported_state = 0;
				vf->enforce = false;
				nodeInfo->v_info.push_back(*vf);
			}
			break;
		}
		case Notification::Type_ValueChanged:
		{
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				ValueID valueid = _notification->GetValueID();
				list<valInfo>::iterator bitr = nodeInfo->v_info.begin();
				list<ValueID>::iterator itd = nodeInfo->m_values.begin();
				bool foundv = false;
				while((bitr != nodeInfo->v_info.end()) && !foundv)
				{
					if(valueid == (*itd))
					{
						(*bitr).f_value = true;
						foundv = true;
					}
					else
					{
						bitr++;
						itd++;
					}
				}
				if(zwDebug)
				{
					fprintf(zl, "\nValue %016llX changed.", valueid.GetId() );
					fprintf(zl, "\nHome ID: 0x%.8x  Node ID: %d,  Polled: %s", nodeInfo->m_homeId, nodeInfo->m_nodeId, nodeInfo->m_polled?"true":"false" );
					fprintf(zl, "\nValue is: \n part of command class: 0x%.2x, of genre: %d, with index %d, and instance %d",
						valueid.GetCommandClassId(), valueid.GetGenre(), valueid.GetIndex(), valueid.GetInstance());
				};
				switch( valueid.GetType() )
				{
				case ValueID::ValueType_Bool:
					bool bTestBool;
					Manager::Get()->GetValueAsBool( valueid, &bTestBool );
					if(zwDebug) fprintf(zl, "\nValueB is: %s\n", bTestBool?"true":"false" );
					(*bitr).reported_state = (bTestBool?1:0);
					break;
				case ValueID::ValueType_Byte:
					uint8 bTestByte;
					Manager::Get()->GetValueAsByte( valueid, &bTestByte );
					if(zwDebug) fprintf(zl, "\nValueb is: 0x%.2x\n", bTestByte );
					(*bitr).reported_state = bTestByte;
					break;
				case ValueID::ValueType_Decimal:
					float bTestFloat;
					Manager::Get()->GetValueAsFloat( valueid, &bTestFloat );
					if(zwDebug) fprintf(zl, "\nValueF is: %.2f\n", bTestFloat );
					break;
				case ValueID::ValueType_Int:
					int32 bTestInt;
					Manager::Get()->GetValueAsInt( valueid, &bTestInt );
					if(zwDebug) fprintf(zl, "\nValueI is: %d\n", bTestInt );
					break;
				case ValueID::ValueType_List:
				case ValueID::ValueType_Max:
				case ValueID::ValueType_Schedule:
				case ValueID::ValueType_Short:
					int16 bTestShort;
					Manager::Get()->GetValueAsShort( valueid, &bTestShort );
					if(zwDebug) fprintf(zl, "\nValueS is: %u\n", bTestShort );
					break;
				case ValueID::ValueType_String:
				{
					string bTestString;
					Manager::Get()->GetValueAsString( valueid, &bTestString );
					if(zwDebug) fprintf(zl, "\nValueStr is: %s\n", bTestString.c_str() );
				}
					break;
				case ValueID::ValueType_Button:
					break;
				}
			}
			else
			{
				// ValueChanged notification for a node that doesn't appear to exist in our g_nodes list
				fprintf(zl, "\nERROR: Value changed notification for an unidentified node.\n" );
			}
			break;
		}
		case Notification::Type_ValueRemoved:
		{
			if(zwDebug) fprintf(zl, "\nNotification:  ValueRemoved" );
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				// Remove the value from out list
				for( list<ValueID>::iterator it = nodeInfo->m_values.begin(); it != nodeInfo->m_values.end(); ++it )
				{
					if( (*it) == _notification->GetValueID() )
					{
						nodeInfo->m_values.erase( it );
						break;
					}
				}
			}
			break;
		}
		case Notification::Type_ValueRefreshed:
		case Notification::Type_NodeNew:
		case Notification::Type_CreateButton:
		case Notification::Type_DeleteButton:
		case Notification::Type_ButtonOn:
		case Notification::Type_ButtonOff:
		case Notification::Type_EssentialNodeQueriesComplete:
		default:
			break;
	}
#ifdef WIN32
	LeaveCriticalSection( &(zwD->g_criticalSection) );
#endif
#ifdef linux
	pthread_mutex_unlock( &(zwD->g_criticalSection) );
#endif
};

#ifdef WIN32
int _tmain(int argc, _TCHAR* argv[])
{
#endif
#ifdef linux
int main(int argc, char* argv[])
{
#endif
	string chkptfilename;
	struct stat finfo;
	time_t current_time;
	zl = fopen("zwLog.txt","w");
	if(zl == NULL)
	{
		fprintf(stderr, "Could not open log file\n");
		return(-911);
	}
	if(ferror(zl)) {
		fprintf(stderr, "Error in fprintf\n");
		return(-910);
	}
	fprintf(zl, "Opening Log File\n"); fflush(zl);
	zwD = new zwDevices();
	zwP = new zwPrefs();
	zwR = new zwRules();

#ifdef WIN32
	string cfgfile = "zwconfig.txt";
#endif
#ifdef linux
	string cfgfile = (const char *) argv[1];
#endif
	int cfgcode = zwP->ingest(cfgfile); // Read the configuration file before initializing ZwRules
	if(cfgcode < 0)
	{
		if(cfgcode == -205)
		{
			fprintf(zl, "Didn't specify a rules file\n"); fflush(zl);
		}
		else
		{
			fprintf(zl, "Error in processing %s code: %d\n", cfgfile.c_str(), cfgcode); fflush(zl);
		};
		return cfgcode;
	};
	int initcode = zwR->init(zwP);
	if(initcode <0)
	{
		fprintf(zl, "Error in init, code: %d\n", initcode); fflush(zl);
		return initcode;
	}
	// If a checkpoint file exists, we're going to do a checkpoint recovery
	if((chkptfilename = zwP->getchkptfilename()).empty())
	{
		chkptfilename = "zwchkpt.txt";
	};
	if(stat(chkptfilename.c_str(), &finfo)==0)
	{ // File exists. Load checkpoint into rules list
		if(zwR->loadchkptfile(chkptfilename.c_str())!=0)
		{ // error
			return -1; // Bad checkpoint file
		};
		// We're going to simulate controlling the devices
		// from the date the file was written until the current time
		zwR->setexecutemode(false);
		zwR->setsimstarttime(finfo.st_mtime);
		zwR->setsimendtime(time(&current_time));
		if(zwR->run(zwP, zwD)!=0)
		{ // error
			return -3; // Problem updating the rules list
		};
		// We're ready to control the devices for real
		zwR->setexecutemode(true);
	}
	else // File doesn't exist. Parse rules into rules list
	{
		int rc;
		if((rc = zwR->ingest(zwP))!=0)
		{ // error
			fprintf(zl, "Rule parser returned error code - %d\n", rc); fflush(zl);
			return -2; // Problem parsing rules file
		};
	};
	// At this point, we have a rules list to execute or simulate
#ifdef MONGOOSE
	struct mg_server *server = mg_create_server(NULL, event_handler);
	if(zwR->getexecutemode())
	{ // We're not providing the web service during a simulation
		mg_set_option(server, "document_root", ".");      // Serve current directory
		mg_set_option(server, "listening_port", "8000");  // Open port 8000
		mg_start_thread(serve, server);
	}
#endif
	if(zwR->run(zwP, zwD)!=0)
	{ // error
		return -4; // Problem executing the rules list
	};
#ifdef MONGOOSE
	mg_destroy_server(&server);
#endif
	fprintf(zl,"Program Terminated\n"); fflush(zl);
	fclose(zl);
//	string dum1;
//	scanf("%s", dum1.c_str());
	return 0; // we're done
}
