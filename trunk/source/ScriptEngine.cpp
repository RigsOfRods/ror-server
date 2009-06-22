// created on 22th of June 2009 by Thomas Fischer
#include "logger.h"
#include "mutexutils.h"
#include "ScriptEngine.h"
#include "sequencer.h"
#include "scriptstdstring/scriptstdstring.h" // angelscript addon
#include "scriptmath/scriptmath.h" // angelscript addon

using namespace std;

// cross platform assert
#ifdef WIN32
extern "C" {
_CRTIMP void __cdecl _wassert(_In_z_ const wchar_t * _Message, _In_z_ const wchar_t *_File, _In_ unsigned _Line);
}
# define assert_net(_Expression) (void)( (!!(_Expression)) || (_wassert(_CRT_WIDE(#_Expression), _CRT_WIDE(__FILE__), __LINE__), 0) )
#else
# define assert_net(expr) assert(expr)
#endif

ScriptEngine::ScriptEngine(Sequencer *seq) : seq(seq), 
	engine(0),
	context(0),
	playerAddedFunctionPtr(0),
	playerDeletedFunctionPtr(0),
	playerChatFunctionPtr(0),
	frameStepFunctionPtr(0)
{
	init();
}

ScriptEngine::~ScriptEngine()
{
	// Clean up
	if(engine) engine->Release();
	if(context) context->Release();
}

int ScriptEngine::loadScript(std::string scriptname)
{
	if(scriptname.empty()) return 0;
	// Load the entire script file into the buffer
	int result=0;
	string script;
	result = loadScriptFile(scriptname.c_str(), script);
	if( result )
	{
		Logger::log(LOG_ERROR,"ScriptEngine: Unkown error while loading script file: %s\n", scriptname.c_str());
		return 1;
	}

	// Add the script to the module as a section. If desired, multiple script
	// sections can be added to the same module. They will then be compiled
	// together as if it was one large script.
	asIScriptModule *mod = engine->GetModule("script", asGM_ALWAYS_CREATE);
	result = mod->AddScriptSection(scriptname.c_str(), script.c_str(), script.length());
	if( result < 0 )
	{
		Logger::log(LOG_ERROR,"ScriptEngine: Unkown error while adding script section\n");
		return 1;
	}

	// Build the module
	result = mod->Build();
	if( result < 0 )
	{
		if(result == asINVALID_CONFIGURATION)
		{
			Logger::log(LOG_ERROR,"ScriptEngine: The engine configuration is invalid.\n");
			return 1;
		} else if(result == asERROR)
		{
			Logger::log(LOG_ERROR,"ScriptEngine: The script failed to build.\n");
			return 1;
		} else if(result == asBUILD_IN_PROGRESS)
		{
			Logger::log(LOG_ERROR,"ScriptEngine: Another thread is currently building.\n");
			return 1;
		}
		Logger::log(LOG_ERROR,"ScriptEngine: Unkown error while building the script.\n");
		return 1;
	}

	// Find the function that is to be called.
	int funcId = mod->GetFunctionIdByDecl("void main()");
	if( funcId < 0 )
	{
		// The function couldn't be found. Instruct the script writer to include the
		// expected function in the script.
		Logger::log(LOG_WARN,"ScriptEngine: The script must have the function 'void main()'. Please add it and try again.\n");
		return 1;
	}

	// get some other optional functions
	frameStepFunctionPtr = mod->GetFunctionIdByDecl("void frameStep(float)");
	playerDeletedFunctionPtr = mod->GetFunctionIdByDecl("void playerDeleted(int, int)");
	playerAddedFunctionPtr = mod->GetFunctionIdByDecl("void playerAdded(int)");
	playerChatFunctionPtr = mod->GetFunctionIdByDecl("void playerChat(int, string msg)");
	
	//eventCallbackFunctionPtr = mod->GetFunctionIdByDecl("void eventCallback(int event, int value)");

	// Create our context, prepare it, and then execute
	context = engine->CreateContext();

	//context->SetLineCallback(asMETHOD(ScriptEngine,LineCallback), this, asCALL_THISCALL);

	// this does not work :(
	//context->SetExceptionCallback(asMETHOD(ScriptEngine,ExceptionCallback), this, asCALL_THISCALL);

	context->Prepare(funcId);
	Logger::log(LOG_INFO,"ScriptEngine: Executing main()\n");
	result = context->Execute();
	if( result != asEXECUTION_FINISHED )
	{
		// The execution didn't complete as expected. Determine what happened.
		if( result == asEXECUTION_EXCEPTION )
		{
			// An exception occurred, let the script writer know what happened so it can be corrected.
			Logger::log(LOG_ERROR,"ScriptEngine: An exception '%s' occurred. Please correct the code in file '%s' and try again.\n", context->GetExceptionString(), scriptname.c_str());
		}
	}

	return 0;
}

