#include <vector>

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

#include "statpict.h"

#include "rornet.h"
#include "sequencer.h"
#include "logger.h"
#include "config.h"
#include "userauth.h"

#include "logger.h"
#include "sha1_util.h"
#include "sha1.h"

// xpm images
#include "server.xpm"

class MyApp : public wxApp
{
public:
	virtual bool OnInit();
};

// Define a new frame type: this is going to be our main frame
class MyDialog : public wxDialog
{
public:
	// ctor(s)
	MyDialog(const wxString& title, MyApp *_app);

	void logCallback(int level, std::string msg, std::string msgf); //called by callback function
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
	void OnQuit(wxCloseEvent &event);
	int loglevel;

	void OnBtnStart(wxCommandEvent& event);
	void OnBtnStop(wxCommandEvent& event);
	void OnBtnExit(wxCommandEvent& event);
	void OnBtnPubIP(wxCommandEvent& event);

	int startServer();
	int stopServer();
	void updatePlayerList();

	void OnTimer(wxTimerEvent& event);

	DECLARE_EVENT_TABLE()
};

enum {btn_start, btn_stop, btn_exit, EVT_timer1, btn_pubip};

BEGIN_EVENT_TABLE(MyDialog, wxDialog)
	EVT_CLOSE(MyDialog::OnQuit)
	EVT_BUTTON(btn_start, MyDialog::OnBtnStart)
	EVT_BUTTON(btn_stop,  MyDialog::OnBtnStop)
	EVT_BUTTON(btn_exit,  MyDialog::OnBtnExit)
	EVT_BUTTON(btn_pubip, MyDialog::OnBtnPubIP)
	EVT_TIMER(EVT_timer1, MyDialog::OnTimer)
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

void pureLogCallback(int level, std::string msg, std::string msgf)
{
	dialogInstance->logCallback(level, msg, msgf);
}

void MyDialog::logCallback(int level, std::string msg, std::string msgf)
{
	if(level < loglevel) return;

	if(level == LOG_INFO)
		txtConsole->SetDefaultStyle(wxTextAttr(wxColour(0,150,0)));
	else if(level == LOG_VERBOSE)
		txtConsole->SetDefaultStyle(wxTextAttr(wxColour(0,180,0)));
	else if(level == LOG_ERROR)
		txtConsole->SetDefaultStyle(wxTextAttr(wxColour(150,0,0)));
	else if(level == LOG_WARN)
		txtConsole->SetDefaultStyle(wxTextAttr(wxColour(180,130,0)));
	else if(level == LOG_DEBUG)
		txtConsole->SetDefaultStyle(wxTextAttr(wxColour(180,150,0)));
	else if(level == LOG_STACK)
		txtConsole->SetDefaultStyle(wxTextAttr(wxColour(150,150,150)));


    int lines = 0;
    const char* cstr = msgf.c_str();
    for( ; *cstr ; ++cstr )
        if( *cstr == '\n' )
            ++lines;

	wxString wmsg = conv(msgf);
	txtConsole->Freeze();
	txtConsole->AppendText(wmsg);
	txtConsole->ScrollLines(lines + 1);
	txtConsole->ShowPosition(txtConsole->GetLastPosition());
	txtConsole->Thaw();
}

