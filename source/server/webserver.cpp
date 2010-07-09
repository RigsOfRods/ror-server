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

int formatBytes(double bytes, char *res, bool persec=false)
{
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
		strcat(res, " /s");
	return 0;
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

	mg_printf(conn, "%s", output.c_str());

	delete(dict);
	dict=0;
}

static void show_index(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	ctemplate::TemplateDictionary *dict = getTemplateDict("Overview");

	dict->SetValue("FOO", "test123");

	renderTemplate(dict, conn, "webserver/templates/overview.html");
}


static void data_players(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	static int i=0;
	Json::Value root;   // will contains the root value after parsing.
	root["ResultSet"]["totalResultsAvailable"] = 4;
	root["ResultSet"]["totalResultsReturned"] = 4;
	root["ResultSet"]["firstResultPosition"] = 1;
	Json::Value results;
	for(int j=0;j<10;j++,i++)
	{
		Json::Value result;
		char tmp[255];
		sprintf(tmp, "%s%d", "test", i);
		result["name"] = std::string(tmp);
		
		results.append(result);
	}
	root["ResultSet"]["Result"] = results;

	Json::StyledWriter writer;
	std::string output = writer.write( root );

	mg_printf(conn, "%s", output.c_str());
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

	mg_set_error_callback(ctx, 404, show_404, NULL);
	return 0;
}

int stopWebserver()
{
	mg_stop(ctx);
	return 0;
}

#endif //WITH_WEBSERVER
