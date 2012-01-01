// created on 22th of June 2009 by Thomas Fischer
#ifdef WITH_ANGELSCRIPT

#include "logger.h"
#include "mutexutils.h"
#include "ScriptEngine.h"
#include "sequencer.h"
#include "config.h"
#include "scriptstdstring/scriptstdstring.h" // angelscript addon
#include "scriptmath/scriptmath.h" // angelscript addon
#include "scriptmath3d/scriptmath3d.h" // angelscript addon
#include "scriptarray/scriptarray.h" // angelscript addon
#include "scriptdictionary/scriptdictionary.h" // angelscript addon
#include "scriptany/scriptany.h" // angelscript addon
#include "scriptbuilder/scriptbuilder.h" // angelscript addon
#include "ScriptFileSafe.h" // (edited) angelscript addon

#include "utils.h"

#include <cstdio>


using namespace std;

// cross platform assert
#ifdef WIN32
#include "windows.h" // for Sleep()
extern "C" {
_CRTIMP void __cdecl _wassert(_In_z_ const wchar_t * _Message, _In_z_ const wchar_t *_File, _In_ unsigned _Line);
}
# define assert_net(_Expression) (void)( (!!(_Expression)) || (_wassert(_CRT_WIDE(#_Expression), _CRT_WIDE(__FILE__), __LINE__), 0) )
#else
#include <assert.h>
# define assert_net(expr) assert(expr)
#endif


#ifdef __GNUC__
#include <string.h>
#endif


void *s_sethreadstart(void* se)
{
    STACKLOG;
	((ScriptEngine*)se)->timerLoop();
	return NULL;
}

ScriptEngine::ScriptEngine(Sequencer *seq) : seq(seq), 
	engine(0),
	context(0),
	frameStepFunctionPtr(0),
	playerDeletedFunctionPtr(0),
	playerAddedFunctionPtr(0),
	playerChatFunctionPtr(0),
	gameCmdFunctionPtr(0),
	exit(false)
{
	init();
}

ScriptEngine::~ScriptEngine()
{
	// Clean up
	exit=true;
	if(engine) engine->Release();
	if(context) context->Release();
	if(frameStepFunctionPtr>=0)
	{
		pthread_join(timer_thread, NULL);
	}
}

int RoRServerScriptBuilderIncludeCallback(const char *include, const char *from, CScriptBuilder *builder, void *userParam)
{
	// Resource directory needs to be set in the config
	if(Config::getResourceDir().empty() || Config::getResourceDir()=="/")
		return -1;
		
	std::string myFilename = std::string(include);

	// Remove the possible .as extension
	if(myFilename.length()>3 && myFilename.substr(myFilename.length()-3, 3)==".as")
		myFilename = myFilename.substr(0, myFilename.length()-3);
	
	// Replace all forbidden characters in the filename	
	std::string allowedChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
	for (std::string::iterator it = myFilename.begin() ; it < myFilename.end() ; ++it){
		if( allowedChars.find(*it) == std::string::npos )
			*it = '_';
	}
	myFilename = Config::getResourceDir() + "scripts/includes/" + myFilename + ".as";

	// Include the script section
	int r = builder->AddSectionFromFile(myFilename.c_str());
	if( r < 0 )
		return r;
	
	return 1;

}

