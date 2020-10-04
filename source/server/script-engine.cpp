/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2009   Thomas Fischer
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

// created on 22th of June 2009 by Thomas Fischer
#ifdef WITH_ANGELSCRIPT

// Source headers
#include "config.h"
#include "logger.h"
#include "messaging.h"
#include "mutexutils.h"
#include "script-engine.h"
#include "server-script.h"
#include "sequencer.h"
#include "utils.h"

// Lib headers
#include "scriptstdstring/scriptstdstring.h" // angelscript addon
#include "scriptmath/scriptmath.h" // angelscript addon
#include "scriptmath3d/scriptmath3d.h" // angelscript addon
#include "scriptarray/scriptarray.h" // angelscript addon
#include "scriptdictionary/scriptdictionary.h" // angelscript addon
#include "scriptany/scriptany.h" // angelscript addon
#include "scriptbuilder/scriptbuilder.h" // angelscript addon
#include "ScriptFileSafe.h" // (edited) angelscript addon
#include "SocketW.h"

// Std headers
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace std;

void *s_sethreadstart(void *se) {
    ((ScriptEngine *) se)->timerLoop();
    return NULL;
}

ScriptEngine::ScriptEngine(Sequencer *seq) : seq(seq),
                                             engine(0),
                                             context(0),
                                             frameStepThreadRunning(false),
                                             exit(false) {
    init();
}

ScriptEngine::~ScriptEngine() {
    // Clean up
    exit = true;
    deleteAllCallbacks();
    if (engine) engine->Release();
    if (context) context->Release();
    if (frameStepThreadRunning) {
        pthread_join(timer_thread, NULL);
    }
}

void ScriptEngine::deleteAllCallbacks() {
    if (!engine) return;

    for (std::map<std::string, callbackList>::iterator typeit = callbacks.begin();
         typeit != callbacks.end(); ++typeit) {
        for (callbackList::iterator it = typeit->second.begin(); it != typeit->second.end(); ++it) {
            if (it->obj)
                it->obj->Release();
        }
        typeit->second.clear();
    }
    callbacks.clear();
}

int
RoRServerScriptBuilderIncludeCallback(const char *include, const char *from, CScriptBuilder *builder, void *userParam) {
    // Resource directory needs to be set in the config
    if (Config::getResourceDir().empty() || Config::getResourceDir() == "/")
        return -1;

    std::string myFilename = std::string(include);

    // Remove the possible .as extension
    if (myFilename.length() > 3 && myFilename.substr(myFilename.length() - 3, 3) == ".as")
        myFilename = myFilename.substr(0, myFilename.length() - 3);

    // Replace all forbidden characters in the filename
    std::string allowedChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    for (std::string::iterator it = myFilename.begin(); it < myFilename.end(); ++it) {
        if (allowedChars.find(*it) == std::string::npos)
            *it = '_';
    }
    myFilename = Config::getResourceDir() + "scripts/includes/" + myFilename + ".as";

    // Include the script section
    int r = builder->AddSectionFromFile(myFilename.c_str());
    if (r < 0)
        return r;

    return 1;

}

