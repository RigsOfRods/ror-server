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

#include "statpict.h"

#include "rornet.h"
#include "sequencer.h"
#include "logger.h"
#include "config.h"

#include "logger.h"
#include "sha1_util.h"
#include "sha1.h"

// xpm images
#include "server.xpm"

enum {btn_start, btn_stop, btn_exit};

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
	wxButton *startBtn, *stopBtn, *exitBtn;
	wxTextCtrl *txtConsole, *ipaddr, *port, *slots, *passwd, *terrain, *sname;
	wxComboBox *smode, *logmode;
	wxNotebook *nbook;
	wxPanel *settingsPanel, *logPanel;
	void OnQuit(wxCloseEvent &event);
	int loglevel;

	void OnBtnStart(wxCommandEvent& event);
	void OnBtnStop(wxCommandEvent& event);
	void OnBtnExit(wxCommandEvent& event);
	
	int startServer();
	int stopServer();

	DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(MyDialog, wxDialog)
	EVT_CLOSE(MyDialog::OnQuit)
	EVT_BUTTON(btn_start, MyDialog::OnBtnStart)
	EVT_BUTTON(btn_stop,  MyDialog::OnBtnStop)
	EVT_BUTTON(btn_exit,  MyDialog::OnBtnExit)
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

// 'Main program' equivalent: the program execution "starts" here
bool MyApp::OnInit()
{
	if ( !wxApp::OnInit() )
		return false;

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

MyDialog::MyDialog(const wxString& title, MyApp *_app) : wxDialog(NULL, wxID_ANY, title,  wxPoint(100, 100), wxSize(500, 500))
{
	app=_app;
	loglevel=LOG_INFO;
	dialogInstance = this;

	SetMinSize(wxSize(500,500));
	//SetMaxSize(wxSize(500,500));
	SetWindowStyle(wxRESIZE_BORDER | wxCAPTION);

	wxSizer *mainsizer = new wxBoxSizer(wxVERTICAL);
	mainsizer->SetSizeHints(this);
	this->SetSizer(mainsizer);

	// using xpm now
	const wxBitmap bm(config_xpm);
	wxStaticPicture *imagePanel = new wxStaticPicture(this, -1, bm,wxPoint(0, 0), wxSize(500, 100), wxNO_BORDER);
	mainsizer->Add(imagePanel, 0, wxGROW);

	nbook=new wxNotebook(this, wxID_ANY);
	mainsizer->Add(nbook, 1, wxGROW);

	///// settings
	settingsPanel=new wxPanel(nbook, wxID_ANY);
	wxSizer *settingsSizer = new wxBoxSizer(wxVERTICAL);
	wxSizer *hsizer = new wxGridSizer(3, 2, 2, 2);
	wxStaticText *dText;

	// IP
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Server Name:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	hsizer->Add(dText, 0, wxGROW);
	sname = new wxTextCtrl(settingsPanel, wxID_ANY, _("Default Server"), wxDefaultPosition, wxDefaultSize);
	hsizer->Add(sname, 0, wxGROW);

	// mode
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Mode:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	hsizer->Add(dText, 0, wxGROW);

	wxString choices[1];
	choices[0] = _("LAN");
	choices[1] = _("Internet");
	smode = new wxComboBox(settingsPanel, wxID_ANY, _("LAN"), wxDefaultPosition, wxDefaultSize, 2, choices, wxCB_DROPDOWN|wxCB_READONLY);
	hsizer->Add(smode, 0, wxGROW);

	// IP
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("IP:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	hsizer->Add(dText, 0, wxGROW);
	ipaddr = new wxTextCtrl(settingsPanel, wxID_ANY, _("0.0.0.0"), wxDefaultPosition, wxDefaultSize);
	hsizer->Add(ipaddr, 0, wxGROW);
	
	// Port
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Port:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	hsizer->Add(dText, 0, wxGROW);
	port = new wxTextCtrl(settingsPanel, wxID_ANY, _("12000"), wxDefaultPosition, wxDefaultSize);
	hsizer->Add(port, 0, wxGROW);
	
	// Port
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Password:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	hsizer->Add(dText, 0, wxGROW);
	passwd = new wxTextCtrl(settingsPanel, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize);
	hsizer->Add(passwd, 0, wxGROW);

	// playercount
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Slots:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	hsizer->Add(dText, 0, wxGROW);
	slots = new wxTextCtrl(settingsPanel, wxID_ANY, _("16"), wxDefaultPosition, wxDefaultSize);
	hsizer->Add(slots, 0, wxGROW);

	// terrain
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("Terrain:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	hsizer->Add(dText, 0, wxGROW);
	terrain = new wxTextCtrl(settingsPanel, wxID_ANY, _("any"), wxDefaultPosition, wxDefaultSize);
	hsizer->Add(terrain, 0, wxGROW);

	// debuglevel
	dText = new wxStaticText(settingsPanel, wxID_ANY, _("LogLevel:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
	hsizer->Add(dText, 0, wxGROW);

	wxString choices_log[6];
	choices_log[0] = _("STACK");
	choices_log[1] = _("DEBUG");
	choices_log[2] = _("VERBOSE");
	choices_log[3] = _("INFO");
	choices_log[4] = _("WARN");
	choices_log[5] = _("ERRROR");
	logmode = new wxComboBox(settingsPanel, wxID_ANY, _("INFO"), wxDefaultPosition, wxDefaultSize, 6, choices_log, wxCB_DROPDOWN|wxCB_READONLY);
	hsizer->Add(logmode, 0, wxGROW);

	// some buttons
	startBtn = new wxButton(settingsPanel, btn_start, _("START"));
	hsizer->Add(startBtn, 0, wxGROW);

	stopBtn = new wxButton(settingsPanel, btn_stop, _("STOP"));
	stopBtn->Disable();
	hsizer->Add(stopBtn, 0, wxGROW);

	settingsSizer->Add(hsizer, 0, wxGROW);
	settingsPanel->SetSizer(settingsSizer);
	nbook->AddPage(settingsPanel, _("Settings"), true);

	///// Log
	logPanel=new wxPanel(nbook, wxID_ANY);
	wxSizer *logSizer = new wxBoxSizer(wxVERTICAL);
	txtConsole = new wxTextCtrl(logPanel, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, wxNO_BORDER|wxTE_MULTILINE|wxTE_READONLY|wxTE_RICH|wxTE_RICH2|wxTE_DONTWRAP);
	logSizer->Add(txtConsole, 1, wxGROW);
	logPanel->SetSizer(logSizer);
	nbook->AddPage(logPanel, _("Log"), false);

	exitBtn = new wxButton(this, btn_exit, _("EXIT"));
	mainsizer->Add(exitBtn, 0, wxGROW);
	


	// add the logger callback
	Logger::setCallback(&pureLogCallback);
	Logger::log(LOG_INFO, "GUI log callback working");

	// centers dialog window on the screen
	SetSize(500,500);
	Centre();
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

}

void MyDialog::OnBtnStop(wxCommandEvent& event)
{
	startBtn->Enable();
	stopBtn->Disable();
	stopServer();
}

void MyDialog::OnBtnExit(wxCommandEvent& event)
{
	Destroy();
	exit(0);
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

	if(smode->GetValue() == _("LAN"))
		Config::setServerMode(SERVER_LAN);
	else
		Config::setServerMode(SERVER_INET);

	if(ipaddr->GetValue() != _("0.0.0.0"))
		Config::setIPAddr(conv(ipaddr->GetValue()));
	
	unsigned long portNum=12000;
	port->GetValue().ToULong(&portNum);
	Config::setListenPort(portNum);

	unsigned long slotNum=16;
	slots->GetValue().ToULong(&slotNum);
	Config::setMaxClients(slotNum);

	if(!passwd->GetValue().IsEmpty())
		Config::setPublicPass(conv(passwd->GetValue()));

	Config::setServerName(conv(sname->GetValue()));
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