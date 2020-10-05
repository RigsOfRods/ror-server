/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
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
along with "Rigs of Rods Server". If not,
see <http://www.gnu.org/licenses/>.
*/

#ifdef WITH_ANGELSCRIPT

#include "config.h"
#include "logger.h"
#include "http.h"
#include "messaging.h"
#include "sequencer.h"
#include "server-script.h"

#include <cassert>
#include <string>

// ============================== Class methods ===================================

ServerScript::ServerScript(ScriptEngine *se, Sequencer *seq) : mse(se), seq(seq) {
}

ServerScript::~ServerScript() {
}

void ServerScript::log(std::string &msg) {
    Logger::Log(LOG_INFO, "SCRIPT|%s", msg.c_str());
}

void ServerScript::say(std::string &msg, int uid, int type) {
    seq->serverSayThreadSave(msg, uid, type);
}

void ServerScript::kick(int kuid, std::string &msg) {
    seq->QueueClientForDisconnect(kuid, msg.c_str(), false, false);
    mse->playerDeleted(kuid, 0, true);
}

void ServerScript::ban(int buid, std::string &msg) {
    seq->SilentBan(buid, msg.c_str(), false);
    mse->playerDeleted(buid, 0, true);
}

bool ServerScript::unban(int buid) {
    return seq->UnBan(buid);
}

std::string ServerScript::getUserName(int uid) {
    Client *c = seq->getClient(uid);
    if (!c) return "";


    return Str::SanitizeUtf8(c->user.username);
}

void ServerScript::setUserName(int uid, const std::string &username) {
    Client *c = seq->getClient(uid);
    if (!c) return;
    std::string username_sane = Str::SanitizeUtf8(username.begin(), username.end());
    strncpy(c->user.username, username_sane.c_str(), RORNET_MAX_USERNAME_LEN);
}

std::string ServerScript::getUserAuth(int uid) {
    Client *c = seq->getClient(uid);
    if (!c) return "none";
    if (c->user.authstatus & RoRnet::AUTH_ADMIN) return "admin";
    else if (c->user.authstatus & RoRnet::AUTH_MOD) return "moderator";
    else if (c->user.authstatus & RoRnet::AUTH_RANKED) return "ranked";
    else if (c->user.authstatus & RoRnet::AUTH_BOT) return "bot";
    //else if(c->user.authstatus & RoRnet::AUTH_NONE)
    return "none";
}

int ServerScript::getUserAuthRaw(int uid) {
    Client *c = seq->getClient(uid);
    if (!c) return RoRnet::AUTH_NONE;
    return c->user.authstatus;
}

void ServerScript::setUserAuthRaw(int uid, int authmode) {
    Client *c = seq->getClient(uid);
    if (!c) return;
    c->user.authstatus = authmode & ~(RoRnet::AUTH_RANKED | RoRnet::AUTH_BANNED);
}

int ServerScript::getUserColourNum(int uid) {
    Client *c = seq->getClient(uid);
    if (!c) return 0;
    return c->user.colournum;
}

void ServerScript::setUserColourNum(int uid, int num) {
    Client *c = seq->getClient(uid);
    if (!c) return;
    c->user.colournum = num;
}

std::string ServerScript::getUserToken(int uid) {
    Client *c = seq->getClient(uid);
    if (!c) return 0;
    return std::string(c->user.usertoken, 40);
}

std::string ServerScript::getUserVersion(int uid) {
    Client *c = seq->getClient(uid);
    if (!c) return 0;
    return std::string(c->user.clientversion, 25);
}

std::string ServerScript::getUserIPAddress(int uid) {
    Client *client = seq->getClient(uid);
    if (client != nullptr) {
        return client->GetIpAddress();
    }
    return "";
}

std::string ServerScript::getServerTerrain() {
    return Config::getTerrainName();
}

int ServerScript::sendGameCommand(int uid, std::string cmd) {
    return seq->sendGameCommand(uid, cmd);
}

int ServerScript::getNumClients() {
    return seq->getNumClients();
}

int ServerScript::getStartTime() {
    return seq->getStartTime();
}

