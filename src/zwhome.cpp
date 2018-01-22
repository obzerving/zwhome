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
#include "zwRules.h"
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
 *  From external systems
	http://zwhomeserver:8000/devices/<nodename>?state=[on,off]
	or
	http://zwhomeserver:8000/devices/<nodename>?state=[on,off]&level=[1-100,255]
	http://zwhomeserver:8000/system?state=[on,off]

 *  From human users
	http://zwhomeserver:8000/status

 *  From sensors (readings)
	http://zwhomeserver:8000/sensors/<nodename>?param=[luminance,temperature,humidity]&value=[99999]

 *  From binary and multilevel switches (status)
	http://zwhomeserver:8000/devices/<nodename>?status=[on,off]
	or
	http://zwhomeserver:8000/devices/<nodename>?status=[on,off]&level=[1-100,255]
*/
struct mg_mgr mgr;
struct mg_connection *nc;
struct mg_bind_opts bind_opts;
static const char *s_http_port = "8000";
static struct mg_serve_http_opts s_http_server_opts;
                typedef struct
                {
                    string namestr;
                    string actionstr;
                    time_t time;
                } entrytmplt;
char dcat[20] = {"/devices"};
char scat[20] = {"/sensors"};
char systemcat[20] = {"/system"};
char statcat[20] = {"/status"};

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *hm = (struct http_message *) ev_data;
    char *key, *qkey, *skey, *dkey;
    int endstr;
    char qstr[24];
    bool idone = false;
	switch (ev)
	{
		case MG_EV_HTTP_REQUEST:
			if(zwDebug)fprintf(zl,"URI=%s\n", hm->uri.p); fflush(zl);
            if (mg_vcmp(&hm->uri, systemcat) == 0)
			{
				char sysstate[4]="?";
				qkey = (char *) strchr(hm->uri.p, '?');
                qkey++; // Pointing to start of query string (character after ?)
                key = strchr(qkey, ' '); // Pointing to end of query string + 1
                endstr = key - qkey;
                strncpy(qstr, qkey, endstr);
                qstr[endstr]='\0';
                key = qstr+endstr;
                skey=strstr(qstr, "state");
                if(skey != NULL)
                {
                    skey=skey+6; // Pointing at first character after =
                    endstr = key-skey;
                    strncpy(sysstate, skey, key-skey);
                    sysstate[endstr] = '\0';
                }
				if(!strcmp(sysstate, "on")) strcpy(zwR->sys_stat,"on");
				if(!strcmp(sysstate, "off")) strcpy(zwR->sys_stat,"off");
                mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
                mg_printf_http_chunk(nc, "Turning System %s\n", sysstate);
				fprintf(zl,"Web Request to Turn System %s\n", sysstate);
				fflush(zl);
                mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
			}
			else if (!strncmp(hm->uri.p, dcat, strlen(dcat)))
			{ //This code handles clients controlling switches and dimmers
                char dlevel[4];
				char dname[32];
				char dstate[4];
				dkey = (char *) strstr(hm->uri.p, dcat);
				dkey = dkey + strlen(dcat) + 1; // position at beginning of nodename
				qkey = strchr(dkey, '?');
				endstr = qkey - dkey;
				strncpy(dname, dkey, endstr);
				dname[endstr] = '\0';
                qkey++; // Pointing to start of query string (character after ?)
                key = strchr(qkey, ' '); // Pointing to end of query string + 1
                endstr = key - qkey;
                strncpy(qstr, qkey, endstr);
                qstr[endstr]='\0';
                key = qstr+endstr;
                skey=strstr(qstr, "state");
                if(skey != NULL)
                {
                    skey=skey+6; // Pointing at first character after =
                    qkey = strchr(skey,'&');
                    if(qkey != NULL)
                    { // More than state variable
                        endstr = qkey-skey;
                        strncpy(dstate, skey, endstr);
                        dstate[endstr] = '\0';
                        qkey++; // Pointing at next variable
                        skey = strstr(qkey, "level");
                        if(skey != NULL)
                        {
                            skey = skey + 6; // Pointing at first character after =
                            endstr = key-skey;
                            strncpy(dlevel, skey, endstr);
                            dlevel[endstr] = '\0';
                        }
                        else
                        { // We only want the level variable and didn't get it
                            dlevel[0] = '\0';
                        }
                    }
                    else
                    { // Only state variable
                        endstr = key-skey;
                        strncpy(dstate, skey, key-skey);
                        dstate[endstr] = '\0';
                        dlevel[0] = '\0';
                    }
                    pthread_mutex_lock( &(zwD->g_criticalSection) );
                    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
                    list<dvInfo>::iterator idv = zwR->dvl.begin();
                    while((idv != zwR->dvl.end()) && !idone)
                    {
                        if(!strcmp((*idv).nodename.c_str(), dname))
                        {
                            list<string> vbtn;
                            vbtn.push_back((*idv).nodename);
                            if((strcmp(dstate, "on") == 0))
                            {
                                if(dlevel[0] == '\0')
                                { // no level specified, so use previous level
                                    mg_printf_http_chunk(nc, "Turning %s %s\r\n", dname, dstate);
                                    fprintf(zl, "Web Request to Turn %s %s\n", dname, dstate);
                                    fflush(zl);
                                    zwD->doAction((uint8) 255, vbtn, false);
                                }
                                else
                                { // use the specified level
                                    uint8 dimlvl = (uint8) atoi(dlevel); // user can specify 1-100, but
                                    if((dimlvl >  (uint8) 99) && (dimlvl <  (uint8) 255)) dimlvl = (uint8) 99; // we can only use 1-99 & 255
                                    mg_printf_http_chunk(nc, "Turning %s %s to %s percent\r\n", dname, dstate,dlevel);
                                    fprintf(zl, "Web Request to Turn %s %s to %s percent\n", dname, dstate,dlevel);
                                    fflush(zl);
                                    zwD->doAction(dimlvl, vbtn, false);
                                }
                            }
                            else if((strcmp(dstate, "off") == 0))
                            {
                                mg_printf_http_chunk(nc, "Turning %s %s\r\n", dname, dstate);
                                fprintf(zl, "Web Request to Turn %s %s\n", dname, dstate);
                                fflush(zl);
                                zwD->doAction((uint8) 0, vbtn, false);
                            }
                            idone = true;
                        }
                        idv++;
                    } // while
                    pthread_mutex_unlock( &(zwD->g_criticalSection) );
                    if(idone) mg_printf_http_chunk(nc, "Completed\n");
                    else mg_printf_http_chunk(nc, "Unknown nodename: %s\n", dname);
                    mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
                    fflush(zl);
                } // found state variable
                else
                { // See if it's the device itself reporting a change
                    skey=strstr(qstr, "status");
                    if(skey != NULL)
                    {
                        skey=skey+7; // Pointing at first character after =
                        qkey = strchr(skey,'&');
                        if(qkey != NULL)
                        { // More than state variable
                            endstr = qkey-skey;
                            strncpy(dstate, skey, endstr);
                            dstate[endstr] = '\0';
                            qkey++; // Pointing at next variable
                            skey = strstr(qkey, "level");
                            if(skey != NULL)
                            {
                                skey = skey + 6; // Pointing at first character after =
                                endstr = key-skey;
                                strncpy(dlevel, skey, endstr);
                                dlevel[endstr] = '\0';
                            }
                            else
                            { // We only want the level variable and didn't get it
                                dlevel[0] = '\0';
                            }
                        }
                        else
                        { // Only state variable
                            endstr = key-skey;
                            strncpy(dstate, skey, key-skey);
                            dstate[endstr] = '\0';
                            dlevel[0] = '\0';
                        }
                        list<dvInfo>::iterator idv = zwR->dvl.begin();
                        while((idv != zwR->dvl.end()) && !idone)
                        {
                            if(!strcmp((*idv).nodename.c_str(), dname))
                            {
                                float fvalue = 0.0;
                                if(!strcmp(dstate, "on")) fvalue = 1.0;
                                if(dlevel[0] != '\0') fvalue = atof(dlevel);
                                ((*idv).reading.front()).lastreading = fvalue;
                            }
                            idv++;
                        }
                        mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
                        if(idone) mg_printf_http_chunk(nc, "Status Received\n");
                        else mg_printf_http_chunk(nc, "Unknown nodename: %s\n", dname);
                        mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
                    }
                } // device reporting in
			} // if (!strncmp(conn->uri, switchcat, strlen(switchcat))
			else if (!strncmp(hm->uri.p, scat, strlen(scat)))
			{ // This code handles wifi sensors reporting in
                char svalue[8];
				char sname[32];
				char sparam[24];
				dkey = (char *) strstr(hm->uri.p, scat);
				dkey = dkey + strlen(dcat) + 1; // position at beginning of nodename
				qkey = strchr(dkey, '?');
				endstr = qkey - dkey;
				strncpy(sname, dkey, endstr);
				sname[endstr] = '\0';
                qkey++; // Pointing to start of query string (character after ?)
                key = strchr(qkey, ' '); // Pointing to end of query string + 1
                endstr = key - qkey;
                strncpy(qstr, qkey, endstr);
                qstr[endstr]='\0';
                key = qstr+endstr;
                skey=strstr(qstr, "param");
                if(skey != NULL)
                {
                    skey=skey+6; // Pointing at first character after =
                    qkey = strchr(skey,'&');
                    if(qkey != NULL)
                    { // More than state variable
                        endstr = qkey-skey;
                        strncpy(sparam, skey, endstr);
                        sparam[endstr] = '\0';
                        qkey++; // Pointing at next variable
                        skey = strstr(qkey, "value");
                        if(skey != NULL)
                        {
                            skey = skey + 6; // Pointing at first character after =
                            endstr = key-skey;
                            strncpy(svalue, skey, endstr);
                            svalue[endstr] = '\0';
                        }
                        else
                        {
                            svalue[0] = '\0';
                        }
                    }
                    else
                    { // Only param variable
                        endstr = key-skey;
                        strncpy(sparam, skey, key-skey);
                        sparam[endstr] = '\0';
                        svalue[0] = '\0';
                    }
                }
                else
                {
                    sparam[0] = '\0';
                    svalue[0] = '\0';
                }
                if((sparam[0] != '\0') && (svalue[0] != '\0'))
                { // Must have both
                    pthread_mutex_lock( &(zwD->g_criticalSection) );
                    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
                    list<dvInfo>::iterator idv = zwR->srl.begin();
                    while((idv != zwR->srl.end()) && !idone)
                    {
                        if(!strcmp((*idv).nodename.c_str(), sname))
                        {
                            float fvalue = atof(svalue);
                            if(zwDebug) fprintf(zl, "Received report from %s: %s = %s\n", sname, sparam, svalue);
                            for(list<readInfo>::iterator ri = (*idv).reading.begin(); ri != (*idv).reading.end(); ri++)
                            {
                                if((((*ri).typeindex == 1) && (!strcmp(sparam, "temperature")))
                                || (((*ri).typeindex == 3) && (!strcmp(sparam, "luminance")))
                                || (((*ri).typeindex == 5) && (!strcmp(sparam, "relativehumidity"))))
                                {
                                    if((fvalue >= (*ri).min) && (fvalue <= (*ri).max))
                                    {
                                        bool done = false;
                                        int dp = 0;
                                        while(!done && (dp < 10))
                                        {
                                            if((*ri).points[dp] == -100)
                                            {
                                                (*ri).points[dp] = fvalue;
                                                dp++;
                                                done = true;
                                            }
                                            else dp++;
                                        }
                                        if(!done)
                                        { // slide the data points
                                            for(int j=0; j<dp-1; j++)(*ri).points[j] = (*ri).points[j+1];
                                            (*ri).points[dp-1] = fvalue;
                                            done = true;
                                        }
                                        float summit = 0.0; // compute the average
                                        for(int j=0; j<dp; j++) summit = summit + (*ri).points[j];
                                        (*ri).lastreading = summit/dp;
                                    }
                                    else
                                    { // Value outside of range, so drop it
                                        if(zwDebug) fprintf(zl, "Reading from Sensor %s outside of range: %f\n", ((*idv).nodename).c_str(), fvalue);
                                    }
                                    idone = true;
                                }
                            }
                        }
                        idv++;
                    } // while
                    pthread_mutex_unlock( &(zwD->g_criticalSection) );
                    if(idone) mg_printf_http_chunk(nc, "{\"sleep\":%d}\n",60);
                    else mg_printf_http_chunk(nc, "Unknown nodename: %s\n", sname);
                    mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
                }
                fflush(zl);
			} // if (!strncmp(hm->uri.p, scat, strlen(dcat)))
			else if (!strncmp(hm->uri.p, statcat, strlen(statcat)))
			{
                string rfn = zwP->getrulefilename();
                mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Type: text/html\r\n\r\n");
                mg_printf_http_chunk(nc, "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\
<html><head><meta content=\"text/html; charset=ISO-8859-1\" http-equiv=\"content-type\"><title>HAC Home Page</title></head><body>\
<table style=\"text-align: left; width: 100%;\" border=\"0\" cellpadding=\"2\" cellspacing=\"2\"><tbody><tr align=\"center\">\
<td colspan=\"2\" rowspan=\"1\">Home Automation System</td></tr><tr><td colspan=\"2\" rowspan=\"1\"></td></tr>");

                mg_printf_http_chunk(nc, "<td colspan=\"2\" rowspan=\"1\">Executing Rule File: %s</td></tr>", (zwP->getrulefilename()).c_str());

                mg_printf_http_chunk(nc, "<tr><td colspan=\"2\" rowspan=\"1\"></td></tr><tr valign=\"top\"><td style=\"width: 50%;\">\
<table style=\"text-align: left; width: 100%; height: 26px;\" border=\"1\" cellpadding=\"2\" cellspacing=\"2\"><tbody><tr>\
<td style=\"height: 26px; background-color: rgb(19, 84, 194); width: 75%;\"><span style=\"color: white; font-style: italic;\">Device</span>\
</td><td style=\"height: 26px; background-color: rgb(19, 84, 194);\"><span style=\"color: white; font-style: italic;\">Status</span></td></tr>");
                for(list<dvInfo>::iterator itd=zwR->dvl.begin(); itd!=zwR->dvl.end(); itd++) // for each node
                {
                    if(!(*itd).isController)
                    {
                        string ds;
                        if((*itd).reading.front().lastreading == -100.0)
                        {
                            ds.append("NTR");
                        }
                        else if((*itd).reading.front().lastreading == 0.0)
                        {
                            ds.append("off");
                        }
                        else
                        {
                            ds.append("on");
                            if((*itd).canDim)
                            {
                                char sbf[16];
                                snprintf(sbf, 15, "%.1f",(*itd).reading.front().lastreading);
                                ds = ds + " (" + string(sbf) + ")";
                            }
                            if((*itd).isFan)
                            {
                                ds.append(" (");
                                if(((*itd).reading.front().lastreading > 0.0) && ((*itd).reading.front().lastreading < 31.0)) ds.append("Low");
                                if(((*itd).reading.front().lastreading > 30.0) && ((*itd).reading.front().lastreading < 61.0)) ds.append("Medium");
                                if(((*itd).reading.front().lastreading > 60.0) && ((*itd).reading.front().lastreading < 100.0)) ds.append("High");
                                ds.append(")");
                            }
                        }
                        mg_printf_http_chunk(nc, "<tr><td style=\"height: 26px;\">%s</td><td style=\"height: 26px;\">%s</td></tr>",
                                            (*itd).nodename.c_str(), ds.c_str());
                    }
                }
                // Now, for the sensors...
                for(list<dvInfo>::iterator itd=zwR->srl.begin(); itd!=zwR->srl.end(); itd++) // for each node
                {
                        for(list<readInfo>::iterator ri = (*itd).reading.begin(); ri != (*itd).reading.end(); ri++)
                        {
                            string devtitle((*itd).nodename);
                            string devunits;
                            switch ((*ri).typeindex)
                            {
                                case 1:
                                    devtitle.append(string(" (Temperature)"));
                                    devunits.append(string(1,'F'));
                                    break;
                                case 3:
                                    devtitle.append(string(" (Luminance)"));
                                    devunits.append(string(" lux"));
                                    break;
                                case 5:
                                    devtitle.append(string(" (Relative Humidity)"));
                                    devunits.append(string(1,'%'));
                                    break;
                                default:
                                    break;
                            }
                            if((*ri).lastreading == -100.0) mg_printf_http_chunk(nc, "<tr><td style=\"height: 26px;\">%s</td><td style=\"height: 26px;\">NTR</td></tr>", devtitle.c_str());
                            else mg_printf_http_chunk(nc, "<tr><td style=\"height: 26px;\">%s</td><td style=\"height: 26px;\">%.1f%s</td></tr>",
                                            devtitle.c_str(), (*ri).lastreading, devunits.c_str());
                        }
                }


                vector<entrytmplt> entries;
                vector<entrytmplt>::iterator it;
                time_t nowTime = time(NULL);
                mg_printf_http_chunk(nc, "</tbody></table></td><td><table style=\"text-align: left; width: 100%; height: 26px;\" border=\"1\" cellpadding=\"2\" cellspacing=\"2\">\
<tbody><tr><td style=\"height: 26px; background-color: rgb(19, 84, 194);\"><span style=\"color: white; font-style: italic;\">Upcoming Events</span></td></tr>");
                for(list<zwRules::zwRule>::iterator rl = zwR->rulelist.begin(); rl != zwR->rulelist.end(); rl++)
                { // For each rule
                    entrytmplt entry; // create a table entry
                    for(uint fe = 0; fe < (*rl).fromEvent.Time.size(); fe++)
                    {
                        if(((*rl).fromEvent.Time[fe] > nowTime) || ((*rl).firing > -1))
                        {
                            for(list<string>::iterator i=(*rl).device.begin(); i!=(*rl).device.end();i++)
                            {
                                entry.namestr = (*i);
                                if((*rl).fromEvent.Time[fe] > nowTime) // We want fromEvents that will happen in the future
                                { // (If the rule is firing, we just want its toEvent)
                                    entry.actionstr = string(((*rl).on_action == 0)?"off":"on");
                                    entry.time = (*rl).fromEvent.Time[fe];
                                    if(entries.size() == 0)
                                    { // first item in vector
                                        entries.push_back(entry);
                                    }
                                    else
                                    { // Place this item in ascending time order
                                        bool founde = false;
                                        vector<entrytmplt>::iterator j=entries.begin();
                                        while((j!=entries.end()) and !founde)
                                        {
                                            if(((entry.time == (*j).time) && (entry.namestr == (*j).namestr)) && (entry.actionstr == (*j).actionstr))  // check for duplicate entry
                                            {
                                                if(zwDebug) fprintf(zl, "Duplicate entry in upcoming events\n");
                                                founde = true; // This will keep the entry from being included in the upcoming events list
                                            }
                                            else if(entry.time < (*j).time)
                                            {
                                                j = entries.insert(j, entry);
                                                founde = true;
                                            }
                                            j++;
                                        }
                                        if(!founde) entries.push_back(entry); // this entry is the latest
                                    }
                                }
                                // now, do it for the corresponding toEvent (future or firing)
                                if(((*rl).off_action > 99) && ((*rl).off_action < 255)) entry.actionstr = string("remove enforcement");
                                else entry.actionstr = string(((*rl).off_action == 0)?"off":"on");
                                entry.time = (*rl).toEvent.Time[fe];
                                bool founde = false;
                                vector<entrytmplt>::iterator j=entries.begin();
                                while((j!=entries.end()) and !founde)
                                {
                                    if(((entry.time == (*j).time) && (entry.namestr == (*j).namestr)) && (entry.actionstr == (*j).actionstr))  // check for duplicate entry
                                    {
                                        if(zwDebug) fprintf(zl, "Duplicate entry in upcoming events\n");
                                        founde = true; // This will keep the entry from being included in the upcoming events list
                                    }
                                    else if(entry.time < (*j).time)
                                    {
                                        j = entries.insert(j, entry);
                                        founde = true;
                                    }
                                    j++;
                                }
                                if(!founde) entries.push_back(entry); // this entry is the latest
                            }
                        } // if future event or firing...
                    } // for fe...
                }
                int evt_table_limit = 11; // Only display the next 10 events
                int evt_table_cnt = 0;
                for(vector<entrytmplt>::iterator i=entries.begin(); i!=entries.end();i++)
                {
                    if(++evt_table_cnt < evt_table_limit) mg_printf_http_chunk(nc,
                    "<tr><td style=\"height: 26px;\">%s %s: %s</td></tr>", (*i).namestr.c_str(),
                    (*i).actionstr.c_str(), ctime(&(*i).time));
                }

                mg_printf_http_chunk(nc, "</tbody></table></td></tr></tbody></table><br></body></html>");
                mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
			}
			else
			{
                mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */
			}

			break;
		default:
            break;
	}
}