int ScriptEngine::loadScript(std::string scriptname) {
    if (scriptname.empty()) return 0;

    int r;
    CScriptBuilder builder;
    builder.SetIncludeCallback(RoRServerScriptBuilderIncludeCallback, NULL);

    r = builder.StartNewModule(engine, "script");
    if (r < 0) {
        Logger::Log(LOG_ERROR, "ScriptEngine: Unknown error while starting a new script module.");
        return 1;
    }

    r = builder.AddSectionFromFile(scriptname.c_str());
    if (r < 0) {
        Logger::Log(LOG_ERROR, "ScriptEngine: Unknown error while adding a new section from file.");
        return 1;
    }

    r = builder.BuildModule();
    if (r < 0) {
        if (r == asINVALID_CONFIGURATION)
            Logger::Log(LOG_ERROR, "ScriptEngine: The engine configuration is invalid.");

        else if (r == asERROR)
            Logger::Log(LOG_ERROR, "ScriptEngine: The script failed to build.");

        else if (r == asBUILD_IN_PROGRESS)
            Logger::Log(LOG_ERROR, "ScriptEngine: Another thread is currently building.");

        else if (r == asINIT_GLOBAL_VARS_FAILED)
            Logger::Log(LOG_ERROR,
                        "ScriptEngine: It was not possible to initialize at least one of the global variables.");

        else
            Logger::Log(LOG_ERROR, "ScriptEngine: Unknown error while building the script.");

        return 1;
    }

    // Get the newly created module
    asIScriptModule *mod = builder.GetModule();

    // get some other optional functions
    asIScriptFunction *func;

    func = mod->GetFunctionByDecl("void frameStep(float)");
    if (func) addCallback("frameStep", func, NULL);

    func = mod->GetFunctionByDecl("void playerDeleted(int, int)");
    if (func) addCallback("playerDeleted", func, NULL);

    func = mod->GetFunctionByDecl("void playerAdded(int)");
    if (func) addCallback("playerAdded", func, NULL);

    func = mod->GetFunctionByDecl("int streamAdded(int, StreamRegister@)");
    if (func) addCallback("streamAdded", func, NULL);

    func = mod->GetFunctionByDecl("int playerChat(int, string msg)");
    if (func) addCallback("playerChat", func, NULL);

    func = mod->GetFunctionByDecl("void gameCmd(int, string)");
    if (func) addCallback("gameCmd", func, NULL);

    // Create and configure our context
    context = engine->CreateContext();
    //context->SetLineCallback(asMETHOD(ScriptEngine,LineCallback), this, asCALL_THISCALL);
    context->SetExceptionCallback(asMETHOD(ScriptEngine, ExceptionCallback), this, asCALL_THISCALL);

    // Find the function that is to be called.
    func = mod->GetFunctionByDecl("void main()");
    if (!func) {
        // The function couldn't be found. Instruct the script writer to include the
        // expected function in the script.
        Logger::Log(LOG_WARN,
                    "ScriptEngine: The script must have the function 'void main()'. Please add it and try again.");
        return 1;
    }

    // prepare and execute the main function
    context->Prepare(func);
    Logger::Log(LOG_INFO, "ScriptEngine: Executing main()");
    r = context->Execute();
    if (r != asEXECUTION_FINISHED) {
        // The execution didn't complete as expected. Determine what happened.
        if (r == asEXECUTION_EXCEPTION) {
            // An exception occurred, let the script writer know what happened so it can be corrected.
            Logger::Log(LOG_ERROR,
                        "ScriptEngine: An exception '%s' occurred. Please correct the code in file '%s' and try again.",
                        context->GetExceptionString(), scriptname.c_str());
        }
    }

    return 0;
}

void ScriptEngine::ExceptionCallback(asIScriptContext *ctx, void *param) {
    const asIScriptFunction *function = ctx->GetExceptionFunction();
    Logger::Log(LOG_INFO, "--- exception ---");
    Logger::Log(LOG_INFO, "desc: %s", ctx->GetExceptionString());
    Logger::Log(LOG_INFO, "func: %s", function->GetDeclaration());
    Logger::Log(LOG_INFO, "modl: %s", function->GetModuleName());
    Logger::Log(LOG_INFO, "sect: %s", function->GetScriptSectionName());
    int col, line = ctx->GetExceptionLineNumber(&col);
    Logger::Log(LOG_INFO, "line: %d,%d", line, col);

    // Show the call stack with the variables
    Logger::Log(LOG_INFO, "--- call stack ---");
    char tmp[2048] = "";
    for (asUINT n = 0; n < ctx->GetCallstackSize(); n++) {
        function = ctx->GetFunction(n);
        sprintf(tmp, "%s (%d): %s", function->GetScriptSectionName(), ctx->GetLineNumber(n),
                function->GetDeclaration());
        Logger::Log(LOG_INFO, tmp);
        PrintVariables(ctx, n);
    }
    Logger::Log(LOG_INFO, "--- end of script exception message ---");
}

