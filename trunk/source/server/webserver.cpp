#ifdef WITH_WEBSERVER
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

#include <ctemplate/template.h>
#include <json/json.h>

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

std::string formatBytes(double bytes, bool persec=false)
{
	char res[256]="";
	if(bytes < 1)
		sprintf(res, "0");
	else if(bytes <= 1024)
		sprintf(res, "%0.2f B", bytes);
	else if(bytes > 1024 && bytes <= 1048576)
		sprintf(res, "%0.2f KB", bytes / 1024.0f);
	else if(bytes > 1048576 && bytes <= 1073741824)
		sprintf(res, "%0.2f MB", bytes / 1024.0f / 1024.0f);
	else //if(bytes > 1073741824 && bytes <= 1099511627776)
		sprintf(res, "%0.2f GB", bytes / 1024.0f / 1024.0f / 1024.0f);
	//else if(bytes > 1099511627776)
	//	sprintf(res, "%0.2f TB", bytes / 1024.0f / 1024.0f / 1024.0f / 1024.0f);
	if( !(bytes < 1) && persec)
		strcat(res, "/s");
	return std::string(res);
}

static ctemplate::TemplateDictionary *getTemplateDict(std::string title)
{
	ctemplate::TemplateDictionary *dict = new ctemplate::TemplateDictionary("website");
	
	// if you remove the following line, it will enforce reading from the disc -> nice to create the templates
	ctemplate::Template::ClearCache();
	
	dict->SetValue("TITLE", "Rigs of Rods Server - " + title);
	
	ctemplate::TemplateDictionary* dict_header = dict->AddIncludeDictionary("HEADER");
	dict_header->SetFilename("webserver/templates/header.html");
	ctemplate::TemplateDictionary* dict_footer = dict->AddIncludeDictionary("FOOTER");
	dict_footer->SetFilename("webserver/templates/footer.html");
	return dict;
}

static void renderTemplate(ctemplate::TemplateDictionary *dict, struct mg_connection *conn, std::string templatefile)
{
	std::string output;
	ctemplate::ExpandTemplate(templatefile, ctemplate::DO_NOT_STRIP, dict, &output);

	mg_write(conn, output.c_str(), output.size());

	delete(dict);
	dict=0;
}

static void show_index(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	ctemplate::TemplateDictionary *dict = getTemplateDict("Overview");

	dict->SetValue("FOO", "test123");

	renderTemplate(dict, conn, "webserver/templates/overview.html");
}

#define ranrange(a, b) (int)((a) + rand()/(RAND_MAX + 1.0) * ((b) - (a) + 1))
static void data_stats_traffic(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	Json::Value root;   // will contains the root value after parsing.
	Json::Value results;
	for(int i=0;i<100;i++)
	{
		double val = ranrange(0,100);
		Json::Value result;
		result["Name"]  = i;
		result["Value"] = (int)val;
		results.append(result);
	}
	root["Traffic"] = results;
	Json::StyledWriter writer;
	std::string output = writer.write( root );

	mg_write(conn, output.c_str(), output.size());
}

static Json::Value getConfValue(std::string name, std::string value)
{
	Json::Value result;
	result["name"]  = name;
	result["value"] = value;
	return result;
}

static Json::Value getConfValue(std::string name, int value)
{
	Json::Value result;
	result["name"]  = name;
	result["value"] = value;
	return result;
}