void ScriptEngine::ExceptionCallback(asIScriptContext *ctx, void *param)
{
	asIScriptEngine *engine = ctx->GetEngine();
	int funcID = ctx->GetExceptionFunction();
	const asIScriptFunction *function = engine->GetFunctionDescriptorById(funcID);
	Logger::log(LOG_INFO,"--- exception ---\n");
	Logger::log(LOG_INFO,"desc: %s\n", (ctx->GetExceptionString()));
	Logger::log(LOG_INFO,"func: %s\n", (function->GetDeclaration()));
	Logger::log(LOG_INFO,"modl: %s\n", (function->GetModuleName()));
	Logger::log(LOG_INFO,"sect: %s\n", (function->GetScriptSectionName()));
	int col, line = ctx->GetExceptionLineNumber(&col);
	Logger::log(LOG_INFO,"line %d, %d\n", line, col);

	// Print the variables in the current function
	PrintVariables(ctx, -1);

	// Show the call stack with the variables
	Logger::log(LOG_INFO,"--- call stack ---\n");
	for( int n = 0; n < ctx->GetCallstackSize(); n++ )
	{
		funcID = ctx->GetCallstackFunction(n);
		const asIScriptFunction *func = engine->GetFunctionDescriptorById(funcID);
		line = ctx->GetCallstackLineNumber(n,&col);
		Logger::log(LOG_INFO,"%s:%s : %d,%d\n", func->GetModuleName(), func->GetDeclaration(), line, col);

		PrintVariables(ctx, n);
	}
}

