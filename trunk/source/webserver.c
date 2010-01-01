#include "webserver.h"

#include "sequencer.h"
#include "messaging.h"

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
	else if(bytes > 1073741824 && bytes <= 1099511627776)
		sprintf(res, "%0.2f GB", bytes / 1024.0f / 1024.0f / 1024.0f);
	else if(bytes > 1099511627776)
		sprintf(res, "%0.2f TB", bytes / 1024.0f / 1024.0f / 1024.0f / 1024.0f);
	return 0;
}

static void show_index(struct mg_connection *conn, const struct mg_request_info *request_info, void *data)
{
	mg_printf(conn, "%s",
		"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
		"<html>"
		"<head>"
		"<meta http-equiv='refresh' content='10' />"
		"</head>"
		"<body>");

	mg_printf(conn, "<h2>Rigs of Rods Server (%s)</h2>", RORNET_VERSION);

	/*
	mg_printf(conn, "<li><code>REQUEST_METHOD: %s "
	    "REQUEST_URI: \"%s\" QUERY_STRING: \"%s\""
	    " REMOTE_ADDR: %lx REMOTE_USER: \"(null)\"</code><hr>",
	    request_info->request_method, request_info->uri,
	    request_info->query_string ? request_info->query_string : "(null)",
	    request_info->remote_ip);
	*/
	
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
	for(std::vector<client_t>::iterator it = clients.begin(); it != clients.end(); it++)
	{
		// some auth identifiers
		char authst[5] = "/";
		if(it->authstate & AUTH_ADMIN) strcat(authst, "A");
		if(it->authstate & AUTH_MOD) strcat(authst, "M");
		if(it->authstate & AUTH_RANKED) strcat(authst, "R");
		if(it->authstate & AUTH_BOT) strcat(authst, "B");
		if(it->authstate & AUTH_BANNED) strcat(authst, "X");

		// construct screen
		if (it->status == FREE)
		{
			mg_printf(conn, "<tr><td>%i</td><td colspan='6'>free</td></tr>", it->slotnum);
		} else if (it->status == BUSY)
		{
			mg_printf(conn, "<tr><td>%i</td><td colspan='6'>busy</td></tr>", it->slotnum);
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
				char *typeStr = "unkown";
				if(sit->second.type == 0) typeStr = "Vehicle";
				if(sit->second.type == 1) typeStr = "Character";
				if(sit->second.type == 2) typeStr = "AI Traffic";
				if(sit->second.type == 3) typeStr = "Chat";

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
	mg_printf(conn, "%s", "</table border><hr>");
	mg_printf(conn, "%s", "<small>Authentication Types: R=Ranked, M=Moderator, A=Admin, B=Bot, X=Banned<br>"
		"Traffic format : [Incoming], [Outgoing]"
		"</small><hr>");


	int timediff = Messaging::getTime() - Sequencer::getStartTime();
	int uphours = timediff/60/60;
	int upminutes = (timediff-(uphours*60*60))/60;
	int upseconds = (timediff-(uphours*60*60)-(upminutes*60));

	char tmp1[128]="", tmp2[128]="";

	mg_printf(conn, "%s", "<h3>Statistics</h3>");
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
	mg_printf(conn, "%s", "</body></html>");
}

/*
 * This callback function is used to show how to handle 404 error
 */
static void
show_404(struct mg_connection *conn,
		const struct mg_request_info *request_info,
		void *user_data)
{
	mg_printf(conn, "%s", "HTTP/1.1 200 OK\r\n");
	mg_printf(conn, "%s", "Content-Type: text/plain\r\n\r\n");
	mg_printf(conn, "%s", "Oops. File not found! ");
	mg_printf(conn, "%s", "This is a custom error handler.");
}

/*
 * Make sure we have ho zombies from CGIs
 */
static void
signal_handler(int sig_num)
{
	switch (sig_num) {
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
	mg_set_error_callback(ctx, 404, show_404, NULL);
	return 0;
}

int stopWebserver()
{
	mg_stop(ctx);
	return 0;
}