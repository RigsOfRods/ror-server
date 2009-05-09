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
	wxTextCtrl *txtConsole;
	wxRadioBox *smode;
	void OnQuit(wxCloseEvent &event);

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
	if(level == LOG_STACK) return;

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

	wxString wmsg = conv(msgf);
	txtConsole->AppendText(wmsg);
	txtConsole->SetInsertionPointEnd();
}

MyDialog::MyDialog(const wxString& title, MyApp *_app) : wxDialog(NULL, wxID_ANY, title,  wxPoint(100, 100), wxSize(500, 500))
{
	app=_app;
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

	txtConsole = new wxTextCtrl(this, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, wxNO_BORDER|wxTE_MULTILINE|wxTE_READONLY|wxTE_RICH|wxTE_RICH2|wxTE_DONTWRAP);
	mainsizer->Add(txtConsole, 1, wxGROW);

	wxArrayString choices;
	choices.Add(_("LAN"));
	choices.Add(_("Internet"));

	wxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

	smode = new wxRadioBox(this, wxID_ANY, _("Mode"), wxDefaultPosition, wxDefaultSize, choices, 2, wxRA_SPECIFY_ROWS);
	hsizer->Add(smode, 0, wxGROW);

	startBtn = new wxButton(this, btn_start, _("START"));
	hsizer->Add(startBtn, 0, wxGROW);

	stopBtn = new wxButton(this, btn_stop, _("STOP"));
	stopBtn->Disable();
	hsizer->Add(stopBtn, 0, wxGROW);

	exitBtn = new wxButton(this, btn_exit, _("EXIT"));
	hsizer->Add(exitBtn, 0, wxGROW);

	mainsizer->Add(hsizer, 0, wxGROW);

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
	Sequencer::initilize();

}

void MyDialog::OnBtnStop(wxCommandEvent& event)
{
	startBtn->Enable();
	stopBtn->Disable();
}

void MyDialog::OnBtnExit(wxCommandEvent& event)
{
	Destroy();
	exit(0);
}


int MyDialog::startServer()
{
	// det default verbose levels
	Logger::setLogLevel(LOGTYPE_DISPLAY, LOG_INFO);
	Logger::setLogLevel(LOGTYPE_FILE, LOG_VERBOSE);
	Logger::setFlushLevel(LOG_ERROR);
	Logger::setOutputFile("server.log");

	if(smode->GetSelection() == 0)
		Config::setServerMode(SERVER_LAN);
	else if(smode->GetSelection() == 1)
		Config::setServerMode(SERVER_INET);
		
	if( !Config::checkConfig() )
		return 1;
	
	Sequencer::initilize();

	if(Config::getServerMode() == SERVER_INET)
		Sequencer::notifyRoutine(); 

	Sequencer::cleanUp();
	return 0;
}

int MyDialog::stopServer()
{
	Sequencer::cleanUp();
	return 0;
}