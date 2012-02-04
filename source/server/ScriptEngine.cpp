// created on 22th of June 2009 by Thomas Fischer
#ifdef WITH_ANGELSCRIPT

#include "logger.h"
#include "mutexutils.h"
#include "ScriptEngine.h"
#include "sequencer.h"
#include "config.h"
#include "messaging.h"
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
	frameStepThreadRunning(false),
	exit(false)
{
	init();
}

ScriptEngine::~ScriptEngine()
{
	// Clean up
	exit=true;
	deleteAllCallbacks();
	if(engine) engine->Release();
	if(context) context->Release();
	if(frameStepThreadRunning)
	{
		pthread_join(timer_thread, NULL);
	}
}

void ScriptEngine::deleteAllCallbacks()
{
	if(!engine) return;
	
	for(std::map <std::string , callbackList >::iterator typeit = callbacks.begin(); typeit != callbacks.end(); ++typeit )
	{
		for (callbackList::iterator it = typeit->second.begin(); it != typeit->second.end(); ++it)
		{
			if(it->obj)
				it->obj->Release();
		}
		typeit->second.clear();
	}
	callbacks.clear();
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
		Logger::log(LOG_ERROR,"ScriptEngine: Unknown error while starting a new script module.");
		return 1;
	}

	r = builder.AddSectionFromFile(scriptname.c_str());
	if( r < 0 )
	{
		Logger::log(LOG_ERROR,"ScriptEngine: Unknown error while adding a new section from file.");
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
			Logger::log(LOG_ERROR,"ScriptEngine: Unknown error while building the script.");

		return 1;
	}

	// Get the newly created module
	asIScriptModule *mod = builder.GetModule();

	// get some other optional functions
	asIScriptFunction* func;

	func = mod->GetFunctionByDecl("void frameStep(float)");
	if(func) addCallback("frameStep", func, NULL);
	
	func = mod->GetFunctionByDecl("void playerDeleted(int, int)");
	if(func) addCallback("playerDeleted", func, NULL);
	
	func = mod->GetFunctionByDecl("void playerAdded(int)");
	if(func) addCallback("playerAdded", func, NULL);
	
	func = mod->GetFunctionByDecl("int playerChat(int, string msg)");
	if(func) addCallback("playerChat", func, NULL);
	
	func = mod->GetFunctionByDecl("void gameCmd(int, string)");
	if(func) addCallback("gameCmd", func, NULL);

	// Create and configure our context
	context = engine->CreateContext();
	//context->SetLineCallback(asMETHOD(ScriptEngine,LineCallback), this, asCALL_THISCALL);
	context->SetExceptionCallback(asMETHOD(ScriptEngine,ExceptionCallback), this, asCALL_THISCALL);
	
	// Find the function that is to be called.
	func = mod->GetFunctionByDecl("void main()");
	if(!func)
	{
		// The function couldn't be found. Instruct the script writer to include the
		// expected function in the script.
		Logger::log(LOG_WARN,"ScriptEngine: The script must have the function 'void main()'. Please add it and try again.");
		return 1;
	}

	// prepare and execute the main function
	context->Prepare(func);
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
	Logger::log(LOG_INFO,"--- script exception ---");
	Logger::log(LOG_INFO," desc: %s", (ctx->GetExceptionString()));
	Logger::log(LOG_INFO," func: %s", (function->GetDeclaration()));
	Logger::log(LOG_INFO," modl: %s", (function->GetModuleName()));
	Logger::log(LOG_INFO," sect: %s", (function->GetScriptSectionName()));
	int col, line = ctx->GetExceptionLineNumber(&col);
	Logger::log(LOG_INFO," line: %d, %d", line, col);

	// Show the call stack with the variables
	Logger::log(LOG_INFO,"--- call stack ---");
	for( unsigned int n = 0; n < ctx->GetCallstackSize(); n++ )
	{
		const asIScriptFunction *func = ctx->GetFunction(n);
		line = ctx->GetLineNumber(n,&col);
		Logger::log(LOG_INFO, "- %d -", n);
		Logger::log(LOG_INFO, "%s: %s : %d,%d", func->GetModuleName(), func->GetDeclaration(), line, col);
		PrintVariables(ctx, n);
	}

	Logger::log(LOG_INFO, "--- end of script exception message ---");
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
		Logger::log(LOG_ERROR,"ScriptEngine: Unknown error while setting up message callback");
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

	// Register ServerScript class
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
	result = engine->RegisterObjectMethod("ServerScriptClass", "int getUserAuthRaw(int uid)", asMETHOD(ServerScript,getUserAuthRaw), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "int getUserColourNum(int uid)", asMETHOD(ServerScript,getUserColourNum), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserToken(int uid)", asMETHOD(ServerScript,getUserToken), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserVersion(int uid)", asMETHOD(ServerScript,getUserVersion), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string getServerTerrain()", asMETHOD(ServerScript,getServerTerrain), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "int getTime()", asMETHOD(ServerScript,getTime), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "int getStartTime()", asMETHOD(ServerScript,getStartTime), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void setCallback(const string &in, const string &in, ?&in)", asMETHOD(ServerScript,setCallback), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void deleteCallback(const string &in, const string &in, ?&in)", asMETHOD(ServerScript,deleteCallback), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void throwException(const string &in)", asMETHOD(ServerScript,throwException), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_version()", asMETHOD(ServerScript,get_version), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_asVersion()", asMETHOD(ServerScript,get_asVersion), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_protocolVersion()", asMETHOD(ServerScript,get_protocolVersion), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "uint get_maxClients()", asMETHOD(ServerScript,get_maxClients), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_serverName()", asMETHOD(ServerScript,get_serverName), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_IPAddr()", asMETHOD(ServerScript,get_IPAddr), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "uint get_listenPort()", asMETHOD(ServerScript,get_listenPort), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "int get_serverMode()", asMETHOD(ServerScript,get_serverMode), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_owner()", asMETHOD(ServerScript,get_owner), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_website()", asMETHOD(ServerScript,get_website), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_ircServ()", asMETHOD(ServerScript,get_ircServ), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "string get_voipServ()", asMETHOD(ServerScript,get_voipServ), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectBehaviour("ServerScriptClass", asBEHAVE_ADDREF, "void f()",asMETHOD(ServerScript,addRef), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectBehaviour("ServerScriptClass", asBEHAVE_RELEASE, "void f()",asMETHOD(ServerScript,releaseRef), asCALL_THISCALL); assert_net(result>=0);
	ServerScript *serverscript = new ServerScript(this, seq);
	result = engine->RegisterGlobalProperty("ServerScriptClass server", serverscript); assert_net(result>=0);
	
	// Register ServerType enum for the server.serverMode attribute
	result = engine->RegisterEnum("ServerType"); assert_net(result>=0);
	result = engine->RegisterEnumValue("ServerType", "SERVER_LAN",  SERVER_LAN ); assert_net(result>=0);
	result = engine->RegisterEnumValue("ServerType", "SERVER_INET", SERVER_INET); assert_net(result>=0);
	result = engine->RegisterEnumValue("ServerType", "SERVER_AUTO", SERVER_AUTO); assert_net(result>=0);
	
	// Register publish mode enum for the playerChat callback
	result = engine->RegisterEnum("broadcastType"); assert_net(result>=0);
	result = engine->RegisterEnumValue("broadcastType", "BROADCAST_AUTO",   BROADCAST_AUTO); assert_net(result>=0);
	result = engine->RegisterEnumValue("broadcastType", "BROADCAST_BLOCK",  BROADCAST_BLOCK); assert_net(result>=0);
	result = engine->RegisterEnumValue("broadcastType", "BROADCAST_NORMAL", BROADCAST_NORMAL); assert_net(result>=0);
	result = engine->RegisterEnumValue("broadcastType", "BROADCAST_AUTHED", BROADCAST_AUTHED); assert_net(result>=0);
	result = engine->RegisterEnumValue("broadcastType", "BROADCAST_ALL",    BROADCAST_ALL); assert_net(result>=0);
	
	// Register authorizations
	result = engine->RegisterEnum("authType"); assert_net(result>=0);
	result = engine->RegisterEnumValue("authType", "AUTH_NONE",   AUTH_NONE); assert_net(result>=0);
	result = engine->RegisterEnumValue("authType", "AUTH_ADMIN",  AUTH_ADMIN); assert_net(result>=0);
	result = engine->RegisterEnumValue("authType", "AUTH_RANKED", AUTH_RANKED); assert_net(result>=0);
	result = engine->RegisterEnumValue("authType", "AUTH_MOD",    AUTH_MOD); assert_net(result>=0);
	result = engine->RegisterEnumValue("authType", "AUTH_BOT",    AUTH_BOT); assert_net(result>=0);
	result = engine->RegisterEnumValue("authType", "AUTH_BANNED", AUTH_BANNED); assert_net(result>=0);
	result = engine->RegisterEnumValue("authType", "AUTH_ALL",    0x11111111); assert_net(result>=0);
	
	// Register serverSayType
	result = engine->RegisterEnum("serverSayType"); assert_net(result>=0);
	result = engine->RegisterEnumValue("serverSayType", "FROM_SERVER", FROM_SERVER); assert_net(result>=0);
	result = engine->RegisterEnumValue("serverSayType", "FROM_HOST",   FROM_HOST); assert_net(result>=0);
	result = engine->RegisterEnumValue("serverSayType", "FROM_MOTD",   FROM_MOTD); assert_net(result>=0);
	result = engine->RegisterEnumValue("serverSayType", "FROM_RULES",  FROM_RULES); assert_net(result>=0);
	
	// register constants
	result = engine->RegisterGlobalProperty("const int TO_ALL", (void*)&TO_ALL); assert_net(result>=0);
	
	
	Logger::log(LOG_INFO,"ScriptEngine: Registration done");
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

// unused method
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

// unused method
int ScriptEngine::executeString(std::string command)
{
	// This method would work if you include the scriptHelper add-on
#if 0
	if(!engine) return 0;
	if(!context) context = engine->CreateContext();
	asIScriptModule* mod = engine->GetModule("script", asGM_CREATE_IF_NOT_EXISTS);
	int result = ExecuteString(engine, command.c_str(), mod, context);
	if(result<0)
	{
		Logger::log(LOG_ERROR, "ScriptEngine: Error while executing string: '" + command + "'.");
	}
	return result;
#endif // 0
	return 0;
}

int ScriptEngine::framestep(float dt)
{
	if(!engine) return 0;
	MutexLocker scoped_lock(context_mutex);
	if(!context) context = engine->CreateContext();
	int r;
	
	// Copy the callback list, because the callback list itself may get changed while executing the script
	callbackList queue(callbacks["frameStep"]);
	
	// loop over all callbacks
	for (unsigned int i=0; i<queue.size(); ++i)
	{
		// prepare the call
		r = context->Prepare(queue[i].func);
		if(r<0) continue;

		// Set the object if present (if we don't set it, then we call a global function)
		if(queue[i].obj!=NULL)
		{
			context->SetObject(queue[i].obj);
			if(r<0) continue;
		}

		// Set the arguments
		context->SetArgFloat(0, dt);
		
		// Execute it
		r = context->Execute();
	}
	return 0;
}

void ScriptEngine::playerDeleted(int uid, int crash)
{
	if(!engine) return;
	MutexLocker scoped_lock(context_mutex);
	if(!context) context = engine->CreateContext();
	int r;
	
	// Copy the callback list, because the callback list itself may get changed while executing the script
	callbackList queue(callbacks["playerDeleted"]);
	
	// loop over all callbacks
	for (unsigned int i=0; i<queue.size(); ++i)
	{
		// prepare the call
		r = context->Prepare(queue[i].func);
		if(r<0) continue;

		// Set the object if present (if we don't set it, then we call a global function)
		if(queue[i].obj!=NULL)
		{
			context->SetObject(queue[i].obj);
			if(r<0) continue;
		}

		// Set the arguments
		context->SetArgDWord(0, uid);
		context->SetArgDWord(1, crash);
		
		// Execute it
		r = context->Execute();
	}
	return;
}

void ScriptEngine::playerAdded(int uid)
{
	if(!engine) return;
	MutexLocker scoped_lock(context_mutex);
	if(!context) context = engine->CreateContext();
	int r;
	
	// Copy the callback list, because the callback list itself may get changed while executing the script
	callbackList queue(callbacks["playerAdded"]);
	
	// loop over all callbacks
	for (unsigned int i=0; i<queue.size(); ++i)
	{
		// prepare the call
		r = context->Prepare(queue[i].func);
		if(r<0) continue;

		// Set the object if present (if we don't set it, then we call a global function)
		if(queue[i].obj!=NULL)
		{
			context->SetObject(queue[i].obj);
			if(r<0) continue;
		}

		// Set the arguments
		context->SetArgDWord(0, uid);
		
		// Execute it
		r = context->Execute();
	}
	return;
}

int ScriptEngine::playerChat(int uid, UTFString msg)
{
	if(!engine) return 0;
	MutexLocker scoped_lock(context_mutex);
	if(!context) context = engine->CreateContext();
	int r;
	std::string msgstr = UTF8toString(msg);
	int ret = BROADCAST_AUTO;

	// Copy the callback list, because the callback list itself may get changed while executing the script
	callbackList queue(callbacks["playerChat"]);
	
	// loop over all callbacks
	for (unsigned int i=0; i<queue.size(); ++i)
	{
		// prepare the call
		r = context->Prepare(queue[i].func);
		if(r<0) continue;

		// Set the object if present (if we don't set it, then we call a global function)
		if(queue[i].obj!=NULL)
		{
			context->SetObject(queue[i].obj);
			if(r<0) continue;
		}

		// Set the arguments
		context->SetArgDWord(0, uid);
		context->SetArgObject(1, (void *)&msgstr);
		
		// Execute it
		r = context->Execute();
		if(r==asEXECUTION_FINISHED)
		{			
			int newRet = context->GetReturnDWord();

			// Only use the new result if it's more restrictive than what we already had
			if(newRet>ret)
				ret = newRet;
		}
	}

	return ret;
}

void ScriptEngine::gameCmd(int uid, const std::string& cmd)
{
	if(!engine) return;
	MutexLocker scoped_lock(context_mutex);
	if(!context) context = engine->CreateContext();
	int r;

	// Copy the callback list, because the callback list itself may get changed while executing the script
	callbackList queue(callbacks["gameCmd"]);
	
	// loop over all callbacks
	for (unsigned int i=0; i<queue.size(); ++i)
	{
		// prepare the call
		r = context->Prepare(queue[i].func);
		if(r<0) continue;

		// Set the object if present (if we don't set it, then we call a global function)
		if(queue[i].obj!=NULL)
		{
			context->SetObject(queue[i].obj);
			if(r<0) continue;
		}

		// Set the arguments
		context->SetArgDWord(0, uid);
		context->SetArgObject(1, (void *)&cmd);
		
		// Execute it
		r = context->Execute();
	}

	return;
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

void ScriptEngine::setException(const std::string& message)
{	
	if(!engine || !context)
	{
		// There's not much we can do, except for logging the message
		Logger::log(LOG_INFO, "--- script exception ---");
		Logger::log(LOG_INFO, " desc: %s", (message.c_str()));
		Logger::log(LOG_INFO, "--- end of script exception message ---");
	}
	else
		context->SetException(message.c_str());
}

void ScriptEngine::addCallbackScript(const std::string& type, const std::string& _func, asIScriptObject* obj)
{
	if(!engine) return;

	// get the function declaration and check the type at the same time
	std::string funcDecl = "";
	if(type=="frameStep")
		funcDecl = "void "+_func+"(float)";
	else if(type=="playerChat")
		funcDecl = "int "+_func+"(int, const string &in)";
	else if(type=="gameCmd")
		funcDecl = "void "+_func+"(int, const string &in)";
	else if(type=="playerAdded")
		funcDecl = "void "+_func+"(int)";
	else if(type=="playerDeleted")
		funcDecl = "void "+_func+"(int, int)";
	else
	{
		setException("Type "+type+" does not exist! Possible type strings: 'frameStep', 'playerChat', 'gameCmd', 'playerAdded', 'playerDeleted'.");
		return;
	}
	
	asIScriptFunction* func;
	if(obj)
	{
		// search for a method in the class
		asIObjectType* objType = obj->GetObjectType();
		func = objType->GetMethodByDecl(funcDecl.c_str());
		if(!func)
		{
			// give a nice error message that says that the method was not found.
			func = objType->GetMethodByName(_func.c_str());
			if(func)
				setException("Method '"+std::string(func->GetDeclaration(false))+"' was found in '"+objType->GetName()+"' but the correct declaration is: '"+funcDecl+"'.");
			else
				setException("Method '"+funcDecl+"' was not found in '"+objType->GetName()+"'.");
			return;
		}
	}
	else
	{
		// search for a global function
		asIScriptModule* mod  = engine->GetModule("script");
		func = mod->GetFunctionByDecl(funcDecl.c_str());
		if(!func)
		{
			// give a nice error message that says that the function was not found.
			func = mod->GetFunctionByName(_func.c_str());
			if(func)
				setException("Function '"+std::string(func->GetDeclaration(false))+"' was found, but the correct declaration is: '"+funcDecl+"'.");
			else
				setException("Function '"+funcDecl+"' was not found.");
			return;
		}
	}
	
	if(callbackExists(type, func, obj))
		Logger::log(LOG_INFO, "ScriptEngine: error: Function '"+std::string(func->GetDeclaration(false))+"' is already a callback for '"+type+"'.");
	else
		addCallback(type, func, obj);
}

void ScriptEngine::addCallback(const std::string& type, asIScriptFunction* func, asIScriptObject* obj)
{
	if(!engine) return;
	
	if(obj)
	{
		// We're about to store a reference to the object, so let's tell the script engine about that
		// This avoids the object from going out of scope while we're still trying to access it.
		// BUT: it prevents local objects from being destroyed automatically....
		engine->AddRefScriptObject(obj, obj->GetTypeId());
	}

	// Add the function to the list
	callback_t tmp;
	tmp.obj  = obj;
	tmp.func = func;
	callbacks[type].push_back(tmp);

	// Do we need to start the frameStep thread?
	if(type=="frameStep" && !frameStepThreadRunning)
	{
		frameStepThreadRunning = true;
		Logger::log(LOG_DEBUG,"ScriptEngine: starting timer thread");
		pthread_create(&timer_thread, NULL, s_sethreadstart, this);
	}

	// finished :)
	Logger::log(LOG_INFO, "ScriptEngine: success: Added a '"+type+"' callback for: "+std::string(func->GetDeclaration(true)));
}

void ScriptEngine::deleteCallbackScript(const std::string& type, const std::string& _func, asIScriptObject* obj)
{
	if(!engine) return;

	// get the function declaration and check the type at the same time
	std::string funcDecl = "";
	if(type=="frameStep")
		funcDecl = "void "+_func+"(float)";
	else if(type=="playerChat")
		funcDecl = "int "+_func+"(int, const string &in)";
	else if(type=="gameCmd")
		funcDecl = "void "+_func+"(int, const string &in)";
	else if(type=="playerAdded")
		funcDecl = "void "+_func+"(int)";
	else if(type=="playerDeleted")
		funcDecl = "void "+_func+"(int, int)";
	else
	{
		setException("Type "+type+" does not exist! Possible type strings: 'frameStep', 'playerChat', 'gameCmd', 'playerAdded', 'playerDeleted'.");
		Logger::log(LOG_INFO, "ScriptEngine: error: Failed to remove callback: "+_func);
		return;
	}

	asIScriptFunction* func;
	if(obj)
	{
		// search for a method in the class
		asIObjectType* objType = obj->GetObjectType();
		func = objType->GetMethodByDecl(funcDecl.c_str());
		if(!func)
		{
			// give a nice error message that says that the method was not found.
			func = objType->GetMethodByName(_func.c_str());
			if(func)
				setException("Method '"+std::string(func->GetDeclaration(false))+"' was found in '"+objType->GetName()+"' but the correct declaration is: '"+funcDecl+"'.");
			else
				setException("Method '"+funcDecl+"' was not found in '"+objType->GetName()+"'.");
			Logger::log(LOG_INFO, "ScriptEngine: error: Failed to remove callback: "+funcDecl);
			return;
		}
	}
	else
	{
		// search for a global function
		asIScriptModule* mod  = engine->GetModule("script");
		func = mod->GetFunctionByDecl(funcDecl.c_str());
		if(!func)
		{
			// give a nice error message that says that the function was not found.
			func = mod->GetFunctionByName(_func.c_str());
			if(func)
				setException("Function '"+std::string(func->GetDeclaration(false))+"' was found, but the correct declaration is: '"+funcDecl+"'.");
			else
				setException("Function '"+funcDecl+"' was not found.");
			Logger::log(LOG_INFO, "ScriptEngine: error: Failed to remove callback: "+funcDecl);
			return;
		}
	}
	deleteCallback(type, func, obj);
}

void ScriptEngine::deleteCallback(const std::string& type, asIScriptFunction* func, asIScriptObject* obj)
{
	if(!engine) return;

	for (callbackList::iterator it = callbacks[type].begin(); it != callbacks[type].end(); ++it)
	{
		if(it->obj==obj && it->func==func)
		{
			callbacks[type].erase(it);
			Logger::log(LOG_INFO, "ScriptEngine: success: removed a '"+type+"' callback: "+std::string(func->GetDeclaration(true)));
			if(obj)
				engine->ReleaseScriptObject(obj, obj->GetTypeId());
			return;
		}
	}
	Logger::log(LOG_INFO, "ScriptEngine: error: failed to remove callback: "+std::string(func->GetDeclaration(true)));
}

bool ScriptEngine::callbackExists(const std::string& type, asIScriptFunction* func, asIScriptObject* obj)
{
	if(!engine) return false;

	for (callbackList::iterator it = callbacks[type].begin(); it != callbacks[type].end(); ++it)
	{
		if(it->obj==obj && it->func==func)
			return true;
	}
	return false;
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
	seq->disconnect(kuid, msg.c_str(), false, false);
}

void ServerScript::ban(int buid, std::string &msg)
{
	seq->silentBan(buid, msg.c_str());
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

int ServerScript::getUserAuthRaw(int uid)
{
	client_t *c = seq->getClient(uid);
	if(!c) return AUTH_NONE;
	return c->user.authstatus;
}

int ServerScript::getUserColourNum(int uid)
{
	client_t *c = seq->getClient(uid);
	if(!c) return 0;
	return c->user.colournum;
}

std::string ServerScript::getUserToken(int uid)
{
	client_t *c = seq->getClient(uid);
	if(!c) return 0;
	return std::string(c->user.usertoken, 40);
}

std::string ServerScript::getUserVersion(int uid)
{
	client_t *c = seq->getClient(uid);
	if(!c) return 0;
	return std::string(c->user.clientversion, 25);
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

int ServerScript::getStartTime()
{
	return seq->getStartTime();
}

int ServerScript::getTime()
{
	return Messaging::getTime();
}

void ServerScript::deleteCallback(const std::string& type, const std::string& func, void* obj, int refTypeId)
{
	if(refTypeId & asTYPEID_SCRIPTOBJECT && (refTypeId & asTYPEID_OBJHANDLE))
	{
		mse->deleteCallbackScript(type, func, *(asIScriptObject**)obj);
	}
	else if(refTypeId==asTYPEID_VOID)
	{
		mse->deleteCallbackScript(type, func, NULL);
	}
	else if(refTypeId & asTYPEID_SCRIPTOBJECT)
	{
		// We received an object instead of a handle of the object.
		// We cannot allow this because this will crash if the deleteCallback is called from inside a constructor of a global variable.
		mse->setException("server.deleteCallback should be called with a handle of the object! (that is: put an @ sign in front of the object)");
		
		// uncomment to enable anyway:
		//mse->deleteCallbackScript(type, func, (asIScriptObject*)obj);
	}
	else
	{
		mse->setException("The object for the callback has to be a script-class or null!");
	}
}

void ServerScript::setCallback(const std::string& type, const std::string& func, void* obj, int refTypeId)
{
	if(refTypeId & asTYPEID_SCRIPTOBJECT && (refTypeId & asTYPEID_OBJHANDLE))
	{
		mse->addCallbackScript(type, func, *(asIScriptObject**)obj);
	}
	else if(refTypeId==asTYPEID_VOID)
	{
		mse->addCallbackScript(type, func, NULL);
	}
	else if(refTypeId & asTYPEID_SCRIPTOBJECT)
	{
		// We received an object instead of a handle of the object.
		// We cannot allow this because this will crash if the setCallback is called from inside a constructor of a global variable.
		mse->setException("server.setCallback should be called with a handle of the object! (that is: put an @ sign in front of the object)");
		
		// uncomment to enable anyway:
		//mse->addCallbackScript(type, func, (asIScriptObject*)obj);
	}
	else
	{
		mse->setException("The object for the callback has to be a script-class or null!");
	}
}

void ServerScript::throwException(const std::string& message)
{
	mse->setException(message);
}

std::string ServerScript::get_version()
{
	return std::string(VERSION);
}

std::string ServerScript::get_asVersion()
{
	return std::string(ANGELSCRIPT_VERSION_STRING);
}

std::string ServerScript::get_protocolVersion()
{
	return std::string(RORNET_VERSION);
}

unsigned int ServerScript::get_maxClients() { return Config::getMaxClients(); }
std::string  ServerScript::get_serverName() { return Config::getServerName(); }
std::string  ServerScript::get_IPAddr()     { return Config::getIPAddr();     }
unsigned int ServerScript::get_listenPort() { return Config::getListenPort(); }
int          ServerScript::get_serverMode() { return Config::getServerMode(); }
std::string  ServerScript::get_owner()      { return Config::getOwner();      }
std::string  ServerScript::get_website()    { return Config::getWebsite();    }
std::string  ServerScript::get_ircServ()    { return Config::getIRC();        }
std::string  ServerScript::get_voipServ()   { return Config::getVoIP();       }


#endif //WITH_ANGELSCRIPT