void ScriptEngine::LineCallback(asIScriptContext *ctx, void *param)
{
	char tmp[1024]="";
	asIScriptEngine *engine = ctx->GetEngine();
	int funcID = ctx->GetCurrentFunction();
	int col;
	int line = ctx->GetCurrentLineNumber(&col);
	int indent = ctx->GetCallstackSize();
	for( int n = 0; n < indent; n++ )
		sprintf(tmp+n," ");
	const asIScriptFunction *function = engine->GetFunctionDescriptorById(funcID);
	sprintf(tmp+indent,"%s:%s:%d,%d", function->GetModuleName(),
	                    function->GetDeclaration(),
	                    line, col);

	strcat(tmp, "\n");
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
		Logger::log(LOG_INFO," this = 0x%x\n", varPointer);
	}

	int numVars = ctx->GetVarCount(stackLevel);
	for( int n = 0; n < numVars; n++ )
	{
		int typeId = ctx->GetVarTypeId(n, stackLevel);
		void *varPointer = ctx->GetAddressOfVar(n, stackLevel);
		if( typeId == engine->GetTypeIdByDecl("int") )
		{
			Logger::log(LOG_INFO," %s = %d\n", ctx->GetVarDeclaration(n, stackLevel), *(int*)varPointer);
		}
		else if( typeId == engine->GetTypeIdByDecl("string") )
		{
			std::string *str = (std::string*)varPointer;
			if( str )
			{
				Logger::log(LOG_INFO," %s = '%s'\n", ctx->GetVarDeclaration(n, stackLevel), str->c_str());
			} else
			{
				Logger::log(LOG_INFO," %s = <null>\n", ctx->GetVarDeclaration(n, stackLevel));
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
			Logger::log(LOG_ERROR,"ScriptEngine: One of the arguments is incorrect, e.g. obj is null for a class method.\n");
			return;
		} else if(result == asNOT_SUPPORTED)
		{
			Logger::log(LOG_ERROR,"ScriptEngine: 	The arguments are not supported, e.g. asCALL_GENERIC.\n");
			return;
		}
		Logger::log(LOG_ERROR,"ScriptEngine: Unkown error while setting up message callback\n");
		return;
	}

	// AngelScript doesn't have a built-in string type, as there is no definite standard
	// string type for C++ applications. Every developer is free to register it's own string type.
	// The SDK do however provide a standard add-on for registering a string type, so it's not
	// necessary to register your own string type if you don't want to.
	RegisterStdString(engine);
	RegisterScriptMath_Native(engine);

	// Register everything
	result = engine->RegisterObjectType("ServerScriptClass", sizeof(ServerScript), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void log(const string &in)", asMETHOD(ServerScript,log), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "void say(const string &in, int uid, int type)", asMETHOD(ServerScript,say), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "bool kick(int kuid, const string &in)", asMETHOD(ServerScript,kick), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "bool ban(int buid, const string &in)", asMETHOD(ServerScript,ban), asCALL_THISCALL); assert_net(result>=0);
	result = engine->RegisterObjectMethod("ServerScriptClass", "bool unban(int buid)", asMETHOD(ServerScript,unban), asCALL_THISCALL); assert_net(result>=0);

	ServerScript *serverscript = new ServerScript(this, seq);
	result = engine->RegisterGlobalProperty("ServerScriptClass server", serverscript); assert_net(result>=0);

	Logger::log(LOG_INFO,"ScriptEngine: Registration done\n");
}

void ScriptEngine::msgCallback(const asSMessageInfo *msg)
{
	const char *type = "Error";
	if( msg->type == asMSGTYPE_INFORMATION )
		type = "Info";
	else if( msg->type == asMSGTYPE_WARNING )
		type = "Warning";

	Logger::log(LOG_INFO,"ScriptEngine: %s (%d, %d): %s = %s\n", msg->section, msg->row, msg->col, type, msg->message);
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
	// TOFIX: add proper error output
	if(!engine) return;
	if(!context) context = engine->CreateContext();

	int result = engine->ExecuteString("terrainScript", command.c_str(), &context);
	if(result<0)
	{
		Logger::log(LOG_ERROR,"error while executing string\n");
	}
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
	  asDWORD ret = context->GetReturnDWord();
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

int ScriptEngine::playerChat(int uid, char *msg)
{
	if(!engine || playerChatFunctionPtr<0) return -1;
	if(!context) context = engine->CreateContext();
	context->Prepare(playerChatFunctionPtr);

	std::string msgstr = std::string(msg);
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


/* class that implements the interface for the scripts */
ServerScript::ServerScript(ScriptEngine *se, Sequencer *seq) : mse(se), seq(seq)
{
}

ServerScript::~ServerScript()
{
}

void ServerScript::log(std::string &msg)
{
	Logger::log(LOG_INFO,"SCRIPT|%s\n", msg.c_str());
}

void ServerScript::say(std::string &msg, int uid, int type)
{
	seq->serverSayThreadSave(msg, uid, type);
}

bool ServerScript::kick(int kuid, std::string &msg)
{
	return seq->kick(kuid, 0, msg.c_str());
}

bool ServerScript::ban(int buid, std::string &msg)
{
	return seq->ban(buid, 0, msg.c_str());
}

bool ServerScript::unban(int buid)
{
	return seq->unban(buid);
}