int ServerScript::getTime() {
    return Messaging::getTime();
}

void ServerScript::deleteCallback(const std::string &type, const std::string &func, void *obj, int refTypeId) {
    if (refTypeId & asTYPEID_SCRIPTOBJECT && (refTypeId & asTYPEID_OBJHANDLE)) {
        mse->deleteCallbackScript(type, func, *(asIScriptObject **) obj);
    } else if (refTypeId == asTYPEID_VOID) {
        mse->deleteCallbackScript(type, func, NULL);
    } else if (refTypeId & asTYPEID_SCRIPTOBJECT) {
        // We received an object instead of a handle of the object.
        // We cannot allow this because this will crash if the deleteCallback is called from inside a constructor of a global variable.
        mse->setException(
                "server.deleteCallback should be called with a handle of the object! (that is: put an @ sign in front of the object)");

        // uncomment to enable anyway:
        //mse->deleteCallbackScript(type, func, (asIScriptObject*)obj);
    } else {
        mse->setException("The object for the callback has to be a script-class or null!");
    }
}

void ServerScript::setCallback(const std::string &type, const std::string &func, void *obj, int refTypeId) {
    if (refTypeId & asTYPEID_SCRIPTOBJECT && (refTypeId & asTYPEID_OBJHANDLE)) {
        mse->addCallbackScript(type, func, *(asIScriptObject **) obj);
    } else if (refTypeId == asTYPEID_VOID) {
        mse->addCallbackScript(type, func, NULL);
    } else if (refTypeId & asTYPEID_SCRIPTOBJECT) {
        // We received an object instead of a handle of the object.
        // We cannot allow this because this will crash if the setCallback is called from inside a constructor of a global variable.
        mse->setException(
                "server.setCallback should be called with a handle of the object! (that is: put an @ sign in front of the object)");

        // uncomment to enable anyway:
        //mse->addCallbackScript(type, func, (asIScriptObject*)obj);
    } else {
        mse->setException("The object for the callback has to be a script-class or null!");
    }
}

void ServerScript::throwException(const std::string &message) {
    mse->setException(message);
}

std::string ServerScript::get_version() {
    return std::string(VERSION);
}

std::string ServerScript::get_asVersion() {
    return std::string(ANGELSCRIPT_VERSION_STRING);
}

std::string ServerScript::get_protocolVersion() {
    return std::string(RORNET_VERSION);
}

unsigned int ServerScript::get_maxClients() { return Config::getMaxClients(); }

std::string ServerScript::get_serverName() { return Config::getServerName(); }

std::string ServerScript::get_IPAddr() { return Config::getIPAddr(); }

unsigned int ServerScript::get_listenPort() { return Config::getListenPort(); }

int ServerScript::get_serverMode() { return Config::getServerMode(); }

std::string ServerScript::get_owner() { return Config::getOwner(); }

std::string ServerScript::get_website() { return Config::getWebsite(); }

std::string ServerScript::get_ircServ() { return Config::getIRC(); }

std::string ServerScript::get_voipServ() { return Config::getVoIP(); }

int ServerScript::rangeRandomInt(int from, int to) {
    return (int) (from + (to - from) * ((float) rand() / (float) RAND_MAX));
}

void ServerScript::broadcastUserInfo(int uid) {
    seq->broadcastUserInfo(uid);
}

Http::Response ServerScript::httpRequest(std::string method,
                                         std::string host,
                                         std::string url,
                                         std::string content_type,
                                         std::string payload)
{
    Http::Response response;
    Http::Request(method, host, url, content_type, payload, &response);
    return response;
}

// ============================== Static methods and helpers ===================================

// Stream_register_t wrapper
std::string stream_register_get_name(RoRnet::StreamRegister *reg) {
    return std::string(reg->name);
}

