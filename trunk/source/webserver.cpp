#include "webserver.h"

#include "sequencer.h"
#include "messaging.h"
#include "notifier.h"
#include "userauth.h"
#include "logger.h"
#include "config.h"

// important webserver context
struct mg_context	*ctx;

#ifdef _WIN32
#include <winsock.h>
#define	snprintf			_snprintf

#ifndef _WIN32_WCE
#ifdef _MSC_VER /* pragmas not valid on MinGW */
#endif /* _MSC_VER */
#else /* _WIN32_WCE */
/* Windows CE-specific definitions */
#pragma comment(lib,"ws2")
//#include "compat_wince.h"
#endif /* _WIN32_WCE */

#else
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>
#endif

#ifndef _WIN32_WCE /* Some ANSI #includes are not available on Windows CE */
#include <time.h>
#include <errno.h>
#include <signal.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "mongoose.h"

typedef struct colour_t
{
	double r, g, b;
} colour_t;

// some colours with a good contrast inbetween
colour_t cvals[] =
{
	{0.0,0.8,0.0},
	{0.0,0.4,0.701960784314},
	{1.0,0.501960784314,0.0},
	{1.0,0.8,0.0},
	{0.2,0.0,0.6},
	{0.6,0.0,0.6},
	{0.8,1.0,0.0},
	{1.0,0.0,0.0},
	{0.501960784314,0.501960784314,0.501960784314},
	{0.0,0.560784313725,0.0},
	{0.0,0.282352941176,0.490196078431},
	{0.701960784314,0.352941176471,0.0},
	{0.701960784314,0.560784313725,0.0},
	{0.419607843137,0.0,0.419607843137},
	{0.560784313725,0.701960784314,0.0},
	{0.701960784314,0.0,0.0},
	{0.745098039216,0.745098039216,0.745098039216},
	{0.501960784314,1.0,0.501960784314},
	{0.501960784314,0.788235294118,1.0},
	{1.0,0.752941176471,0.501960784314},
	{1.0,0.901960784314,0.501960784314},
	{0.666666666667,0.501960784314,1.0},
	{0.933333333333,0.0,0.8},
	{1.0,0.501960784314,0.501960784314},
	{0.4,0.4,0.0},
	{1.0,0.749019607843,1.0},
	{0.0,1.0,0.8},
	{0.8,0.4,0.6},
	{0.6,0.6,0.0},
};

int getPlayerColour(int num, char *res)
{
    int numColours = sizeof(cvals) / sizeof(colour_t);
	if(num<0 || num > numColours) return 1;
	
	sprintf(res, "rgb(%d,%d,%d)", (int)(cvals[num].r * 255.0f), (int)(cvals[num].g * 255.0f), (int)(cvals[num].b * 255.0f));
	return 0;
}

int formatBytes(double bytes, char *res)
{
	if(bytes <= 1024)
		sprintf(res, "%0.2f B", bytes);
	else if(bytes > 1024 && bytes <= 1048576)
		sprintf(res, "%0.2f KB", bytes / 1024.0f);
	else if(bytes > 1048576 && bytes <= 1073741824)
		sprintf(res, "%0.2f MB", bytes / 1024.0f / 1024.0f);
	else //if(bytes > 1073741824 && bytes <= 1099511627776)
		sprintf(res, "%0.2f GB", bytes / 1024.0f / 1024.0f / 1024.0f);
	//else if(bytes > 1099511627776)
	//	sprintf(res, "%0.2f TB", bytes / 1024.0f / 1024.0f / 1024.0f / 1024.0f);
	return 0;
}