MyDialog::MyDialog(const wxString& title, MyApp *_app) : wxDialog(NULL, wxID_ANY, title,  wxPoint(100, 100), wxSize(500, 500), wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER|wxRESIZE_BOX)
{
	app=_app;
	loglevel=LOG_INFO;
	dialogInstance = this;

	SetMinSize(wxSize(500,500));
	//SetMaxSize(wxSize(500,500));
	SetWindowStyle(wxRESIZE_BORDER | wxCAPTION);

	timer1 = new wxTimer(this, EVT_timer1);

	wxSizer *mainsizer = new wxBoxSizer(wxVERTICAL);
	mainsizer->SetSizeHints(this);
	this->SetSizer(mainsizer);

	// head image - using xpm
	wxStaticPicture *imagePanel = new wxStaticPicture(this, -1, wxBitmap(config_xpm), wxPoint(0, 0), wxSize(500, 100), wxNO_BORDER);
	mainsizer->Add(imagePanel, 0, wxGROW);

	nbook=new wxNotebook(this, wxID_ANY);
	mainsizer->Add(nbook, 1, wxGROW);

	///// settings
	settingsPanel=new wxScrolledWindow(nbook, wxID_ANY);
	settingsPanel->SetVirtualSize(300, 800);
	settingsPanel->SetScrollbars(0, 10, 1,0, 0, 0);

	wxSizer *settingsSizer = new wxBoxSizer(wxVERTICAL);
	wxFlexGridSizer *grid_sizer = new wxFlexGridSizer(3, 2, 10, 10);
	wxStaticText *dText;

	// server version
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Server Version:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	dText = new wxStaticText(settingsPanel, wxID_ANY, conv(RORNET_VERSION), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);

	// server name
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Server Name:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	sname = new wxTextCtrl(settingsPanel, wxID_ANY, _("Default Server"), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(sname, 0, wxGROW);

	// mode
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Mode:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);

	wxString choices[1];
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
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Admin UID:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	grid_sizer->Add(dText, 0, wxGROW);
	adminuid = new wxTextCtrl(settingsPanel, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize);
	grid_sizer->Add(adminuid, 0, wxGROW);

	// some buttons
	startBtn = new wxButton(settingsPanel, btn_start, _("START"));
	grid_sizer->Add(startBtn, 0, wxGROW);

	stopBtn = new wxButton(settingsPanel, btn_stop, _("STOP"));
	stopBtn->Disable();
	grid_sizer->Add(stopBtn, 0, wxGROW);

	settingsSizer->Add(grid_sizer, 0, wxGROW);
	settingsPanel->SetSizer(settingsSizer);
	nbook->AddPage(settingsPanel, _("Settings"), true);

	///// Log
	logPanel=new wxPanel(nbook, wxID_ANY);
	wxSizer *logSizer = new wxBoxSizer(wxVERTICAL);
	txtConsole = new wxTextCtrl(logPanel, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, wxNO_BORDER|wxTE_MULTILINE|wxTE_READONLY|wxTE_RICH|wxTE_RICH2|wxTE_DONTWRAP);
	logSizer->Add(txtConsole, 1, wxGROW);
	logPanel->SetSizer(logSizer);
	nbook->AddPage(logPanel, _("Log"), false);


	///// player list
	playersPanel=new wxPanel(nbook, wxID_ANY);
	wxSizer *playersSizer = new wxBoxSizer(wxVERTICAL);
	slotlist = new wxListCtrl(playersPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,wxLC_REPORT);
	slotlist->InsertColumn(0, _("Slot"), wxLIST_FORMAT_LEFT, 30);
	slotlist->InsertColumn(1, _("Status"), wxLIST_FORMAT_LEFT, 50);
	slotlist->InsertColumn(2, _("UID"), wxLIST_FORMAT_LEFT, 30);
	slotlist->InsertColumn(3, _("IP"), wxLIST_FORMAT_LEFT, 100);
	slotlist->InsertColumn(4, _("Auth"), wxLIST_FORMAT_LEFT, 40);
	slotlist->InsertColumn(5, _("Nick"), wxLIST_FORMAT_LEFT, 100);
	slotlist->InsertColumn(6, _("Vehicle"), wxLIST_FORMAT_LEFT, 120);

	for(unsigned int i=0; i<Config::getMaxClients();i++)
	{
		slotlist->InsertItem(i, wxString::Format(wxT("%d"),i));
		slotlist->SetItem(i, 1, _("FREE"));
	}

	playersSizer->Add(slotlist, 1, wxGROW);
	playersPanel->SetSizer(playersSizer);
	nbook->AddPage(playersPanel, _("Slots"), false);

	// main sizer again
	exitBtn = new wxButton(this, btn_exit, _("EXIT"));
	mainsizer->Add(exitBtn, 0, wxGROW);



	// add the logger callback
	Logger::setCallback(&pureLogCallback);
	Logger::log(LOG_INFO, "GUI log callback working");

	// centers dialog window on the screen
	Show();
	SetSize(500,500);
	Centre();
}