static void data_configuration(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	static int i=0;
	Json::Value root;   // will contains the root value after parsing.
	Json::Value results;
	results.append(getConfValue("Max Clients", Config::getMaxClients()));
	results.append(getConfValue("Server Name", Config::getServerName()));
	results.append(getConfValue("Terrain Name", Config::getTerrainName()));
	results.append(getConfValue("Password Protected", Config::getPublicPassword().empty()?"No":"Yes"));
	results.append(getConfValue("IP Address", Config::getIPAddr()=="0.0.0.0"?"0.0.0.0 (Any)":Config::getIPAddr()));
	results.append(getConfValue("Listening Port", Config::getListenPort()));
	results.append(getConfValue("Protocol Version", std::string(RORNET_VERSION)));

	bool advertised = Sequencer::getNotifier()->getAdvertised();
	std::string serverMode = "AUTO";
	if(Config::getServerMode() == SERVER_LAN)
		serverMode = "LAN";
	else if(Config::getServerMode() == SERVER_INET && advertised)
		serverMode = "Public, Internet";
	else if(Config::getServerMode() == SERVER_INET && !advertised)
		serverMode = "Private, Internet";

	results.append(getConfValue("Advertized on Master Server", advertised?"Yes":"No"));
	results.append(getConfValue("Server Mode", serverMode));
	
	if(Config::getServerMode() == SERVER_INET && advertised)
	{
		int trustlevel = Sequencer::getNotifier()->getTrustLevel();
		results.append(getConfValue("Server Trust Level", trustlevel));
	}

	results.append(getConfValue("Print Console Stats", Config::getPrintStats()?"Yes":"No"));

#ifdef WITH_ANGELSCRIPT
	results.append(getConfValue("Scripting support", "Yes"));
	results.append(getConfValue("Scripting enabled", Config::getEnableScripting()?"Yes":"No"));
	if(Config::getEnableScripting())
		results.append(getConfValue("Script Name in use", Config::getScriptName()));
#else // WITH_ANGELSCRIPT
	results.append(getConfValue("Scripting support", "No"));
#endif // WITH_ANGELSCRIPT

	results.append(getConfValue("Webserver Port", Config::getWebserverPort()));

	root["ResultSet"]["Result"] = results;
	root["ResultSet"]["totalResultsAvailable"] = results.size();
	root["ResultSet"]["totalResultsReturned"] = results.size();
	root["ResultSet"]["firstResultPosition"] = 1;

	Json::StyledWriter writer;
	std::string output = writer.write( root );

	mg_write(conn, output.c_str(), output.size());
}