void html_header(struct mg_connection *conn)
{
	mg_printf(conn, "%s",
		"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
		"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\""
		"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">"
		"<html>"
		"<head>"
		"<meta http-equiv='refresh' content='10' />"
		"<title>Rigs of Rods Server</title>"
		"</head>"
		"<body>");

	mg_printf(conn, "<h2>Rigs of Rods Server (%s)</h2>", RORNET_VERSION);

	mg_printf(conn, "%s",
		"<div style='border:1px solid black;float:left;margin-right:10px;padding:5px;width:135px;'>"
		"<ul style='margin-left:-20px;list-style-type:none'>"
         "<li><a href='/config'>Configuration</a></li>"
         "<li><a href='/list'>Player List</a></li>"
         "<li><a href='/map'>Overview Map</a></li>"
         "<li><a href='/stats-general'>Statistics</a></li>"
         "<li><a href='/log/'>Log</a></li>"
         "<li><a href='/chat/'>Chat</a></li>"
		 );

	if(Config::getServerMode()==SERVER_INET && Sequencer::getNotifier() && Sequencer::getNotifier()->getAdvertised())
	{
		int trustlevel = Sequencer::getNotifier()->getTrustLevel();
		if(trustlevel > 1)
			mg_printf(conn, "%s", "<li><a href='/userauth/'>User Auth Cache</a></li>");
	}
	
	mg_printf(conn, "%s",
         "</ul>"
         "</div>"
		 "<div style='padding:5px;border:1px solid #dddddd;margin-left:150px;'>"
		 );
	/*
	mg_printf(conn, "<li><code>REQUEST_METHOD: %s "
	    "REQUEST_URI: \"%s\" QUERY_STRING: \"%s\""
	    " REMOTE_ADDR: %lx REMOTE_USER: \"(null)\"</code><hr>",
	    request_info->request_method, request_info->uri,
	    request_info->query_string ? request_info->query_string : "(null)",
	    request_info->remote_ip);
	*/
}

void html_footer(struct mg_connection *conn)
{
	mg_printf(conn, "%s", "</div></body></html>");
}

static void show_index(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	html_header(conn);
	html_footer(conn);
}

static void show_list(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	html_header(conn);
	mg_printf(conn, "%s", "<table border='1'>"
		"<tr>"
		"<td><b>Slot</b></td>"
		"<td><b>Status</b></td>"
		"<td><b>UID</b></td>"
		"<td><b>IP / Type</b></td>"
		"<td><b>Name</b></td>"
		"<td><b>Traffic</b></td>"
		"<td><b>Rate</b></td>"
		"<td><b>Authentication</b></td>"
		"</tr>");

	std::vector<client_t> clients = Sequencer::getClients();
	int slotnum=0;
	for(std::vector<client_t>::iterator it = clients.begin(); it != clients.end(); it++,slotnum++)
	{
		// some auth identifiers
		char *authst = (char *)"none";
		if(it->authstate & AUTH_BANNED)	authst = (char *)"banned";
		if(it->authstate & AUTH_BOT)    authst = (char *)"bot";
		if(it->authstate & AUTH_RANKED) authst = (char *)"ranked";
		if(it->authstate & AUTH_MOD)    authst = (char *)"moderator";
		if(it->authstate & AUTH_ADMIN)  authst = (char *)"admin";

		// construct screen
		if (it->status == FREE)
		{
			mg_printf(conn, "<tr><td>%i</td><td>free</td></tr>", it->slotnum);
		} else if (it->status == BUSY)
		{
			mg_printf(conn, "<tr><td>%i</td><td>busy</td></tr>", it->slotnum);
		} else if (it->status == USED)
		{
			char playerColour[128] = "";
			getPlayerColour(it->colournumber, playerColour);

			// get traffic stats for all streams
			char str_bw_in[128]="", str_bw_out[128]="", str_bw_in_rate[128]="", str_bw_out_rate[128]="";
			double bw_in=0, bw_out=0, bw_in_rate=0, bw_out_rate=0;
			for(std::map<unsigned int, stream_traffic_t>::iterator tit = it->streams_traffic.begin(); tit != it->streams_traffic.end(); tit++)
			{
				bw_in += tit->second.bandwidthIncoming;
				bw_in_rate += tit->second.bandwidthIncomingRate;
				bw_out += tit->second.bandwidthOutgoing;
				bw_out_rate += tit->second.bandwidthOutgoingRate;
			}
			formatBytes(bw_in, str_bw_in);
			formatBytes(bw_in_rate, str_bw_in_rate);
			formatBytes(bw_out, str_bw_out);
			formatBytes(bw_out_rate, str_bw_out_rate);

			// print the row
			mg_printf(conn, "<tr style='background-color:%s;'>", playerColour);
			mg_printf(conn, "<td>%i</td>", it->slotnum);
			mg_printf(conn, "<td>%s</td>", "used");
			mg_printf(conn, "<td><b>%03i</b></td>", it->uid);
			mg_printf(conn, "<td>%s</td>", it->ip_addr);
			mg_printf(conn, "<td>%s</td>", it->nickname);
			mg_printf(conn, "<td>%s, %s</td>", str_bw_in, str_bw_out);
			mg_printf(conn, "<td>%s/s, %s/s</td>", str_bw_in_rate, str_bw_out_rate);
			mg_printf(conn, "<td>%s</td>", authst);
			mg_printf(conn, "%s", "</tr>");

			
			
			for(std::map<unsigned int, stream_register_t>::iterator sit = it->streams.begin(); sit != it->streams.end(); sit++)
			{
				char *typeStr = (char *)"unkown";
				if(sit->second.type == 0) typeStr = (char *)"Vehicle";
				if(sit->second.type == 1) typeStr = (char *)"Character";
				if(sit->second.type == 2) typeStr = (char *)"AI Traffic";
				if(sit->second.type == 3) typeStr = (char *)"Chat";

				// traffic stats
				stream_traffic_t *traf = 0;
				if(it->streams_traffic.find(sit->first) != it->streams_traffic.end())
				{
					traf = &(it->streams_traffic.find(sit->first)->second);
				}

				mg_printf(conn, "<tr>"
					"<td></td>"
					"<td>%d</td>"
					"<td><b>%03d:%02d</b></td>"
					"<td>%s (%d)</td>"
					"<td>%s</td>", sit->second.status, it->uid, sit->first, typeStr, sit->second.type, sit->second.name);
				if(traf)
				{
					char tmp1[128]="", tmp2[128]="";
					formatBytes(traf->bandwidthIncoming, tmp1);
					formatBytes(traf->bandwidthOutgoing, tmp2);
					mg_printf(conn, "<td>%s, %s</td>", tmp1, tmp2);

					formatBytes(traf->bandwidthIncomingRate, tmp1);
					formatBytes(traf->bandwidthOutgoingRate, tmp2);
					mg_printf(conn, "<td>%s/s, %s/s</td>", tmp1, tmp2);
				} else
				{
					mg_printf(conn, "%s", "<td colspan='2'></td>");
				}

				mg_printf(conn, "%s", "</tr>");

			}
		}
	}
	if(slotnum < (int)Config::getMaxClients())
	{
		// add remaining free slots
		for(int i=slotnum;i<(int)Config::getMaxClients();i++)
			mg_printf(conn, "<tr><td>%i</td><td>free</td></tr>", i);
	}
	mg_printf(conn, "%s", "</table border><hr>");
	mg_printf(conn, "%s", "<small>Authentication Types: R=Ranked, M=Moderator, A=Admin, B=Bot, X=Banned<br>"
		"Traffic format : [Incoming], [Outgoing]"
		"</small><hr>");

	html_footer(conn);
}

