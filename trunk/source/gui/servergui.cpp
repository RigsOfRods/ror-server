#include <vector>
#include <deque>

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/gauge.h>
#include <wx/scrolbar.h>
#include <wx/grid.h>
#include <wx/treectrl.h>
#include <wx/tooltip.h>
#include <wx/mediactrl.h>
#include <wx/intl.h>
#include <wx/textfile.h>
#include <wx/cmdline.h>
#include <wx/radiobox.h>
#include <wx/html/htmlwin.h>
#include <wx/uri.h>
#include <wx/dir.h>
#include <wx/fs_inet.h>
#include <wx/scrolwin.h>
#include <wx/log.h>
#include <wx/timer.h>
#include <wx/listctrl.h>
#include <wx/menu.h>

#include "statpict.h"

#include "rornet.h"
#include "sequencer.h"
#include "messaging.h"
#include "logger.h"
#include "config.h"
#include "userauth.h"

#include "logger.h"
#include "sha1_util.h"
#include "sha1.h"
#include "utils.h"

// xpm images
#include "server.xpm"
#include "play.xpm"
#include "play_dis.xpm"
#include "shutdown.xpm"
#include "shutdown_dis.xpm"
#include "stop.xpm"
#include "stop_dis.xpm"

class MyApp : public wxApp
{
public:
	virtual bool OnInit();
	virtual bool OnExceptionInMainLoop();
};

typedef struct log_queue_element_t
{
	int level;
	UTFString msg;
	UTFString msgf;
} log_queue_element_t;

// Define a new frame type: this is going to be our main frame
class MyDialog : public wxDialog
{
public:
	// ctor(s)
	MyDialog(const wxString& title, MyApp *_app);

	void logCallback(int level, UTFString msg, UTFString msgf); //called by callback function
private:
	MyApp *app;
	wxButton *startBtn, *stopBtn, *exitBtn, *pubipBtn;
	wxTextCtrl *txtConsole, *ipaddr, *port, *slots, *passwd, *terrain, *sname, *logfilename, *adminuid;
	wxComboBox *smode, *logmode;
	wxNotebook *nbook;
	wxListCtrl *slotlist;
	wxScrolledWindow *settingsPanel;
	wxPanel *logPanel, *playersPanel;
	wxTimer *timer1;
	wxToolBar *tb;
	void OnQuit(wxCloseEvent &event);
	int loglevel;
	int secondsCounter;
	pthread_t notifyThread;
	bool server_is_running;
	
	Mutex log_queue_mutex;
	std::deque <log_queue_element_t> log_queue;
	
	void OnBtnStart(wxCommandEvent& event);
	void OnBtnStop(wxCommandEvent& event);
	void OnBtnExit(wxCommandEvent& event);
	void OnBtnPubIP(wxCommandEvent& event);

	void OnContextMenu(wxContextMenuEvent& event);

	int startServer();
	int stopServer();
	void updateLogTab();
	void updatePlayerList();

	void OnTimer(wxTimerEvent& event);

	DECLARE_EVENT_TABLE()
};

enum {btn_start, btn_stop, btn_exit, EVT_timer1, btn_pubip, ctxtmenu_kick, ctxtmenu_ban, ctxtmenu_pm};

BEGIN_EVENT_TABLE(MyDialog, wxDialog)
	EVT_CLOSE(MyDialog::OnQuit)
	EVT_BUTTON(btn_start, MyDialog::OnBtnStart)
	EVT_BUTTON(btn_stop,  MyDialog::OnBtnStop)
	EVT_BUTTON(btn_exit,  MyDialog::OnBtnExit)
	EVT_BUTTON(btn_pubip, MyDialog::OnBtnPubIP)
	EVT_TIMER(EVT_timer1, MyDialog::OnTimer)

	EVT_TOOL(btn_start, MyDialog::OnBtnStart)
	EVT_TOOL(btn_stop,  MyDialog::OnBtnStop)
	EVT_TOOL(btn_exit,  MyDialog::OnBtnExit)
END_EVENT_TABLE()

IMPLEMENT_APP(MyApp)



inline wxString conv(const char *s)
{
	return wxString(s, wxConvUTF8);
}

inline wxString conv(const std::string& s)
{
	return wxString(s.c_str(), wxConvUTF8);
}

inline std::string conv(const wxString& s)
{
	return std::string(s.mb_str(wxConvUTF8));
}

void *s_notifyThreadStart(void* vid)
{
    STACKLOG;
	Sequencer::notifyRoutine();
	return NULL;
}