void MyDialog::OnTimer(wxTimerEvent& event)
{
	updatePlayerList();
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
			slotlist->SetItem(i, 1, _("FREE"));
			slotlist->SetItem(i, 2, wxString());
			slotlist->SetItem(i, 3, wxString());
			slotlist->SetItem(i, 4, wxString());
			slotlist->SetItem(i, 5, wxString());
			slotlist->SetItem(i, 6, wxString());
			continue;
		}

		if(clients[i].status == FREE)
			slotlist->SetItem(i, 1, _("FREE"));
		if(clients[i].status == BUSY)
			slotlist->SetItem(i, 1, _("BUSY"));
		if(clients[i].status == USED)
			slotlist->SetItem(i, 1, _("USED"));

		slotlist->SetItem(i, 2, wxString::Format(wxT("%d"),clients[i].uid));
		slotlist->SetItem(i, 3, conv(clients[i].ip_addr));

		char authst[5] = "";
		if(clients[i].authstate & AUTH_ADMIN) strcat(authst, "A");
		if(clients[i].authstate & AUTH_MOD) strcat(authst, "M");
		if(clients[i].authstate & AUTH_RANKED) strcat(authst, "R");
		if(clients[i].authstate & AUTH_BOT) strcat(authst, "B");
		if(clients[i].authstate & AUTH_BANNED) strcat(authst, "X");
		slotlist->SetItem(i, 4, conv(authst));

		slotlist->SetItem(i, 5, conv(clients[i].nickname));
		slotlist->SetItem(i, 6, conv(clients[i].vehicle_name));
	}
	slotlist->Thaw();
}

void MyDialog::OnQuit(wxCloseEvent &event)
{
	Destroy();
	exit(0);
}


void MyDialog::OnBtnStart(wxCommandEvent& event)
{
	startBtn->Disable();
	stopBtn->Enable();
	nbook->SetSelection(1);
	startServer();
	timer1->Start(5000);
}

void MyDialog::OnBtnStop(wxCommandEvent& event)
{
	timer1->Stop();
	stopServer();
	// FIXME: bug upon server restart ...
	//startBtn->Enable();
	stopBtn->Disable();
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
	Logger::setFlushLevel(LOG_ERROR);
	Logger::setOutputFile("server.log");

	Config::setPrintStats(false);

	if(smode->GetValue() == _("LAN"))
		Config::setServerMode(SERVER_LAN);
	else
		Config::setServerMode(SERVER_INET);

	if(ipaddr->GetValue() != _("0.0.0.0"))
		Config::setIPAddr(conv(ipaddr->GetValue()));

	Logger::setOutputFile(conv(logfilename->GetValue()));

	unsigned long portNum=12000;
	port->GetValue().ToULong(&portNum);
	Config::setListenPort(portNum);

	unsigned long slotNum=16;
	slots->GetValue().ToULong(&slotNum);
	Config::setMaxClients(slotNum);

	if(!passwd->GetValue().IsEmpty())
		Config::setPublicPass(conv(passwd->GetValue()));

	std::string user_token = conv(adminuid->GetValue());
	if(Sequencer::getUserAuth()) Sequencer::getUserAuth()->setUserAuth(user_token, AUTH_ADMIN);

	//server name, replace space with underscore
	wxString server_name = sname->GetValue();
	const wxChar from = wxChar(' ');
	const wxChar to = wxChar('_');
	server_name.Replace(&from, &to);
	Config::setServerName(conv(server_name));
	Config::setTerrain(conv(terrain->GetValue()));

	if( !Config::checkConfig() )
		return 1;

	Sequencer::initilize();

	if(Config::getServerMode() == SERVER_INET)
		Sequencer::notifyRoutine();
	return 0;
}

int MyDialog::stopServer()
{
	Sequencer::cleanUp();
	return 0;
}