static void show_stats_general(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	int timediff = Messaging::getTime() - Sequencer::getStartTime();
	int uphours = timediff/60/60;
	int upminutes = (timediff-(uphours*60*60))/60;
	int upseconds = (timediff-(uphours*60*60)-(upminutes*60));

	char tmp1[128]="", tmp2[128]="";

	html_header(conn);
	mg_printf(conn, "%s", "<ul>");
	mg_printf(conn, "<li>uptime: %d hours, %d minutes, %d seconds</li>", uphours, upminutes, upseconds);
	mg_printf(conn, "%s", "<li>total traffic:");
	mg_printf(conn, "%s", " <ul>");

	formatBytes(Messaging::getBandwitdthIncoming(), tmp1);
	formatBytes(Messaging::getBandwidthOutgoing(), tmp2);

	mg_printf(conn, "  <li>incoming: %s</li>", tmp1);
	mg_printf(conn, "  <li>outgoing: %s</li>", tmp2);
	mg_printf(conn, "%s", " </ul>");
	mg_printf(conn, "%s", "<li>bandwidth used (last minute):");
	mg_printf(conn, "%s", " <ul>");

	formatBytes(Messaging::getBandwitdthIncomingRate(), tmp1);
	formatBytes(Messaging::getBandwidthOutgoingRate(), tmp2);

	mg_printf(conn, "  <li>incoming: %s/s</li>", tmp1);
	mg_printf(conn, "  <li>outgoing: %s/s</li>", tmp2);
	mg_printf(conn, "%s", " </ul>");
	mg_printf(conn, "%s", "</ul>");
	html_footer(conn);
}