int ScriptEngine::loadScript(std::string scriptname)
{
	if(scriptname.empty()) return 0;
	
	int r;
	CScriptBuilder builder;
	builder.SetIncludeCallback(RoRServerScriptBuilderIncludeCallback, NULL);

	r = builder.StartNewModule(engine, "script");
	if( r < 0 )
	{
		Logger::log(LOG_ERROR,"ScriptEngine: Unkown error while starting a new script module.");
		return 1;
	}

	r = builder.AddSectionFromFile(scriptname.c_str());
	if( r < 0 )
	{
		Logger::log(LOG_ERROR,"ScriptEngine: Unkown error while adding a new section from file.");
		return 1;
	}

	r = builder.BuildModule();
	if( r < 0 )
	{
		if(r == asINVALID_CONFIGURATION)
			Logger::log(LOG_ERROR,"ScriptEngine: The engine configuration is invalid.");

		else if(r == asERROR)
			Logger::log(LOG_ERROR,"ScriptEngine: The script failed to build.");

		else if(r == asBUILD_IN_PROGRESS)
			Logger::log(LOG_ERROR,"ScriptEngine: Another thread is currently building.");

		else if(r == asINIT_GLOBAL_VARS_FAILED)
			Logger::log(LOG_ERROR,"ScriptEngine: It was not possible to initialize at least one of the global variables.");

		else
			Logger::log(LOG_ERROR,"ScriptEngine: Unkown error while building the script.");

		return 1;
	}

	// Get the newly created module
	asIScriptModule *mod = builder.GetModule();
	
	// Find the function that is to be called.
	int funcId = mod->GetFunctionIdByDecl("void main()");
	if( funcId < 0 )
	{
		// The function couldn't be found. Instruct the script writer to include the
		// expected function in the script.
		Logger::log(LOG_WARN,"ScriptEngine: The script must have the function 'void main()'. Please add it and try again.");
		return 1;
	}

	// get some other optional functions
	frameStepFunctionPtr = mod->GetFunctionIdByDecl("void frameStep(float)");
	if(frameStepFunctionPtr<0) Logger::log(LOG_WARN, "Script Function not used: frameStep");

	playerDeletedFunctionPtr = mod->GetFunctionIdByDecl("void playerDeleted(int, int)");
	if(playerDeletedFunctionPtr<0) Logger::log(LOG_WARN, "Script Function not used: playerDeleted");

	playerAddedFunctionPtr = mod->GetFunctionIdByDecl("void playerAdded(int)");
	if(playerAddedFunctionPtr<0) Logger::log(LOG_WARN, "Script Function not used: playerAdded");

	playerChatFunctionPtr = mod->GetFunctionIdByDecl("int playerChat(int, string msg)");
	if(playerChatFunctionPtr<0) Logger::log(LOG_WARN, "Script Function not used: playerChat");

	gameCmdFunctionPtr = mod->GetFunctionIdByDecl("void gameCmd(int, string)");
	if(gameCmdFunctionPtr<0) Logger::log(LOG_WARN, "Script Function not used: gameCmd");
	
	//eventCallbackFunctionPtr = mod->GetFunctionIdByDecl("void eventCallback(int event, int value)");

	// Create our context, prepare it, and then execute
	context = engine->CreateContext();

	//context->SetLineCallback(asMETHOD(ScriptEngine,LineCallback), this, asCALL_THISCALL);

	// this does not work :(
	//context->SetExceptionCallback(asMETHOD(ScriptEngine,ExceptionCallback), this, asCALL_THISCALL);

	context->Prepare(funcId);
	Logger::log(LOG_INFO,"ScriptEngine: Executing main()");
	r = context->Execute();
	if( r != asEXECUTION_FINISHED )
	{
		// The execution didn't complete as expected. Determine what happened.
		if( r == asEXECUTION_EXCEPTION )
		{
			// An exception occurred, let the script writer know what happened so it can be corrected.
			Logger::log(LOG_ERROR,"ScriptEngine: An exception '%s' occurred. Please correct the code in file '%s' and try again.", context->GetExceptionString(), scriptname.c_str());
		}
	}

	return 0;
}