static void* serve(void *server)
    	{
    		fprintf(zl,"Starting server poll\n"); fflush(zl);
    		for(;;) mg_mgr_poll(&mgr, 1000);
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
	pthread_mutex_lock( &(zwD->g_criticalSection) );
	switch( _notification->GetType() )
	{
		case Notification::Type_AllNodesQueried:
		{
			if(zwDebug) fprintf(zl, "Notificaton:  AllNodesQueried\n" );
			zwD->g_nodesQueried = true;
			pthread_cond_broadcast(&(zwD->initCond));
			break;
		}
		case Notification::Type_AllNodesQueriedSomeDead:
		{
			if(zwDebug) fprintf(zl, "Notificaton:  AllNodesQueriedSomeDead\n" );
			zwD->g_nodesQueried = true;
			pthread_cond_broadcast(&(zwD->initCond));
			break;
		}
		case Notification::Type_AwakeNodesQueried:
		{
			if(zwDebug) fprintf(zl, "Notificaton:  AwakeNodesQueried\n" );
			zwD->g_nodesQueried = true;
			pthread_cond_broadcast(&(zwD->initCond));
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
			pthread_cond_broadcast(&(zwD->initCond));
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
				fprintf(zl, "Notification:  NodeAdded\n" ); fflush(zl);
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
			if(zwDebug)
			{
                fprintf(zl, "Notification:  ValueAdded\n" ); fflush(zl);
			}
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				// Add the new value to our list
				ValueID valueid = _notification->GetValueID();
				nodeInfo->m_values.push_back( valueid );
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
			if(zwDebug)
			{
                fprintf(zl, "Notification:  ValueChanged\n" ); fflush(zl);
			}
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
					fprintf(zl, "\nValue ID %016llX.", valueid.GetId() ); fflush(zl);
					fprintf(zl, "\nHome ID: 0x%.8x  Node ID: %d,  Polled: %s", nodeInfo->m_homeId, nodeInfo->m_nodeId, nodeInfo->m_polled?"true":"false" ); fflush(zl);
					fprintf(zl, "\nValue is: \n part of command class: 0x%.2x, of genre: %d, with index %d, and instance %d",
						valueid.GetCommandClassId(), valueid.GetGenre(), valueid.GetIndex(), valueid.GetInstance()); fflush(zl);
				};
				bool foundit = false;
				switch(valueid.GetCommandClassId())
				{
                    case 37: // Binary Switch
                    case 38: // Multilevel Switch
                    {
                        list<dvInfo>::iterator dv = zwR->dvl.begin();
                        while((dv != zwR->dvl.end()) && !foundit)
                        {
                            if(valueid.GetNodeId() == (*dv).nodeId)
                            {
                                if(valueid.GetIndex() == ((*dv).reading.front()).typeindex)
                                {
                                    switch(valueid.GetType())
                                    {
                                        case ValueID::ValueType_Bool:
                                            bool boolRpt;
                                            Manager::Get()->GetValueAsBool(valueid, &boolRpt);
                                            (*dv).reading.front().lastreading = boolRpt?1.0:0.0;
                                            break;
                                        case ValueID::ValueType_Byte:
                                            uint8 byteRpt;
                                            Manager::Get()->GetValueAsByte(valueid, &byteRpt);
                                            (*dv).reading.front().lastreading = (float) byteRpt;
                                            break;
                                        default:
                                            break;
                                    }
                                    foundit = true;
                                }
                            }
                            dv++;
                        }
                        break;
                    }
                    case 49: //Multilevel Sensor
                        list<dvInfo>::iterator dv = zwR->srl.begin();
                        while((dv != zwR->srl.end()) && !foundit)
                        {
                            if(valueid.GetNodeId() == (*dv).nodeId)
                            {
                                bool foundi = false;
                                list<readInfo>::iterator ri = (*dv).reading.begin();
                                while((ri != (*dv).reading.end()) && !foundi)
                                {
                                    if(valueid.GetIndex() == (*ri).typeindex)
                                    {
                                        float fvalue;
                                        Manager::Get()->GetValueAsFloat( (*itd), &fvalue );
                                        if((fvalue >= (*ri).min) && (fvalue <= (*ri).max))
                                        {
                                            bool done = false;
                                            int dp = 0;
                                            while(!done && (dp < 10))
                                            {
                                                if((*ri).points[dp] == -100)
                                                {
                                                    (*ri).points[dp] = fvalue;
                                                    dp++;
                                                    done = true;
                                                }
                                                else dp++;
                                            }
                                            if(!done)
                                            { // slide the data points
                                                for(int j=0; j<dp-1; j++)(*ri).points[j] = (*ri).points[j+1];
                                                (*ri).points[dp-1] = fvalue;
                                                done = true;
                                            }
                                            float summit = 0.0; // compute the average
                                            for(int j=0; j<dp; j++) summit = summit + (*ri).points[j];
                                            (*ri).lastreading = summit/dp;
                                        }
                                        else
                                        { // Value outside of range, so drop it
                                            if(zwDebug) fprintf(zl, "Reading from Sensor %s outside of range: %f\n", ((*dv).nodename).c_str(), fvalue);
                                        }
                                        foundit = true;
                                        foundi = true;
                                    }
                                    ri++;
                                }
                            }
                            dv++;
                        }
                        break;
				}
				switch( valueid.GetType() )
				{
				case ValueID::ValueType_Bool:
					bool bTestBool;
					Manager::Get()->GetValueAsBool( valueid, &bTestBool );
					if(zwDebug)
					{
                        fprintf(zl, "\nValueB is: %s\n", bTestBool?"true":"false" ); fflush(zl);
					}
					(*bitr).reported_state = (bTestBool?1:0);
					break;
				case ValueID::ValueType_Byte:
					uint8 bTestByte;
					Manager::Get()->GetValueAsByte( valueid, &bTestByte );
					if(zwDebug)
					{
                        fprintf(zl, "\nValueb is: 0x%.2x\n", bTestByte ); fflush(zl);
					}
					(*bitr).reported_state = bTestByte;
					break;
				case ValueID::ValueType_Decimal:
					float bTestFloat;
					Manager::Get()->GetValueAsFloat( valueid, &bTestFloat );
					if(zwDebug)
					{
                        fprintf(zl, "\nValueF is: %.2f\n", bTestFloat ); fflush(zl);
                    }
					break;
				case ValueID::ValueType_Int:
					int32 bTestInt;
					Manager::Get()->GetValueAsInt( valueid, &bTestInt );
					if(zwDebug)
					{
                        fprintf(zl, "\nValueI is: %d\n", bTestInt ); fflush(zl);
                    }
					break;
				case ValueID::ValueType_List:
					break;
				case ValueID::ValueType_Max:
					break;
				case ValueID::ValueType_Schedule:
					break;
				case ValueID::ValueType_Short:
					int16 bTestShort;
					Manager::Get()->GetValueAsShort( valueid, &bTestShort );
					if(zwDebug)
					{
                        fprintf(zl, "\nValueS is: %u\n", bTestShort ); fflush(zl);
                    }
					break;
				case ValueID::ValueType_String:
				{
					string bTestString;
					Manager::Get()->GetValueAsString( valueid, &bTestString );
					if(zwDebug)
					{
                        fprintf(zl, "\nValueStr is: %s\n", bTestString.c_str() ); fflush(zl);
					}
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
	pthread_mutex_unlock( &(zwD->g_criticalSection) );
};

int main(int argc, char* argv[])
{
	string chkptfilename, cfgfile;
	zl = fopen("zwLog.txt","w");
	if(zl == NULL)
	{
		fprintf(stderr, "Could not open log file\n");
		return(-911);
	}
	if(ferror(zl)) {
		fprintf(stderr, "Error in log file zl\n");
		return(-910);
	}
	fprintf(zl, "Opening Log File\n"); fflush(zl);
	zwD = new zwDevices();
	zwP = new zwPrefs();

	if(argc < 2) cfgfile.assign("zwconfig.txt");
	else cfgfile.assign((const char *) argv[1]);
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
	zwR = new zwRules();
	int initcode = zwR->init(zwP);
	if(initcode <0)
	{
		fprintf(zl, "Error in init, code: %d\n", initcode); fflush(zl);
		return initcode;
	}
	// At this point, we know what the wifi devices and sensors are
        // Since we ran the prequel program to name the nodes, we have an existing zwave network file
		zwP->readZWcfg(&(zwR->dvl), &(zwR->srl));
		if(zwDebug)
		{
            fprintf(zl,"Found %d nodes in file:\n", (zwR->dvl).size()); fflush(zl);
            for(list<dvInfo>::iterator dvi=zwR->dvl.begin();dvi != zwR->dvl.end(); ++dvi)
            {
                fprintf(zl,"%d - %s\n", dvi->nodeId, (dvi->nodename).c_str());
            }
        }
		// Start up the z-wave controller
		if(zwD->init(zwR->getexecutemode())!=0)
		{
			return -600; // couldn't init devices
		}
		// Build the device list from the zwcfg*.xml file
		int rc;
		if((rc = zwR->ingest(zwP))!=0)
		{ // error
			fprintf(zl, "Rule parser returned error code - %d\n", rc); fflush(zl);
			return -2; // Problem parsing rules file
		};
	// At this point, we have a rules list to execute or simulate
#ifdef MONGOOSE
    const char *err_str;
    void *server;
	if(zwR->getexecutemode())
	{ // We're not providing the web service during a simulation
        mg_mgr_init(&mgr, NULL);
        s_http_server_opts.document_root = ".";
        s_http_server_opts.enable_directory_listing = "yes";
        memset(&bind_opts, 0, sizeof(bind_opts));
        bind_opts.error_string = &err_str;
        nc = mg_bind_opt(&mgr, s_http_port, ev_handler, bind_opts);
        if (nc == NULL) {
            fprintf(stderr, "Error starting server on port %s: %s\n", s_http_port,
            *bind_opts.error_string);
            exit(1);
        }
        mg_set_protocol_http_websocket(nc);
		mg_start_thread(serve, server);
	}
#endif
	if(zwR->run(zwP, zwD)!=0)
	{ // error
		return -4; // Problem executing the rules list
	};
#ifdef MONGOOSE
	mg_mgr_free(&mgr);
#endif
	fprintf(zl,"Program Terminated\n"); fflush(zl);
	fclose(zl);
//	string dum1;
//	scanf("%s", dum1.c_str());
	return 0; // we're done
}