static void show_configuration(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	html_header(conn);

	mg_printf(conn, "%s", "<table border='1'>");
	mg_printf(conn, "<tr><td><b>Max Clients</b></td><td>%d</td></tr>", Config::getMaxClients());
	mg_printf(conn, "<tr><td><b>Server Name</b></td><td>%s</td></tr>", Config::getServerName().c_str());
	mg_printf(conn, "<tr><td><b>Terrain Name</b></td><td>%s</td></tr>", Config::getTerrainName().c_str());
	mg_printf(conn, "<tr><td><b>Password Protected</b></td><td>%s</td></tr>", Config::getPublicPassword().empty()?"No":"Yes");
	mg_printf(conn, "<tr><td><b>IP Address</b></td><td>%s</td></tr>", Config::getIPAddr().empty()?"Any":Config::getIPAddr().c_str());
	mg_printf(conn, "<tr><td><b>Port</b></td><td>%d</td></tr>", Config::getListenPort());
	mg_printf(conn, "<tr><td><b>Protocol Version</b></td><td>%d</td></tr>", RORNET_VERSION);

	bool advertised = Sequencer::getNotifier()->getAdvertised();

	char *serverMode = (char *)"AUTO";
	if(Config::getServerMode()==SERVER_LAN)
		serverMode = (char *)"LAN";
	else if(Config::getServerMode()==SERVER_INET && advertised)
		serverMode = (char *)"Public, Internet";
	else if(Config::getServerMode()==SERVER_INET && !advertised)
		serverMode = (char *)"Private, Internet";
	mg_printf(conn, "<tr><td><b>Server Mode</b></td><td>%s</td></tr>", serverMode);

	mg_printf(conn, "<tr><td><b>Registered at Master Server</b></td><td>%s</td></tr>", advertised?"Yes":"No");

	if(Config::getServerMode() == SERVER_INET && advertised)
	{
		int trustlevel = Sequencer::getNotifier()->getTrustLevel();
		mg_printf(conn, "<tr><td><b>Server TrustLevel</b></td><td>%d</td></tr>", trustlevel);
	}

	mg_printf(conn, "<tr><td><b>Print Console Stats</b></td><td>%s</td></tr>", Config::getPrintStats()?"Yes":"No");

	mg_printf(conn, "<tr><td><b>Scripting enabled</b></td><td>%s</td></tr>", Config::getEnableScripting()?"Yes":"No");
	if(Config::getEnableScripting())
		mg_printf(conn, "<tr><td><b>Script Name</b></td><td>%s</td></tr>", Config::getScriptName().c_str());

	mg_printf(conn, "<tr><td><b>Webserver Port</b></td><td>%d</td></tr>", Config::getWebserverPort());
	
	mg_printf(conn, "%s", "</table>");
	html_footer(conn);
}

static void show_map(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	html_header(conn);
	std::string userlist="<ul>";
#ifdef WITH_ANGELSCRIPT
	mg_printf(conn, "%s", "<div style='background-color:#aaaaaa;width:300px;height:300px;position:relative;'>");
	std::vector<client_t> clients = Sequencer::getClients();
	for(std::vector<client_t>::iterator it = clients.begin(); it != clients.end(); it++)
	{
		mg_printf(conn, "<div style='position:absolute;top:%dpx;left:%dpx;font-size:8pt;background-color:white;'>%s</div>\n",
			it->position.x / 3000.0f * 300.0f,
			it->position.y / 3000.0f * 300.0f,
			it->nickname
			);
		userlist += "<li>" + std::string(it->nickname) + "</li>";
	}
	mg_printf(conn, "%s", "</div>");
#else //WITH_ANGELSCRIPT
	mg_printf(conn, "%s", "not available");
#endif //WITH_ANGELSCRIPT
	userlist+="</ul>";
	mg_printf(conn, "<div style='margin:0px;padding:0px;width:340px;padding-left:5px;'>%s</div>", userlist.c_str());

	html_footer(conn);
}

static void show_log(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	html_header(conn);

	int loglevel = 3;
	char *loglevel_chr = mg_get_var(conn, "level");
	if (loglevel_chr != NULL)
	{
		loglevel = atoi(loglevel_chr);
		mg_free(loglevel_chr);
	}

	mg_printf(conn, "Level (currently: %s) - change: "
		"<a href='/log/?level=1'>Debug</a> -- "
		"<a href='/log/?level=2'>Verbose</a> -- "
		"<a href='/log/?level=3'>Info</a> -- "
		"<a href='/log/?level=4'>Warn</a> -- "
		"<a href='/log/?level=5'>Error</a>"
		, Logger::getLoglevelName(loglevel));
	std::string userlist="<ul>";
	mg_printf(conn, "%s", "<div style='background-color:#CCCCCC;font-family:monospace;'>");
	std::deque <log_save_t> loghistory = Logger::getLogHistory();
	for(std::deque <log_save_t>::const_iterator it=loghistory.begin(); it!=loghistory.end(); it++)
	{
		if(it->level < loglevel) continue; // filter log messages
		mg_printf(conn, "%s|t%02d|%5s|%s<br>", it->time.c_str(), it->threadid, Logger::getLoglevelName(it->level), it->msg.c_str());
	}
	mg_printf(conn, "%s", "</div>");
	html_footer(conn);
}