void ScriptEngine::LineCallback(asIScriptContext *ctx, void *param) {
    char tmp[1024] = "";
    asIScriptEngine *engine = ctx->GetEngine();
    int col;
    const char *sectionName;
    int line = ctx->GetLineNumber(0, &col, &sectionName);
    int indent = ctx->GetCallstackSize();
    for (int n = 0; n < indent; n++)
        sprintf(tmp + n, " ");
    const asIScriptFunction *function = engine->GetFunctionById(0);
    sprintf(tmp + indent, "%s:%s:%s:%d,%d", function->GetModuleName(), sectionName,
            function->GetDeclaration(),
            line, col);

    strcat(tmp, "");
    Logger::Log(LOG_INFO, tmp);

//	PrintVariables(ctx, -1);
}

void ScriptEngine::PrintVariables(asIScriptContext *ctx, int stackLevel) {
    asIScriptEngine *engine = ctx->GetEngine();

    // First print the this pointer if this is a class method
    int typeId = ctx->GetThisTypeId(stackLevel);
    void *varPointer = ctx->GetThisPointer(stackLevel);
    if (typeId) {
        Logger::Log(LOG_INFO, " this = 0x%x", varPointer);
    }

    // Print the value of each variable, including parameters
    int numVars = ctx->GetVarCount(stackLevel);
    for (int n = 0; n < numVars; n++) {
        int typeId = ctx->GetVarTypeId(n, stackLevel);
        void *varPointer = ctx->GetAddressOfVar(n, stackLevel);
        if (typeId == asTYPEID_INT32) {
            Logger::Log(LOG_INFO, " %s = %d", ctx->GetVarDeclaration(n, stackLevel), *(int *) varPointer);
        } else if (typeId == asTYPEID_FLOAT) {
            Logger::Log(LOG_INFO, " %s = %f", ctx->GetVarDeclaration(n, stackLevel), *(float *) varPointer);
        } else if (typeId & asTYPEID_SCRIPTOBJECT) {
            asIScriptObject *obj = (asIScriptObject *) varPointer;
            if (obj)
                Logger::Log(LOG_INFO, " %s = {...}", ctx->GetVarDeclaration(n, stackLevel));
            else
                Logger::Log(LOG_INFO, " %s = <null>", ctx->GetVarDeclaration(n, stackLevel));
        } else if (typeId == engine->GetTypeIdByDecl("string")) {
            std::string *str = (std::string *) varPointer;
            if (str)
                Logger::Log(LOG_INFO, " %s = '%s'", ctx->GetVarDeclaration(n, stackLevel), str->c_str());
            else
                Logger::Log(LOG_INFO, " %s = <null>", ctx->GetVarDeclaration(n, stackLevel));
        } else {
            Logger::Log(LOG_INFO, " %s = {...}", ctx->GetVarDeclaration(n, stackLevel));
        }
    }
}

// continue with initializing everything
void ScriptEngine::init() {
    int result;

    // Create the script engine
    engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);

    // Set the message callback to receive information on errors in human readable form.
    // It's recommended to do this right after the creation of the engine, because if
    // some registration fails the engine may send valuable information to the message
    // stream.
    result = engine->SetMessageCallback(asMETHOD(ScriptEngine, msgCallback), this, asCALL_THISCALL);
    if (result < 0) {
        if (result == asINVALID_ARG) {
            Logger::Log(LOG_ERROR,
                        "ScriptEngine: One of the arguments is incorrect, e.g. obj is null for a class method.");
            return;
        } else if (result == asNOT_SUPPORTED) {
            Logger::Log(LOG_ERROR, "ScriptEngine: 	The arguments are not supported, e.g. asCALL_GENERIC.");
            return;
        }
        Logger::Log(LOG_ERROR, "ScriptEngine: Unknown error while setting up message callback");
        return;
    }

    // AngelScript doesn't have a built-in string type, as there is no definite standard
    // string type for C++ applications. Every developer is free to register it's own string type.
    // The SDK do however provide a standard add-on for registering a string type, so it's not
    // necessary to register your own string type if you don't want to.
    RegisterStdString(engine);
    RegisterScriptArray(engine,
                        true); // true = allow arrays to be registered as type[] (e.g. int[]). Needed for backwards compatibillity.
    RegisterStdStringUtils(engine); // depends on string and array
    RegisterScriptMath3D(engine); // depends on string
    RegisterScriptMath(engine);
    RegisterScriptDictionary(engine);
    RegisterScriptFile(engine);
    RegisterScriptAny(engine);

    Logger::Log(LOG_INFO, "ScriptEngine: Registration of libs done, now custom things");

    // Register ServerScript interface
    ServerScript::Register(engine);

    // Create ServerScript global instance
    ServerScript *serverscript = new ServerScript(this, seq);
    result = engine->RegisterGlobalProperty("ServerScriptClass server", serverscript);
    assert(result >= 0);

    Logger::Log(LOG_INFO, "ScriptEngine: Registration done");
}