void ScriptEngine::ExceptionCallback(asIScriptContext *ctx, void *param)
{
	asIScriptEngine *engine = ctx->GetEngine();
	int funcID = ctx->GetExceptionFunction();
	const asIScriptFunction *function = engine->GetFunctionById(funcID);
	Logger::log(LOG_INFO,"--- exception ---");
	Logger::log(LOG_INFO,"desc: %s", (ctx->GetExceptionString()));
	Logger::log(LOG_INFO,"func: %s", (function->GetDeclaration()));
	Logger::log(LOG_INFO,"modl: %s", (function->GetModuleName()));
	Logger::log(LOG_INFO,"sect: %s", (function->GetScriptSectionName()));
	int col, line = ctx->GetExceptionLineNumber(&col);
	Logger::log(LOG_INFO,"line %d, %d", line, col);

	// Print the variables in the current function
	PrintVariables(ctx, -1);

	// Show the call stack with the variables
	Logger::log(LOG_INFO,"--- call stack ---");
	for( unsigned int n = 0; n < ctx->GetCallstackSize(); n++ )
	{
		const asIScriptFunction *func = ctx->GetFunction(n);
		line = ctx->GetLineNumber(n,&col);
		Logger::log(LOG_INFO,"%s:%s : %d,%d", func->GetModuleName(), func->GetDeclaration(), line, col);

		PrintVariables(ctx, n);
	}
}

void ScriptEngine::LineCallback(asIScriptContext *ctx, void *param)
{
	char tmp[1024]="";
	asIScriptEngine *engine = ctx->GetEngine();
	int col;
	const char* sectionName;
	int line = ctx->GetLineNumber(0, &col, &sectionName);
	int indent = ctx->GetCallstackSize();
	for( int n = 0; n < indent; n++ )
		sprintf(tmp+n," ");
	const asIScriptFunction *function = engine->GetFunctionById(0);
	sprintf(tmp+indent,"%s:%s:%s:%d,%d", function->GetModuleName(), sectionName,
	                    function->GetDeclaration(),
	                    line, col);

	strcat(tmp, "");
	Logger::log(LOG_INFO,tmp);

//	PrintVariables(ctx, -1);
}

void ScriptEngine::PrintVariables(asIScriptContext *ctx, int stackLevel)
{
	asIScriptEngine *engine = ctx->GetEngine();

	int typeId = ctx->GetThisTypeId(stackLevel);
	void *varPointer = ctx->GetThisPointer(stackLevel);
	if( typeId )
	{
		Logger::log(LOG_INFO," this = 0x%x", varPointer);
	}

	int numVars = ctx->GetVarCount(stackLevel);
	for( int n = 0; n < numVars; n++ )
	{
		int typeId = ctx->GetVarTypeId(n, stackLevel);
		void *varPointer = ctx->GetAddressOfVar(n, stackLevel);
		if( typeId == engine->GetTypeIdByDecl("int") )
		{
			Logger::log(LOG_INFO," %s = %d", ctx->GetVarDeclaration(n, stackLevel), *(int*)varPointer);
		}
		else if( typeId == engine->GetTypeIdByDecl("string") )
		{
			std::string *str = (std::string*)varPointer;
			if( str )
			{
				Logger::log(LOG_INFO," %s = '%s'", ctx->GetVarDeclaration(n, stackLevel), str->c_str());
			} else
			{
				Logger::log(LOG_INFO," %s = <null>", ctx->GetVarDeclaration(n, stackLevel));
			}
		}
	}
};

// wrappers for functions that are not directly usable