static void show_chat(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	html_header(conn);

	mg_printf(conn, "%s", "<div style='background-color:#CCCCCC;font-family:monospace;'>");
	std::deque <chat_save_t> chathistory = Sequencer::getChatHistory();
	for(std::deque <chat_save_t>::const_iterator it=chathistory.begin(); it!=chathistory.end(); it++)
	{
		mg_printf(conn, "%s|% 20s: %s<br>", it->time.c_str(), it->nick.c_str(), it->msg.c_str());
	}
	mg_printf(conn, "%s", "</div>");
	html_footer(conn);
}

static void show_userauth(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	html_header(conn);

	if(!Sequencer::getUserAuth()) return;

	//int loglevel = 3;
	char *clear_chr = mg_get_var(conn, "clear");
	if (clear_chr != NULL)
	{
		// CLEAR!
		Sequencer::getUserAuth()->clearCache();
	}

	mg_printf(conn, "%s", "<table border='1'>");

	std::map< std::string, std::pair<int, std::string> > users = Sequencer::getUserAuth()->getAuthCache();
	for(std::map< std::string, std::pair<int, std::string> >::const_iterator it = users.begin(); it != users.end(); it++)
	{
		char authst[5] = "/";
		if(it->second.first & AUTH_ADMIN) strcat(authst, "A");
		if(it->second.first & AUTH_MOD) strcat(authst, "M");
		if(it->second.first & AUTH_RANKED) strcat(authst, "R");
		if(it->second.first & AUTH_BOT) strcat(authst, "B");
		if(it->second.first & AUTH_BANNED) strcat(authst, "X");
		mg_printf(conn, "<tr><td><b>%s</b></td><td>%s</td></tr>", it->second.second.c_str(), authst);
	}
	mg_printf(conn, "%s", "</table>");

	mg_printf(conn, "%s", "<a href='/userauth/?clear=1'>Clear Cache</a>");

	html_footer(conn);
}
static void show_404(struct mg_connection *conn, const struct mg_request_info *request_info, void *user_data)
{
	mg_printf(conn, "%s", "HTTP/1.1 200 OK\r\n");
	mg_printf(conn, "%s", "Content-Type: text/plain\r\n\r\n");
	mg_printf(conn, "%s", "Oops. File not found! ");
}

static void signal_handler(int sig_num)
{
	switch (sig_num)
	{
#ifndef _WIN32
	case SIGCHLD:
		while (waitpid(-1, &sig_num, WNOHANG) > 0) ;
		break;
#endif /* !_WIN32 */
	case 0:
	default:
		break;
	}
}

int startWebserver(int port)
{
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, &signal_handler);
#endif /* !_WIN32 */

	ctx = mg_start();

	char portStr[30]="";
	sprintf(portStr, "%d", port);
	mg_set_option(ctx, "ports", portStr);
	mg_set_option(ctx, "dir_list", "no");
	

	/* Register an index page under two URIs */
	mg_set_uri_callback(ctx, "/", &show_index, NULL);
	mg_set_uri_callback(ctx, "/list", &show_list, NULL);
	mg_set_uri_callback(ctx, "/stats-general", &show_stats_general, NULL);
	mg_set_uri_callback(ctx, "/config", &show_configuration, NULL);
	mg_set_uri_callback(ctx, "/map", &show_map, NULL);
	mg_set_uri_callback(ctx, "/log/*", &show_log, NULL);
	mg_set_uri_callback(ctx, "/chat/*", &show_chat, NULL);
	mg_set_uri_callback(ctx, "/userauth/*", &show_userauth, NULL);

	mg_set_error_callback(ctx, 404, show_404, NULL);
	return 0;
}

int stopWebserver()
{
	mg_stop(ctx);
	return 0;
}