void ScriptEngine::msgCallback(const asSMessageInfo *msg) {
    const char *type = "Error";
    if (msg->type == asMSGTYPE_INFORMATION)
        type = "Info";
    else if (msg->type == asMSGTYPE_WARNING)
        type = "Warning";

    Logger::Log(LOG_INFO, "ScriptEngine: %s (%d, %d): %s = %s", msg->section, msg->row, msg->col, type, msg->message);
}

// unused method
int ScriptEngine::loadScriptFile(const char *fileName, string &script) {
    FILE *f = fopen(fileName, "rb");
    if (!f) return 1;

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
int ScriptEngine::executeString(std::string command) {
    // This method would work if you include the scriptHelper add-on
#if 0
    if(!engine) return 0;
    if(!context) context = engine->CreateContext();
    asIScriptModule* mod = engine->GetModule("script", asGM_CREATE_IF_NOT_EXISTS);
    int result = ExecuteString(engine, command.c_str(), mod, context);
    if(result<0)
    {
        Logger::Log(LOG_ERROR, "ScriptEngine: Error while executing string: '" + command + "'.");
    }
    return result;
#endif // 0
    return 0;
}

int ScriptEngine::framestep(float dt) {
    if (!engine) return 0;
    MutexLocker scoped_lock(context_mutex);
    if (!context) context = engine->CreateContext();
    int r;

    // Copy the callback list, because the callback list itself may get changed while executing the script
    callbackList queue(callbacks["frameStep"]);

    // loop over all callbacks
    for (unsigned int i = 0; i < queue.size(); ++i) {
        // prepare the call
        r = context->Prepare(queue[i].func);
        if (r < 0) continue;

        // Set the object if present (if we don't set it, then we call a global function)
        if (queue[i].obj != NULL) {
            context->SetObject(queue[i].obj);
            if (r < 0) continue;
        }

        // Set the arguments
        context->SetArgFloat(0, dt);

        // Execute it
        r = context->Execute();
    }

    // Collect garbage
    engine->GarbageCollect(asGC_ONE_STEP);

    return 0;
}

void ScriptEngine::playerDeleted(int uid, int crash, bool doNestedCall /*= false*/) {
    if (!engine) return;
    if (!doNestedCall) MutexLocker scoped_lock(context_mutex);
    if (!context) context = engine->CreateContext();
    int r;

    // Push the state of the context if this is a nested call
    if (doNestedCall) {
        r = context->PushState();
        if (r < 0) return;
    }

    // Copy the callback list, because the callback list itself may get changed while executing the script
    callbackList queue(callbacks["playerDeleted"]);

    // loop over all callbacks
    for (unsigned int i = 0; i < queue.size(); ++i) {
        // prepare the call
        r = context->Prepare(queue[i].func);
        if (r < 0) continue;

        // Set the object if present (if we don't set it, then we call a global function)
        if (queue[i].obj != NULL) {
            context->SetObject(queue[i].obj);
            if (r < 0) continue;
        }

        // Set the arguments
        context->SetArgDWord(0, uid);
        context->SetArgDWord(1, crash);

        // Execute it
        r = context->Execute();
    }

    // Pop the state of the context if this is was a nested call
    if (doNestedCall) {
        r = context->PopState();
        if (r < 0) return;
    }


    return;
}

void ScriptEngine::playerAdded(int uid) {
    if (!engine) return;
    MutexLocker scoped_lock(context_mutex);
    if (!context) context = engine->CreateContext();
    int r;

    // Copy the callback list, because the callback list itself may get changed while executing the script
    callbackList queue(callbacks["playerAdded"]);

    // loop over all callbacks
    for (unsigned int i = 0; i < queue.size(); ++i) {
        // prepare the call
        r = context->Prepare(queue[i].func);
        if (r < 0) continue;

        // Set the object if present (if we don't set it, then we call a global function)
        if (queue[i].obj != NULL) {
            context->SetObject(queue[i].obj);
            if (r < 0) continue;
        }

        // Set the arguments
        context->SetArgDWord(0, uid);

        // Execute it
        r = context->Execute();
    }
    return;
}

int ScriptEngine::streamAdded(int uid, RoRnet::StreamRegister *reg) {
    if (!engine) return 0;
    MutexLocker scoped_lock(context_mutex);
    if (!context) context = engine->CreateContext();
    int r;
    int ret = BROADCAST_AUTO;

    // Copy the callback list, because the callback list itself may get changed while executing the script
    callbackList queue(callbacks["streamAdded"]);

    // loop over all callbacks
    for (unsigned int i = 0; i < queue.size(); ++i) {
        // prepare the call
        r = context->Prepare(queue[i].func);
        if (r < 0) continue;

        // Set the object if present (if we don't set it, then we call a global function)
        if (queue[i].obj != NULL) {
            context->SetObject(queue[i].obj);
            if (r < 0) continue;
        }

        // Set the arguments
        context->SetArgDWord(0, uid);
        context->SetArgObject(1, (void *) reg);

        // Execute it
        r = context->Execute();
        if (r == asEXECUTION_FINISHED) {
            int newRet = context->GetReturnDWord();

            // Only use the new result if it's more restrictive than what we already had
            if (newRet > ret)
                ret = newRet;
        }
    }

    return ret;
}

int ScriptEngine::playerChat(int uid, std::string msg) {
    if (!engine) return 0;
    MutexLocker scoped_lock(context_mutex);
    if (!context) context = engine->CreateContext();
    int r;
    int ret = BROADCAST_AUTO;

    // Copy the callback list, because the callback list itself may get changed while executing the script
    callbackList queue(callbacks["playerChat"]);

    // loop over all callbacks
    for (unsigned int i = 0; i < queue.size(); ++i) {
        // prepare the call
        r = context->Prepare(queue[i].func);
        if (r < 0) continue;

        // Set the object if present (if we don't set it, then we call a global function)
        if (queue[i].obj != NULL) {
            context->SetObject(queue[i].obj);
            if (r < 0) continue;
        }

        // Set the arguments
        context->SetArgDWord(0, uid);
        context->SetArgObject(1, (void *) &msg);

        // Execute it
        r = context->Execute();
        if (r == asEXECUTION_FINISHED) {
            int newRet = context->GetReturnDWord();

            // Only use the new result if it's more restrictive than what we already had
            if (newRet > ret)
                ret = newRet;
        }
    }

    return ret;
}

void ScriptEngine::gameCmd(int uid, const std::string &cmd) {
    if (!engine) return;
    MutexLocker scoped_lock(context_mutex);
    if (!context) context = engine->CreateContext();
    int r;

    // Copy the callback list, because the callback list itself may get changed while executing the script
    callbackList queue(callbacks["gameCmd"]);

    // loop over all callbacks
    for (unsigned int i = 0; i < queue.size(); ++i) {
        // prepare the call
        r = context->Prepare(queue[i].func);
        if (r < 0) continue;

        // Set the object if present (if we don't set it, then we call a global function)
        if (queue[i].obj != NULL) {
            context->SetObject(queue[i].obj);
            if (r < 0) continue;
        }

        // Set the arguments
        context->SetArgDWord(0, uid);
        context->SetArgObject(1, (void *) &cmd);

        // Execute it
        r = context->Execute();
    }

    return;
}

void ScriptEngine::timerLoop() {
    while (!exit) {
        // sleep 200 miliseconds
#ifndef _WIN32
        usleep(200000);
#else // _WIN32
        Sleep(200);
#endif // _WIN32
        // call script
        framestep(200);
    }
}

void ScriptEngine::setException(const std::string &message) {
    if (!engine || !context) {
        // There's not much we can do, except for logging the message
        Logger::Log(LOG_INFO, "--- script exception ---");
        Logger::Log(LOG_INFO, " desc: %s", (message.c_str()));
        Logger::Log(LOG_INFO, "--- end of script exception message ---");
    } else
        context->SetException(message.c_str());
}

void ScriptEngine::addCallbackScript(const std::string &type, const std::string &_func, asIScriptObject *obj) {
    if (!engine) return;

    // get the function declaration and check the type at the same time
    std::string funcDecl = "";
    if (type == "frameStep")
        funcDecl = "void " + _func + "(float)";
    else if (type == "playerChat")
        funcDecl = "int " + _func + "(int, const string &in)";
    else if (type == "gameCmd")
        funcDecl = "void " + _func + "(int, const string &in)";
    else if (type == "playerAdded")
        funcDecl = "void " + _func + "(int)";
    else if (type == "playerDeleted")
        funcDecl = "void " + _func + "(int, int)";
    else if (type == "streamAdded")
        funcDecl = "int " + _func + "(int, StreamRegister@)";
    else {
        setException("Type " + type +
                     " does not exist! Possible type strings: 'frameStep', 'playerChat', 'gameCmd', 'playerAdded', 'playerDeleted', 'streamAdded'.");
        return;
    }

    asIScriptFunction *func;
    if (obj) {
        // search for a method in the class
        asIObjectType *objType = obj->GetObjectType();
        func = objType->GetMethodByDecl(funcDecl.c_str());
        if (!func) {
            // give a nice error message that says that the method was not found.
            func = objType->GetMethodByName(_func.c_str());
            if (func)
                setException("Method '" + std::string(func->GetDeclaration(false)) + "' was found in '" +
                             objType->GetName() + "' but the correct declaration is: '" + funcDecl + "'.");
            else
                setException("Method '" + funcDecl + "' was not found in '" + objType->GetName() + "'.");
            return;
        }
    } else {
        // search for a global function
        asIScriptModule *mod = engine->GetModule("script");
        func = mod->GetFunctionByDecl(funcDecl.c_str());
        if (!func) {
            // give a nice error message that says that the function was not found.
            func = mod->GetFunctionByName(_func.c_str());
            if (func)
                setException("Function '" + std::string(func->GetDeclaration(false)) +
                             "' was found, but the correct declaration is: '" + funcDecl + "'.");
            else
                setException("Function '" + funcDecl + "' was not found.");
            return;
        }
    }

    if (callbackExists(type, func, obj))
        Logger::Log(LOG_INFO, "ScriptEngine: error: Function '" + std::string(func->GetDeclaration(false)) +
                              "' is already a callback for '" + type + "'.");
    else
        addCallback(type, func, obj);
}

void ScriptEngine::addCallback(const std::string &type, asIScriptFunction *func, asIScriptObject *obj) {
    if (!engine) return;

    if (obj) {
        // We're about to store a reference to the object, so let's tell the script engine about that
        // This avoids the object from going out of scope while we're still trying to access it.
        // BUT: it prevents local objects from being destroyed automatically....
        engine->AddRefScriptObject(obj, obj->GetObjectType());
    }

    // Add the function to the list
    callback_t tmp;
    tmp.obj = obj;
    tmp.func = func;
    callbacks[type].push_back(tmp);

    // Do we need to start the frameStep thread?
    if (type == "frameStep" && !frameStepThreadRunning) {
        frameStepThreadRunning = true;
        Logger::Log(LOG_DEBUG, "ScriptEngine: starting timer thread");
        pthread_create(&timer_thread, NULL, s_sethreadstart, this);
    }

    // finished :)
    Logger::Log(LOG_INFO, "ScriptEngine: success: Added a '" + type + "' callback for: " +
                          std::string(func->GetDeclaration(true)));
}

void ScriptEngine::deleteCallbackScript(const std::string &type, const std::string &_func, asIScriptObject *obj) {
    if (!engine) return;

    // get the function declaration and check the type at the same time
    std::string funcDecl = "";
    if (type == "frameStep")
        funcDecl = "void " + _func + "(float)";
    else if (type == "playerChat")
        funcDecl = "int " + _func + "(int, const string &in)";
    else if (type == "gameCmd")
        funcDecl = "void " + _func + "(int, const string &in)";
    else if (type == "playerAdded")
        funcDecl = "void " + _func + "(int)";
    else if (type == "playerDeleted")
        funcDecl = "void " + _func + "(int, int)";
    else if (type == "streamAdded")
        funcDecl = "int " + _func + "(int, StreamRegister@)";
    else {
        setException("Type " + type +
                     " does not exist! Possible type strings: 'frameStep', 'playerChat', 'gameCmd', 'playerAdded', 'playerDeleted', 'streamAdded'.");
        Logger::Log(LOG_INFO, "ScriptEngine: error: Failed to remove callback: " + _func);
        return;
    }

    asIScriptFunction *func;
    if (obj) {
        // search for a method in the class
        asIObjectType *objType = obj->GetObjectType();
        func = objType->GetMethodByDecl(funcDecl.c_str());
        if (!func) {
            // give a nice error message that says that the method was not found.
            func = objType->GetMethodByName(_func.c_str());
            if (func)
                setException("Method '" + std::string(func->GetDeclaration(false)) + "' was found in '" +
                             objType->GetName() + "' but the correct declaration is: '" + funcDecl + "'.");
            else
                setException("Method '" + funcDecl + "' was not found in '" + objType->GetName() + "'.");
            Logger::Log(LOG_INFO, "ScriptEngine: error: Failed to remove callback: " + funcDecl);
            return;
        }
    } else {
        // search for a global function
        asIScriptModule *mod = engine->GetModule("script");
        func = mod->GetFunctionByDecl(funcDecl.c_str());
        if (!func) {
            // give a nice error message that says that the function was not found.
            func = mod->GetFunctionByName(_func.c_str());
            if (func)
                setException("Function '" + std::string(func->GetDeclaration(false)) +
                             "' was found, but the correct declaration is: '" + funcDecl + "'.");
            else
                setException("Function '" + funcDecl + "' was not found.");
            Logger::Log(LOG_INFO, "ScriptEngine: error: Failed to remove callback: " + funcDecl);
            return;
        }
    }
    deleteCallback(type, func, obj);
}

void ScriptEngine::deleteCallback(const std::string &type, asIScriptFunction *func, asIScriptObject *obj) {
    if (!engine) return;

    for (callbackList::iterator it = callbacks[type].begin(); it != callbacks[type].end(); ++it) {
        if (it->obj == obj && it->func == func) {
            callbacks[type].erase(it);
            Logger::Log(LOG_INFO, "ScriptEngine: success: removed a '" + type + "' callback: " +
                                  std::string(func->GetDeclaration(true)));
            if (obj)
                engine->ReleaseScriptObject(obj, obj->GetObjectType());
            return;
        }
    }
    Logger::Log(LOG_INFO, "ScriptEngine: error: failed to remove callback: " + std::string(func->GetDeclaration(true)));
}

bool ScriptEngine::callbackExists(const std::string &type, asIScriptFunction *func, asIScriptObject *obj) {
    if (!engine) return false;

    for (callbackList::iterator it = callbacks[type].begin(); it != callbacks[type].end(); ++it) {
        if (it->obj == obj && it->func == func)
            return true;
    }
    return false;
}

#endif //WITH_ANGELSCRIPT