void ServerScript::Register(asIScriptEngine* engine)
{
    int result = 0;

    // Register HttpResponse class
    Http::Register(engine);

    // Register ServerScript class itself
    result = engine->RegisterObjectType("ServerScriptClass", sizeof(ServerScript), asOBJ_REF | asOBJ_NOCOUNT);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void Log(const string &in)",
                                          asMETHOD(ServerScript, log), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void say(const string &in, int uid, int type)",
                                          asMETHOD(ServerScript, say), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void kick(int kuid, const string &in)",
                                          asMETHOD(ServerScript, kick), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void ban(int buid, const string &in)",
                                          asMETHOD(ServerScript, ban), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "bool unban(int buid)", asMETHOD(ServerScript, unban),
                                          asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "int cmd(int uid, string cmd)",
                                          asMETHOD(ServerScript, sendGameCommand), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "int getNumClients()",
                                          asMETHOD(ServerScript, getNumClients), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserName(int uid)",
                                          asMETHOD(ServerScript, getUserName), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void setUserName(int uid, const string &in)",
                                          asMETHOD(ServerScript, setUserName), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserAuth(int uid)",
                                          asMETHOD(ServerScript, getUserAuth), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "int getUserAuthRaw(int uid)",
                                          asMETHOD(ServerScript, getUserAuthRaw), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void setUserAuthRaw(int uid, int)",
                                          asMETHOD(ServerScript, setUserAuthRaw), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "int getUserColourNum(int uid)",
                                          asMETHOD(ServerScript, getUserColourNum), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void setUserColourNum(int uid, int)",
                                          asMETHOD(ServerScript, setUserColourNum), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void broadcastUserInfo(int)",
                                          asMETHOD(ServerScript, broadcastUserInfo), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserToken(int uid)",
                                          asMETHOD(ServerScript, getUserToken), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserVersion(int uid)",
                                          asMETHOD(ServerScript, getUserVersion), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string getUserIPAddress(int uid)",
                                          asMETHOD(ServerScript, getUserIPAddress), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string getServerTerrain()",
                                          asMETHOD(ServerScript, getServerTerrain), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "int getTime()", asMETHOD(ServerScript, getTime),
                                          asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "int getStartTime()",
                                          asMETHOD(ServerScript, getStartTime), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass",
                                          "void setCallback(const string &in, const string &in, ?&in)",
                                          asMETHOD(ServerScript, setCallback), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass",
                                          "void deleteCallback(const string &in, const string &in, ?&in)",
                                          asMETHOD(ServerScript, deleteCallback), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "void throwException(const string &in)",
                                          asMETHOD(ServerScript, throwException), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_version()",
                                          asMETHOD(ServerScript, get_version), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_asVersion()",
                                          asMETHOD(ServerScript, get_asVersion), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_protocolVersion()",
                                          asMETHOD(ServerScript, get_protocolVersion), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "uint get_maxClients()",
                                          asMETHOD(ServerScript, get_maxClients), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_serverName()",
                                          asMETHOD(ServerScript, get_serverName), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_IPAddr()",
                                          asMETHOD(ServerScript, get_IPAddr), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "uint get_listenPort()",
                                          asMETHOD(ServerScript, get_listenPort), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "int get_serverMode()",
                                          asMETHOD(ServerScript, get_serverMode), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_owner()", asMETHOD(ServerScript, get_owner),
                                          asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_website()",
                                          asMETHOD(ServerScript, get_website), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_ircServ()",
                                          asMETHOD(ServerScript, get_ircServ), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "string get_voipServ()",
                                          asMETHOD(ServerScript, get_voipServ), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass", "int rangeRandomInt(int, int)",
                                          asMETHOD(ServerScript, rangeRandomInt), asCALL_THISCALL);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("ServerScriptClass",
                                          "HttpResponse httpRequest(string, string, string, string, string)",
                                          asMETHOD(ServerScript, httpRequest), asCALL_THISCALL);
    assert(result >= 0);

    // Register RoRnet::StreamRegister class
    result = engine->RegisterObjectType("StreamRegister", sizeof(RoRnet::StreamRegister),
                                        asOBJ_REF | asOBJ_NOCOUNT);
    assert(result >= 0);
    result = engine->RegisterObjectMethod("StreamRegister", "string getName()",
                                          asFUNCTION(stream_register_get_name), asCALL_CDECL_OBJFIRST);
    assert(result >= 0); // (No property accessor on purpose)
    result = engine->RegisterObjectProperty("StreamRegister", "int type",
                                            offsetof(RoRnet::StreamRegister, type));
    assert(result >= 0);
    result = engine->RegisterObjectProperty("StreamRegister", "int status",
                                            offsetof(RoRnet::StreamRegister, status));
    assert(result >= 0);
    result = engine->RegisterObjectProperty("StreamRegister", "int origin_sourceid",
                                            offsetof(RoRnet::StreamRegister, origin_sourceid));
    assert(result >= 0);
    result = engine->RegisterObjectProperty("StreamRegister", "int origin_streamid",
                                            offsetof(RoRnet::StreamRegister, origin_streamid));
    assert(result >= 0);

    // Register ServerType enum for the server.serverMode attribute
    result = engine->RegisterEnum("ServerType");
    assert(result >= 0);
    result = engine->RegisterEnumValue("ServerType", "SERVER_LAN", SERVER_LAN);
    assert(result >= 0);
    result = engine->RegisterEnumValue("ServerType", "SERVER_INET", SERVER_INET);
    assert(result >= 0);
    result = engine->RegisterEnumValue("ServerType", "SERVER_AUTO", SERVER_AUTO);
    assert(result >= 0);

    // Register publish mode enum for the playerChat callback
    result = engine->RegisterEnum("broadcastType");
    assert(result >= 0);
    result = engine->RegisterEnumValue("broadcastType", "BROADCAST_AUTO", BROADCAST_AUTO);
    assert(result >= 0);
    result = engine->RegisterEnumValue("broadcastType", "BROADCAST_BLOCK", BROADCAST_BLOCK);
    assert(result >= 0);
    result = engine->RegisterEnumValue("broadcastType", "BROADCAST_NORMAL", BROADCAST_NORMAL);
    assert(result >= 0);
    result = engine->RegisterEnumValue("broadcastType", "BROADCAST_AUTHED", BROADCAST_AUTHED);
    assert(result >= 0);
    result = engine->RegisterEnumValue("broadcastType", "BROADCAST_ALL", BROADCAST_ALL);
    assert(result >= 0);

    // Register authorizations
    result = engine->RegisterEnum("authType");
    assert(result >= 0);
    result = engine->RegisterEnumValue("authType", "AUTH_NONE", RoRnet::AUTH_NONE);
    assert(result >= 0);
    result = engine->RegisterEnumValue("authType", "AUTH_ADMIN", RoRnet::AUTH_ADMIN);
    assert(result >= 0);
    result = engine->RegisterEnumValue("authType", "AUTH_RANKED", RoRnet::AUTH_RANKED);
    assert(result >= 0);
    result = engine->RegisterEnumValue("authType", "AUTH_MOD", RoRnet::AUTH_MOD);
    assert(result >= 0);
    result = engine->RegisterEnumValue("authType", "AUTH_BOT", RoRnet::AUTH_BOT);
    assert(result >= 0);
    result = engine->RegisterEnumValue("authType", "AUTH_BANNED", RoRnet::AUTH_BANNED);
    assert(result >= 0);
    result = engine->RegisterEnumValue("authType", "AUTH_ALL", 0xFFFFFFFF);
    assert(result >= 0);

    // Register serverSayType
    result = engine->RegisterEnum("serverSayType");
    assert(result >= 0);
    result = engine->RegisterEnumValue("serverSayType", "FROM_SERVER", FROM_SERVER);
    assert(result >= 0);
    result = engine->RegisterEnumValue("serverSayType", "FROM_HOST", FROM_HOST);
    assert(result >= 0);
    result = engine->RegisterEnumValue("serverSayType", "FROM_MOTD", FROM_MOTD);
    assert(result >= 0);
    result = engine->RegisterEnumValue("serverSayType", "FROM_RULES", FROM_RULES);
    assert(result >= 0);

    // register constants
    result = engine->RegisterGlobalProperty("const int TO_ALL", (void *) &TO_ALL);
    assert(result >= 0);

}

#endif // WITH_ANGELSCRIPT