// continue with initializing everything
void ScriptEngine::init()
{
	int result;
	// Create the script engine
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

	// Set the message callback to receive information on errors in human readable form.
	// It's recommended to do this right after the creation of the engine, because if
	// some registration fails the engine may send valuable information to the message
	// stream.
	result = engine->SetMessageCallback(asMETHOD(ScriptEngine,msgCallback), this, asCALL_THISCALL);
	if(result < 0)
	{
		if(result == asINVALID_ARG)
		{
			Logger::log(LOG_ERROR,"ScriptEngine: One of the arguments is incorrect, e.g. obj is null for a class method.");
			return;
		} else if(result == asNOT_SUPPORTED)
		{
			Logger::log(LOG_ERROR,"ScriptEngine: 	The arguments are not supported, e.g. asCALL_GENERIC.");
			return;
		}
		Logger::log(LOG_ERROR,"ScriptEngine: Unkown error while setting up message callback");
		return;
	}

	// AngelScript doesn't have a built-in string type, as there is no definite standard
	// string type for C++ applications. Every developer is free to register it's own string type.
	// The SDK do however provide a standard add-on for registering a string type, so it's not
	// necessary to register your own string type if you don't want to.
	RegisterStdString(engine);
	RegisterScriptArray(engine, true); // true = allow arrays to be registered as type[] (e.g. int[]). Needed for backwards compatibillity.
	RegisterStdStringUtils(engine); // depends on string and array
	RegisterScriptMath3D(engine); // depends on string
	RegisterScriptMath(engine);
	RegisterScriptDictionary(engine);
	RegisterScriptFile(engine);
	RegisterScriptAny(engine);

	Logger::log(LOG_INFO,"ScriptEngine: Registration of libs done, now custom things");

	// Register everything
	result = engine->RegisterObjectType("ServerScriptClass", sizeof(ServerScript), asOBJ_REF); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void log(const string &in)", asMETHOD(ServerScript,log), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void say(const string &in, int uid, int type)", asMETHOD(ServerScript,say), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void kick(int kuid, const string &in)", asMETHOD(ServerScript,kick), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void ban(int buid, const string &in)", asMETHOD(ServerScript,ban), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "bool unban(int buid)", asMETHOD(ServerScript,unban), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "int cmd(int uid, string cmd)", asMETHOD(ServerScript,sendGameCommand), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "int getNumClients()", asMETHOD(ServerScript,getNumClients), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserName(int uid)", asMETHOD(ServerScript,getUserName), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserAuth(int uid)", asMETHOD(ServerScript,getUserAuth), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "int getUserColourNum(int uid)", asMETHOD(ServerScript,getUserColourNum), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string getServerTerrain()", asMETHOD(ServerScript,getServerTerrain), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectBehaviour("ServerScriptClass", asBEHAVE_ADDREF, "void f()",asMETHOD(ServerScript,addRef), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectBehaviour("ServerScriptClass", asBEHAVE_RELEASE, "void f()",asMETHOD(ServerScript,releaseRef), asCALL_THISCALL); assert_net(result>=0);

	ServerScript *serverscript = new ServerScript(this, seq);
	result = engine->RegisterGlobalProperty("ServerScriptClass server", serverscript); assert_net(result>=0);

	Logger::log(LOG_INFO,"ScriptEngine: Registration done");

	if(frameStepFunctionPtr>=0)
	{
		Logger::log(LOG_DEBUG,"ScriptEngine: starting timer thread");
		pthread_create(&timer_thread, NULL, s_sethreadstart, this);
	}

}

void ScriptEngine::msgCallback(const asSMessageInfo *msg)
{
	const char *type = "Error";
	if( msg->type == asMSGTYPE_INFORMATION )
		type = "Info";
	else if( msg->type == asMSGTYPE_WARNING )
		type = "Warning";

	Logger::log(LOG_INFO,"ScriptEngine: %s (%d, %d): %s = %s", msg->section, msg->row, msg->col, type, msg->message);
}

int ScriptEngine::loadScriptFile(const char *fileName, string &script)
{
	FILE *f = fopen(fileName, "rb");
	if(!f) return 1;

	// Determine the size of the file
	fseek(f, 0, SEEK_END);
	int len = ftell(f);
	fseek(f, 0, SEEK_SET);

	// Load the entire file in one call
	script.resize(len);
	fread(&script[0], len, 1, f);

	fclose(f);
	return 0;
}

void ScriptEngine::executeString(std::string command)
{
#if 0
	// TOFIX: add proper error output
	if(!engine) return;
	if(!context) context = engine->CreateContext();

	int result = engine->ExecuteString("terrainScript", command.c_str(), &context);
	if(result<0)
	{
		Logger::log(LOG_ERROR,"error while executing string");
	}
#endif // 0
}

int ScriptEngine::framestep(float dt)
{
	if(frameStepFunctionPtr<0) return 1;
	if(!engine) return 0;
	if(!context) context = engine->CreateContext();
	context->Prepare(frameStepFunctionPtr);

	// Set the function arguments
	context->SetArgFloat(0, dt);

	//LogManager::getSingleton().logMessage("SE| Executing framestep()");
	int r = context->Execute();
	if( r == asEXECUTION_FINISHED )
	{
	  // The return value is only valid if the execution finished successfully
	  //asDWORD ret = context->GetReturnDWord();
	  context->GetReturnDWord();
	}
	return 0;
}