MyDialog *dialogInstance = 0; // we need to use this to be able to forward the messages
MyApp *myApp = 0;
// 'Main program' equivalent: the program execution "starts" here
bool MyApp::OnInit()
{
	if ( !wxApp::OnInit() )
		return false;

	myApp = this;
	MyDialog *dialog = new MyDialog(_("Rigs of Rods Dedicated Server"), this);
	dialog->Show(true);
	SetTopWindow(dialog);
	SetExitOnFrameDelete(false);
	return true;
}

bool MyApp::OnExceptionInMainLoop()
{
	// something went wrong :/
	try
	{
		throw;
	}
	catch ( std::runtime_error e )
	{
		wxMessageBox(wxString::Format("Unhandled exception caught, program will terminate. \n%s", e.what()),
		             wxT("Unhandled exception"), wxOK | wxICON_ERROR);
 	}
	catch ( ... )
	{
		wxMessageBox(wxT("Unhandled exception caught, program will terminate."),
		             wxT("Unhandled exception"), wxOK | wxICON_ERROR);
	}
	
	return false;
}

void pureLogCallback(int level, UTFString msg, UTFString msgf)
{
	dialogInstance->logCallback(level, msg, msgf);
}

void MyDialog::logCallback(int level, UTFString msg, UTFString msgf)
{
	if(level < loglevel) return;
	
	pthread_mutex_lock(log_queue_mutex.getRaw());
	if(log_queue.size() > 100)
		log_queue.pop_front();
	log_queue_element_t h;
	h.level = level;
	h.msg = msg;
	h.msgf = msgf;
	log_queue.push_back(h);
	pthread_mutex_unlock(log_queue_mutex.getRaw());
}

