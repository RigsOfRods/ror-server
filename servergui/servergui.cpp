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
private:
	MyApp *app;
	wxButton *startBtn, *stopBtn, *exitBtn;
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

MyDialog::MyDialog(const wxString& title, MyApp *_app) : wxDialog(NULL, wxID_ANY, title,  wxPoint(100, 100), wxSize(500, 200))
{
	app=_app;

	SetMinSize(wxSize(500,200));
	SetMaxSize(wxSize(500,200));

	wxSizer *mainsizer = new wxBoxSizer(wxVERTICAL);
	mainsizer->SetSizeHints(this);

	// using xpm now
	const wxBitmap bm(config_xpm);
	wxStaticPicture *imagePanel = new wxStaticPicture(this, -1, bm,wxPoint(0, 0), wxSize(500, 100), wxNO_BORDER);
	mainsizer->Add(imagePanel, 0, wxGROW);
	this->SetSizer(mainsizer);

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

	mainsizer->Add(hsizer, 1, wxGROW);



	// centers dialog window on the screen
	SetSize(500,200);
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