void ScriptEngine::playerDeleted(int uid, int crash)
{
	if(!engine || playerDeletedFunctionPtr<0) return;
	if(!context) context = engine->CreateContext();
	context->Prepare(playerDeletedFunctionPtr);

	context->SetArgDWord(0, uid);
	context->SetArgDWord(1, crash);
	context->Execute();
}

void ScriptEngine::playerAdded(int uid)
{
	if(!engine || playerAddedFunctionPtr<0) return;
	if(!context) context = engine->CreateContext();
	context->Prepare(playerAddedFunctionPtr);

	context->SetArgDWord(0, uid);
	context->Execute();
}

int ScriptEngine::playerChat(int uid, UTFString msg)
{
	if(!engine || playerChatFunctionPtr<0) return -1;
	if(!context) context = engine->CreateContext();
	context->Prepare(playerChatFunctionPtr);

	std::string msgstr = UTF8toString(msg);

	context->SetArgDWord(0, uid);
	context->SetArgObject(1, (void *)&msgstr);
	int r = context->Execute();
	if( r == asEXECUTION_FINISHED )
	{
	  // The return value is only valid if the execution finished successfully
	  asDWORD ret = context->GetReturnDWord();
	  return ret;
	}
	return -1;
}

void ScriptEngine::gameCmd(int uid, const std::string& cmd)
{
	if(!engine || gameCmdFunctionPtr<0) return;
	if(!context) context = engine->CreateContext();
	context->Prepare(gameCmdFunctionPtr);

	context->SetArgDWord(0, uid);
	context->SetArgObject(1, (void *)&cmd);
	context->Execute();
}

void ScriptEngine::timerLoop()
{
	while(!exit)
	{
		// sleep 200 miliseconds
#ifndef WIN32
		usleep(200000);
#else
		Sleep(200);
#endif
		// call script
		framestep(200);
	}
}


/* class that implements the interface for the scripts */
ServerScript::ServerScript(ScriptEngine *se, Sequencer *seq) : mse(se), seq(seq)
{
}

ServerScript::~ServerScript()
{
}

void ServerScript::log(std::string &msg)
{
	Logger::log(LOG_INFO,"SCRIPT|%s", msg.c_str());
}

void ServerScript::say(std::string &msg, int uid, int type)
{
	seq->serverSayThreadSave(msg, uid, type);
}

void ServerScript::kick(int kuid, std::string &msg)
{
	seq->disconnect(kuid, msg.c_str(), false);
}

void ServerScript::ban(int buid, std::string &msg)
{
	seq->ban(buid, msg.c_str());
}

bool ServerScript::unban(int buid)
{
	return seq->unban(buid);
}

std::string ServerScript::getUserName(int uid)
{
	client_t *c = seq->getClient(uid);
	if(!c) return "";

	
	return narrow(tryConvertUTF(c->user.username).asWStr());
}


std::string ServerScript::getUserAuth(int uid)
{
	client_t *c = seq->getClient(uid);
	if(!c) return "none";
	if(c->user.authstatus & AUTH_ADMIN) return "admin";
	else if(c->user.authstatus & AUTH_MOD) return "moderator";
	else if(c->user.authstatus & AUTH_RANKED) return "ranked";
	else if(c->user.authstatus & AUTH_BOT) return "bot";
	//else if(c->user.authstatus & AUTH_NONE) 
	return "none";
}

int ServerScript::getUserColourNum(int uid)
{
	client_t *c = seq->getClient(uid);
	if(!c) return 0;
	return c->user.colournum;
}

std::string ServerScript::getServerTerrain()
{
	return Config::getTerrainName();
}

int ServerScript::sendGameCommand(int uid, std::string cmd)
{
	return seq->sendGameCommand(uid, cmd);
}

int ServerScript::getNumClients()
{
	return seq->getNumClients();
}

#endif //WITH_ANGELSCRIPT