MyDialog::MyDialog(const wxString& title, MyApp *_app) : 
	wxDialog(NULL, wxID_ANY, title,  wxDefaultPosition, wxSize(600, 600), wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{
	app=_app;
	loglevel=LOG_INFO;
	dialogInstance = this;

	secondsCounter=55; // trigger at start

	SetMinSize(wxSize(600,600));
	//SetMaxSize(wxSize(500,500));
	SetWindowStyle(wxRESIZE_BORDER | wxCAPTION);

	timer1 = new wxTimer(this, EVT_timer1);

	wxSizer *mainsizer_0 = new wxBoxSizer(wxHORIZONTAL);
	mainsizer_0->SetSizeHints(this);
	this->SetSizer(mainsizer_0);

	// add left part
	wxSizer *mainsizer = new wxBoxSizer(wxVERTICAL);
	mainsizer_0->Add(mainsizer, 1, wxGROW);

	// add main toolbar (right part)
	wxPanel *tbp = new wxPanel(this,wxID_ANY, wxPoint(0, 0), wxSize(100, 600));
	tb = new wxToolBar(tbp, wxID_ANY, wxPoint(0, 0), wxSize(100, 600), wxTB_VERTICAL|wxTB_TEXT|wxTB_FLAT);
	mainsizer_0->Add(tbp, 0, wxALL);

	tb->SetToolBitmapSize(wxSize(48,48));
	tb->SetToolSeparation(10);

	// icons from http://www.iconarchive.com/category/application/button-icons-by-deleket.html

	tb->AddTool(btn_start, _T("Start"), wxBitmap(play_xpm) , wxBitmap(play_dis_xpm), wxITEM_NORMAL, _T("Start server"), _T("Start server"));
	tb->AddTool(btn_stop, _T("Stop"), wxBitmap(stop_xpm) , wxBitmap(stop_dis_xpm), wxITEM_NORMAL, _T("Stop server"), _T("Stop server"));
	tb->AddSeparator();
	tb->AddTool(btn_exit, _T("Shutdown"), wxBitmap(shutdown_xpm) , wxBitmap(shutdown_dis_xpm), wxITEM_NORMAL, _T("Shutdown server"), _T("Shutdown server"));
	tb->Realize();
	tb->EnableTool(btn_stop, false);


	// head image - using xpm
	wxStaticPicture *imagePanel = new wxStaticPicture(this, -1, wxBitmap(config_xpm), wxPoint(0, 0), wxSize(500, 100), wxNO_BORDER);
	mainsizer->Add(imagePanel, 0, wxGROW);

	nbook=new wxNotebook(this, wxID_ANY);
	mainsizer->Add(nbook, 1, wxGROW);

	///// settings
	settingsPanel=new wxScrolledWindow(nbook, wxID_ANY);
	//settingsPanel->SetVirtualSize(300, 800);
	settingsPanel->SetScrollbars(0, 10, 1,0, 0, 0);

	wxSizer *settingsSizer = new wxBoxSizer(wxVERTICAL);
	wxFlexGridSizer *grid_sizer = new wxFlexGridSizer(3, 2, 10, 20);
	wxStaticText *dText;

	// server version
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Server Version:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	sname = new wxTextCtrl(settingsPanel, wxID_ANY, conv(RORNET_VERSION), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	sname->Disable();
	grid_sizer->Add(sname, 0, wxGROW);

	// server name
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Server Name:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	sname = new wxTextCtrl(settingsPanel, wxID_ANY, _("Default Server"), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(sname, 0, wxGROW);

	// mode
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Mode:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);

	wxString choices[2];
	choices[0] = _("LAN");
	choices[1] = _("Internet");
	smode = new wxComboBox(settingsPanel, wxID_ANY, _("LAN"), wxDefaultPosition, wxDefaultSize, 2, choices, wxCB_DROPDOWN|wxCB_READONLY);
	grid_sizer->Add(smode, 0, wxGROW);

	// IP
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("IP:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	wxSizer *ipaddrSizer = new wxBoxSizer(wxVERTICAL);
	ipaddr = new wxTextCtrl(settingsPanel, wxID_ANY, _("0.0.0.0"), wxDefaultPosition, wxDefaultSize);
	ipaddrSizer->Add(ipaddr, 0, wxGROW);
	pubipBtn = new wxButton(settingsPanel, btn_pubip, _("get public IP address"));
	ipaddrSizer->Add(pubipBtn, 0, wxGROW);
	grid_sizer->Add(ipaddrSizer, 0, wxGROW);

	// Port
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Port:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	port = new wxTextCtrl(settingsPanel, wxID_ANY, _("12000"), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(port, 0, wxGROW);

	// Port
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Password:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	passwd = new wxTextCtrl(settingsPanel, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(passwd, 0, wxGROW);

	// playercount
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Slots:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	slots = new wxTextCtrl(settingsPanel, wxID_ANY, _("16"), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(slots, 0, wxGROW);

	// terrain
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Terrain:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	terrain = new wxTextCtrl(settingsPanel, wxID_ANY, _("any"), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(terrain, 0, wxGROW);

	// debuglevel
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("LogLevel:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	wxString choices_log[6];
	choices_log[0] = _("STACK");
	choices_log[1] = _("DEBUG");
	choices_log[2] = _("VERBOSE");
	choices_log[3] = _("INFO");
	choices_log[4] = _("WARN");
	choices_log[5] = _("ERRROR");
	logmode = new wxComboBox(settingsPanel, wxID_ANY, _("INFO"), wxDefaultPosition, wxDefaultSize, 6, choices_log, wxCB_DROPDOWN|wxCB_READONLY);
	grid_sizer->Add(logmode, 0, wxGROW);

	// logfilename
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Log Filename:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	logfilename = new wxTextCtrl(settingsPanel, wxID_ANY, _("server.log"), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(logfilename, 0, wxGROW);

	// adminuid
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Admin user token:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	adminuid = new wxTextCtrl(settingsPanel, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(adminuid, 0, wxGROW);

	// some buttons
	//startBtn = new wxButton(settingsPanel, btn_start, _("START"));
	//grid_sizer->Add(startBtn, 0, wxGROW);

	//stopBtn = new wxButton(settingsPanel, btn_stop, _("STOP"));
	//stopBtn->Disable();
	//grid_sizer->Add(stopBtn, 0, wxGROW);

	settingsSizer->Add(grid_sizer, 0, wxGROW, 10);
	settingsPanel->SetSizer(settingsSizer);
	nbook->AddPage(settingsPanel, _("Settings"), true);

	///// Log
	logPanel=new wxPanel(nbook, wxID_ANY);
	wxSizer *logSizer = new wxBoxSizer(wxVERTICAL);
	txtConsole = new wxTextCtrl(logPanel, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, wxNO_BORDER|wxTE_MULTILINE|wxTE_READONLY|wxTE_RICH|wxTE_RICH2|wxTE_DONTWRAP);
	logSizer->Add(txtConsole, 1, wxGROW);

	// use monospace for the log
	wxFont font = txtConsole->GetFont();
	font = wxFont(font.GetPointSize()-1, wxTELETYPE, font.GetStyle(), font.GetWeight(), font.GetUnderlined()); 
	txtConsole->SetFont(font);
	
	logPanel->SetSizer(logSizer);
	nbook->AddPage(logPanel, _("Log"), false);


	///// player list
	playersPanel=new wxPanel(nbook, wxID_ANY);
	wxSizer *playersSizer = new wxBoxSizer(wxVERTICAL);
	slotlist = new wxListCtrl(playersPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,wxLC_REPORT|wxLC_SINGLE_SEL);
	slotlist->InsertColumn(0, _("Slot"), wxLIST_FORMAT_LEFT, 30);
	slotlist->InsertColumn(1, _("Status"), wxLIST_FORMAT_LEFT, 50);
	slotlist->InsertColumn(2, _("UID"), wxLIST_FORMAT_LEFT, 30);
	slotlist->InsertColumn(3, _("IP"), wxLIST_FORMAT_LEFT, 100);
	slotlist->InsertColumn(4, _("Auth"), wxLIST_FORMAT_LEFT, 40);
	slotlist->InsertColumn(5, _("Nick"), wxLIST_FORMAT_LEFT, 100);
	slotlist->Connect(wxID_ANY, wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(MyDialog::OnContextMenu), NULL, this);

	playersSizer->Add(slotlist, 1, wxGROW);
	playersPanel->SetSizer(playersSizer);
	nbook->AddPage(playersPanel, _("Slots"), false);

	// main sizer again
	//exitBtn = new wxButton(this, btn_exit, _("EXIT"));
	//mainsizer->Add(exitBtn, 0, wxGROW);

	server_is_running = false;

	// add the logger callback
	Logger::setCallback(&pureLogCallback);
	Logger::log(LOG_INFO, "GUI log callback working");
	updateLogTab();

	// centers dialog window on the screen
	Show();
	SetSize(580,600);
	Centre();
}

void MyDialog::OnTimer(wxTimerEvent& event)
{
	STACKLOG;
	// update the player tab
	// TODO: add a flag, so this is only updated when needed
	updatePlayerList();
	
	// update the log tab
	updateLogTab();

	secondsCounter++;
	if(secondsCounter > 60)
	{
		if(Config::getServerMode() == SERVER_LAN)
		{
			Messaging::updateMinuteStats();
			Sequencer::updateMinuteStats();

			// broadcast our "i'm here" signal
			Messaging::broadcastLAN();
		}
		secondsCounter=0;
	}
}

void MyDialog::updateLogTab()
{
	if( !log_queue.empty() )
	{
		pthread_mutex_lock(log_queue_mutex.getRaw());
		txtConsole->Freeze();
		int lines = 0;
		while( !log_queue.empty() )
		{
			log_queue_element_t h = log_queue.front();
			log_queue.pop_front();

			if(h.level == LOG_INFO)
				txtConsole->SetDefaultStyle(wxTextAttr(wxColour(0,150,0)));
			else if(h.level == LOG_VERBOSE)
				txtConsole->SetDefaultStyle(wxTextAttr(wxColour(0,180,0)));
			else if(h.level == LOG_ERROR)
				txtConsole->SetDefaultStyle(wxTextAttr(wxColour(150,0,0)));
			else if(h.level == LOG_WARN)
				txtConsole->SetDefaultStyle(wxTextAttr(wxColour(180,130,0)));
			else if(h.level == LOG_DEBUG)
				txtConsole->SetDefaultStyle(wxTextAttr(wxColour(180,150,0)));
			else if(h.level == LOG_STACK)
				txtConsole->SetDefaultStyle(wxTextAttr(wxColour(150,150,150)));

			UTFString u = h.msgf;
			for(unsigned int i=0; i < u.size(); i++)
				if( u[i] == '\n' )
					++lines;

			wxString wmsg = wxString(u.asWStr());
			txtConsole->AppendText(wmsg);
		}
		txtConsole->ScrollLines(lines + 1);
		// txtConsole->ShowPosition(txtConsole->GetLastPosition());
		txtConsole->Thaw();
		pthread_mutex_unlock(log_queue_mutex.getRaw());
	}
}

void MyDialog::updatePlayerList()
{
	slotlist->Freeze();
	std::vector<client_t> clients = Sequencer::getClients();
	for(unsigned int i=0; i<Config::getMaxClients();i++)
	{
		slotlist->SetItem(i, 0, wxString::Format(wxT("%d"),i));

		if(i>=clients.size())
		{
			if( slotlist->GetItemText(i, 1) == _("FREE") )
				break;
			slotlist->SetItem(i, 1, _("FREE"));
			slotlist->SetItem(i, 2, wxString());
			slotlist->SetItem(i, 3, wxString());
			slotlist->SetItem(i, 4, wxString());
			slotlist->SetItem(i, 5, wxString());
			//slotlist->SetItem(i, 6, wxString());
			continue;
		}

		if(clients[i].status == FREE)
			slotlist->SetItem(i, 1, _("FREE"));
		if(clients[i].status == BUSY)
			slotlist->SetItem(i, 1, _("BUSY"));
		if(clients[i].status == USED)
			slotlist->SetItem(i, 1, _("USED"));

		slotlist->SetItem(i, 2, wxString::Format(wxT("%d"),clients[i].user.uniqueid));
		slotlist->SetItem(i, 3, conv(clients[i].ip_addr));

		char authst[5] = "";
		if(clients[i].user.authstatus & AUTH_ADMIN) strcat(authst, "A");
		if(clients[i].user.authstatus & AUTH_MOD) strcat(authst, "M");
		if(clients[i].user.authstatus & AUTH_RANKED) strcat(authst, "R");
		if(clients[i].user.authstatus & AUTH_BOT) strcat(authst, "B");
		if(clients[i].user.authstatus & AUTH_BANNED) strcat(authst, "X");
		slotlist->SetItem(i, 4, conv(authst));
		wxString un = wxString(tryConvertUTF(clients[i].user.username).asWStr());
		slotlist->SetItem(i, 5, un);
	}
	slotlist->Thaw();
}

void MyDialog::OnQuit(wxCloseEvent &event)
{
	Destroy();
	exit(0);
}

void MyDialog::OnContextMenu(wxContextMenuEvent& event)
{
	if( server_is_running )
	{
		// which client row is selected?
		long item = slotlist->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if ( item == -1 )
			return;
		
		// get the uid of that row
		int uid = wxAtoi(slotlist->GetItemText(item, 2));
		wxString username = slotlist->GetItemText(item, 5);
		
		if( uid <= 0 )
			return;
		
		// build the menu
		wxPoint point = event.GetPosition();
		point = ScreenToClient(point);
		wxMenu menu;
		menu.SetTitle(username);
		menu.Append(ctxtmenu_kick, wxT("&kick"));
		menu.Append(ctxtmenu_ban, wxT("&ban"));
		menu.AppendSeparator();
		menu.Append(ctxtmenu_pm, wxT("&private message"));

		// get the selection of the user
		const int rc = GetPopupMenuSelectionFromUser(menu, point);
		
		// interpret the selection and take the required action
		if( rc == ctxtmenu_kick )
		{
			wxString msg;
			msg.sprintf("Are you sure that you wish to kick user '%s'?",username);
			int answer = wxMessageBox(msg, _("Confirmation required"), wxYES_NO | wxCANCEL, dialogInstance);
			if (answer == wxYES)
				Sequencer::disconnect(uid, "Kicked by host.", false);
		}
		else if( rc == ctxtmenu_ban )
		{
			wxString msg;
			msg.sprintf("Are you sure that you wish to kick and ban user '%s'?",username);
			int answer = wxMessageBox(msg, _("Confirmation required"), wxYES_NO | wxCANCEL, dialogInstance);
			if (answer == wxYES)
				Sequencer::ban(uid, "Banned by host.");
		}
		else if( rc == ctxtmenu_pm )
		{
			wxString msg;
			msg = wxGetTextFromUser(_("Please enter your private message:"),
				_("Private message"),
				wxEmptyString,
				this,
				wxDefaultCoord,
				wxDefaultCoord,
				true	 
			);
			if( !msg.IsEmpty() )
				Sequencer::serverSay(std::string(msg.mb_str()), uid, FROM_HOST);
		}
	}
}


void MyDialog::OnBtnStart(wxCommandEvent& event)
{
	tb->EnableTool(btn_start, false);
	settingsPanel->Disable();
	wxBeginBusyCursor();
	
	//startBtn->Disable();
	//stopBtn->Enable();
	nbook->SetSelection(1);
	if(startServer())
	{
		settingsPanel->Enable();
		tb->EnableTool(btn_start, true);
	}
	else
	{
		// build the playerlist
		for(unsigned int i=0; i<Config::getMaxClients();i++)
		{
			slotlist->InsertItem(i, wxString::Format(wxT("%d"),i));
			slotlist->SetItem(i, 1, _("FREE"));
		}
		
		// update the gui at 1 fps
		timer1->Start(1000);
		tb->EnableTool(btn_stop, true);
	}
	updateLogTab();
	wxEndBusyCursor();
}

void MyDialog::OnBtnStop(wxCommandEvent& event)
{
	tb->EnableTool(btn_stop, false);
	wxBeginBusyCursor();
	
	// stop updating the gui
	timer1->Stop();
	
	// stop the server
	stopServer();
	
	// clear the playerlist
	slotlist->DeleteAllItems();

	// update the log view one last time
	updateLogTab();
	
	tb->EnableTool(btn_start, true);
	settingsPanel->Enable();
	wxEndBusyCursor();
}

void MyDialog::OnBtnExit(wxCommandEvent& event)
{
	Destroy();
	exit(0);
}

void MyDialog::OnBtnPubIP(wxCommandEvent& event)
{
	wxString ip = conv(Config::getPublicIP());
	ipaddr->SetValue(ip);
}

int MyDialog::startServer()
{
	// set default verbose levels
	loglevel = LOG_INFO;
	if(logmode->GetValue() == _("STACK"))   loglevel = LOG_STACK;
	if(logmode->GetValue() == _("DEBUG"))   loglevel = LOG_DEBUG;
	if(logmode->GetValue() == _("VERBOSE")) loglevel = LOG_VERBOSE;
	if(logmode->GetValue() == _("INFO"))    loglevel = LOG_INFO;
	if(logmode->GetValue() == _("WARN"))    loglevel = LOG_WARN;
	if(logmode->GetValue() == _("ERROR"))   loglevel = LOG_ERROR;

	Logger::setLogLevel(LOGTYPE_FILE, LOG_VERBOSE);
	Logger::setOutputFile("server.log");

	Config::setPrintStats(false);

	// lan or internet server?
	if(smode->GetValue() == _("LAN"))
		Config::setServerMode(SERVER_LAN);
	else
		Config::setServerMode(SERVER_INET);

	// set the IP address
	if(ipaddr->GetValue() != _("0.0.0.0"))
		Config::setIPAddr(conv(ipaddr->GetValue()));

	// set the log file
	Logger::setOutputFile(conv(logfilename->GetValue()));

	// set the port number
	unsigned long portNum=12000;
	port->GetValue().ToULong(&portNum);
	Config::setListenPort(portNum);
	
	// the maximum number of clients
	unsigned long slotNum=16;
	slots->GetValue().ToULong(&slotNum);
	if(slotNum > 64)
		slotNum = 64;
	else if(slotNum < 2)
		slotNum = 2;
	Config::setMaxClients(slotNum);

	// set the password
	if(!passwd->GetValue().IsEmpty())
		Config::setPublicPass(conv(passwd->GetValue()));

	//server name, replace space with underscore
	wxString server_name = sname->GetValue();
	const wxString from = wxString(" ");
	const wxString to = wxString("%20");
	server_name.Replace(from, to, true);

	if( !Config::setServerName(conv(server_name)) )
	{
		Logger::log(LOG_ERROR, "Invalid servername!");
		wxBell();
		return 1;
	}

	// set the terrain
	if( !Config::setTerrain(conv(terrain->GetValue())) )
	{
		Logger::log(LOG_ERROR, "Invalid terrain name!");
		wxBell();
		return 1;
	}

	// check the configuration
	if( !Config::checkConfig() )
	{
		wxBell();
		return 1;
	}

	// initialize the server
	Sequencer::initilize();

	// set the admin
	if(!adminuid->GetValue().IsEmpty())
	{
		std::string user_token = conv(adminuid->GetValue());
		SHA1FromString(user_token, user_token);
		if(Sequencer::getUserAuth())
			Sequencer::getUserAuth()->setUserAuth(AUTH_ADMIN, std::wstring(), user_token);
	}

	// start the notifier
	if(Config::getServerMode() == SERVER_INET)
 		pthread_create(&notifyThread, NULL, s_notifyThreadStart, this);
	
	server_is_running = true;
	
	// successfully started server!
	return 0;
}

int MyDialog::stopServer()
{
	server_is_running = false;
	if( Sequencer::getNotifier() )
	{
		pthread_cancel(notifyThread);
		pthread_detach(notifyThread);
	}
	Sequencer::cleanUp();
	return 0;
}