static void data_players(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	Json::Value rows;

	std::vector<client_t> clients = Sequencer::getClients();
	int row_counter=0;
	for(std::vector<client_t>::iterator it = clients.begin(); it != clients.end(); it++,row_counter++)
	{
		Json::Value row;

		if (it->status == FREE)
		{
			row["slot"] = it->slotnum;
			row["status"] = "FREE";
			rows.append(row);
		} else if (it->status == BUSY)
		{
			row["slot"] = it->slotnum;
			row["status"] = "BUSY";
			rows.append(row);
		} else if (it->status == USED)
		{
			// some auth identifiers
			std::string authst = "none";
			if(it->authstate & AUTH_BANNED) authst = "banned";
			if(it->authstate & AUTH_BOT)    authst = "bot";
			if(it->authstate & AUTH_RANKED) authst = "ranked";
			if(it->authstate & AUTH_MOD)    authst = "moderator";
			if(it->authstate & AUTH_ADMIN)  authst = "admin";

			char playerColour[128] = "";
			getPlayerColour(it->colournumber, playerColour);

			row["slot"]     = it->slotnum;
			row["status"]   = "USED";
			row["uid"]      = it->uid;
			row["ip"]       = std::string(it->ip_addr);
			row["name"]     = std::string(it->nickname);
			row["auth"]     = authst;

			// get traffic stats for all streams
			double bw_in=0, bw_out=0, bw_in_rate=0, bw_out_rate=0;
			double bw_drop_in=0, bw_drop_out=0, bw_drop_in_rate=0, bw_drop_out_rate=0;
			// tit = traffic iterator, not what you might think ...
			for(std::map<unsigned int, stream_traffic_t>::iterator tit = it->streams_traffic.begin(); tit != it->streams_traffic.end(); tit++)
			{
				bw_in            += tit->second.bandwidthIncoming;
				bw_out           += tit->second.bandwidthOutgoing;
				bw_in_rate       += tit->second.bandwidthIncomingRate;
				bw_out_rate      += tit->second.bandwidthOutgoingRate;
				
				bw_drop_in       += tit->second.bandwidthDropIncoming;
				bw_drop_out      += tit->second.bandwidthDropOutgoing;
				bw_drop_in_rate  += tit->second.bandwidthDropIncomingRate;
				bw_drop_out_rate += tit->second.bandwidthDropOutgoingRate;
			}

			// traffic things
			row["bw_normal_sum_up"]      = formatBytes(bw_in);
			row["bw_normal_sum_down"]    = formatBytes(bw_out);
			row["bw_normal_rate_up"]     = formatBytes(bw_in_rate, true);
			row["bw_normal_rate_down"]   = formatBytes(bw_out_rate, true);

			row["bw_drop_sum_up"]        = formatBytes(bw_drop_in);
			row["bw_drop_sum_down"]      = formatBytes(bw_drop_out);
			row["bw_drop_rate_up"]       = formatBytes(bw_drop_in_rate, true);
			row["bw_drop_rate_down"]     = formatBytes(bw_drop_out_rate, true);

			rows.append(row);
			
			// now add the streams themself to the table
			for(std::map<unsigned int, stream_register_t>::iterator sit = it->streams.begin(); sit != it->streams.end(); sit++)
			{
				Json::Value trow;

				std::string typeStr = "unkown";
				if(sit->second.type == 0) typeStr = "Vehicle";
				if(sit->second.type == 1) typeStr = "Character";
				if(sit->second.type == 2) typeStr = "AI Traffic";
				if(sit->second.type == 3) typeStr = "Chat";

				stream_traffic_t *traf = 0;
				if(it->streams_traffic.find(sit->first) != it->streams_traffic.end())
					traf = &(it->streams_traffic.find(sit->first)->second);

				char uidtmp[128] = "";
				sprintf(uidtmp, "%03d:%02d", it->uid, sit->first);

				trow["slot"]     = "";
				trow["status"]   = sit->second.status;
				trow["uid"]      = std::string(uidtmp);
				trow["ip"]       = typeStr;
				trow["name"]     = std::string(sit->second.name);
				trow["auth"]     = "";
				if(traf)
				{
					trow["bw_normal_sum_up"]      = formatBytes(traf->bandwidthIncoming);
					trow["bw_normal_sum_down"]    = formatBytes(traf->bandwidthOutgoing);
					trow["bw_normal_rate_up"]     = formatBytes(traf->bandwidthIncomingRate , true);
					trow["bw_normal_rate_down"]   = formatBytes(traf->bandwidthOutgoingRate, true);

					trow["bw_drop_sum_up"]        = formatBytes(traf->bandwidthDropIncoming);
					trow["bw_drop_sum_down"]      = formatBytes(traf->bandwidthDropOutgoing);
					trow["bw_drop_rate_up"]       = formatBytes(traf->bandwidthDropIncomingRate, true);
					trow["bw_drop_rate_down"]     = formatBytes(traf->bandwidthDropOutgoingRate, true);
				}
				rows.append(trow);
			}

		}

	}

	Json::Value root;   // will contains the root value after parsing.
	root["ResultSet"]["totalResultsAvailable"] = row_counter;
	root["ResultSet"]["totalResultsReturned"]  = row_counter;
	root["ResultSet"]["firstResultPosition"]   = 1;
	root["ResultSet"]["Result"] = rows;

	Json::StyledWriter writer;
	std::string output = writer.write( root );

	mg_write(conn, output.c_str(), output.size());
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
	mg_set_uri_callback(ctx, "/data/players/", &data_players, NULL);
	mg_set_uri_callback(ctx, "/data/stats/traffic", &data_stats_traffic, NULL);
	mg_set_uri_callback(ctx, "/data/configuration/", &data_configuration, NULL);

	mg_set_error_callback(ctx, 404, show_404, NULL);
	return 0;
}

int stopWebserver()
{
	mg_stop(ctx);
	return 0;
}

#endif //WITH_WEBSERVER
