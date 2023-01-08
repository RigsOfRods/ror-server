
/*
Callbacks
---------

The game recognizes the following callbacks. 
You can register them either by using a global function of the same name
or by manually invoking `server.setCallback()`. Note that some callbacks allow
multiple handler functions (`setCallback()` adds another) while other can only
have a single handler function (`setCallback()` replaces the previous).

    void main() ~ required to exist as global function, invoked on startup.
    void frameStep(float dt_millis) ~ executed periodically, the parameter is delta time (time since last execution) in milliseconds.
    void playerAdded(int uid) ~ executed when player joins.
    void playerDeleted(int uid, int crashed) ~ executed when player leaves.
    int streamAdded(int uid, StreamRegister@ reg) ~ executed when player spawns an actor. Returns `broadcastType` which determines how the message is treated.
    int playerChat(int uid, const string &in msg) ~ ONLY ONE AT A TIME ~ executed when player sends a chat message. Returns `broadcastType` which determines how the message is treated.
    void gameCmd(int uid, const string &in cmd) ~ ONLY ONE AT A TIME ~ invoked when a script running on client calls `game.sendGameCmd()`


Constants and enumerations
--------------------------

enum serverSayType // This is used to define who says it, when the server says something
{
    FROM_SERVER = 0,
    FROM_HOST,
    FROM_MOTD,
    FROM_RULES
}

enum broadcastType // This is returned by the `playerChat()/streamAdded()` callback and determines how the message is treated.
{
    // order: least restrictive to most restrictive!
    BROADCAST_AUTO = -1,  // Do not edit the publishmode (for scripts only)
    BROADCAST_ALL,        // broadcast to all clients including sender
    BROADCAST_NORMAL,     // broadcast to all clients except sender
    BROADCAST_AUTHED,     // broadcast to authed users (bots)
    BROADCAST_BLOCK       // no broadcast
};

TO_ALL = -1 // constant for functions that receive an uid for sending something

*/

// ============================================================================
//             Callback functions recognized by the server script 
// ============================================================================

// Required. Executed once on server startup.
void main()
{
    server.Log("Callbacks with predefined names already found and registered by the script engine;");
    server.Log("now registering more callbacks manually.");
    
    // NOTE: because our callbacks are global functions, the last argument (object reference) is unused.
    server.setCallback("frameStep", "myFrameCallback", null); // Add another frameStep callback.
    server.setCallback("playerAdded", "myPlayerConnectedCallback", null); // Add another playerAdded callback.
    server.setCallback("playerDeleted", "myPlayerDisconnectCallback", null); // Add another playerDeleted callback.
    server.setCallback("streamAdded", "myStreamRegisteredCallback", null); // Add another playerDeleted callback.
    server.setCallback("playerChat", "myChatMessageCallback", null); // CAUTION! This replaces the previous callback!
    server.setCallback("gameCmd", "myCommandCallback", null); // CAUTION! This replaces the previous callback!
    
    server.Log("Example server script loaded!");
}

float g_totalTime = 0.f;
float g_timeSinceLastMsg = 0.f;

// Optional, executed periodically, the parameter is delta time (time since last execution) in milliseconds.
void frameStep(float dt_millis)
{
    // reset every 5 sec
    if (g_timeSinceLastMsg >= 5.f)
    {
        server.say("Example server script: frameStep(): total time is " + g_totalTime + " sec.", TO_ALL, FROM_SERVER);
        g_timeSinceLastMsg = 0.f;
    }
    g_totalTime += dt_millis * 0.001;
    g_timeSinceLastMsg += dt_millis * 0.001;
}

/// Optional, executed when player leaves.
/// @param uid Unique ID of the user.
/// @param crashed 1/0 was this an abrupt disconnect?
void playerDeleted(int uid, int crashed)
{
    server.say("Example server script: playerDeleted(): UID: " + uid + ".", TO_ALL, FROM_SERVER);
}

/// Optional, executed when player joins.
void playerAdded(int uid)
{
    server.say("Example server script: playerAdded(): UID: " + uid + ".", TO_ALL, FROM_SERVER);
}

/// Optional, executed when player spawns an actor.
/// @return enum broadcastType
int streamAdded(int uid, StreamRegister@ reg)
{
    server.say("Example server script: streamAdded(): UID: " + uid + ", reg: " + reg.getName() + ".", TO_ALL, FROM_SERVER);
    return BROADCAST_NORMAL;
}

/// Optional, executed when player sends a chat message.
/// @param uid Unique ID of the user who sent the chat message.
/// @return enum broadcastType
int playerChat(int uid, const string &in msg)
{
    server.say("Example server script: playerChat(): UID: " + uid + ", msg: '" + msg + "'.", TO_ALL, FROM_SERVER);
    return BROADCAST_NORMAL;
}

/// Optional, invoked when a script running on client calls `game.sendGameCmd()`
/// @param uid Unique ID of the user who sent the command string.
/// @param cmd The command string sent by the client script.
void gameCmd(int uid, const string &in cmd)
{
    server.say("Example server script: gameCmd(): UID: " + uid + ", cmd: '" + cmd + "'.", TO_ALL, FROM_SERVER);
}
    
// ============================================================================
//                  Callback functions registered manually 
// ============================================================================    

float g_mTotalTime = 0.f;
float g_mTimeSinceLastMsg = 0.f;

// frameStep
void myFrameCallback(float dt_millis)
{
    // reset every 5 sec
    if (g_mTimeSinceLastMsg >= 5.f)
    {
        server.say("Example server script: myFrameCallback(): total time is " + g_mTotalTime + " sec.", TO_ALL, FROM_SERVER);
        g_mTimeSinceLastMsg = 0.f;
    }
    g_mTotalTime += dt_millis * 0.001;
    g_mTimeSinceLastMsg += dt_millis * 0.001;
}

// playerDeleted
void myPlayerDisconnectCallback(int uid, int crashed)
{
    server.say("Example server script: myPlayerDisconnectCallback(): UID: " + uid + ".", TO_ALL, FROM_SERVER);
}

// playerAdded
void myPlayerConnectedCallback(int uid)
{
    server.say("Example server script: myPlayerConnectedCallback(): UID: " + uid + ".", TO_ALL, FROM_SERVER);
}

// streamAdded
int myStreamRegisteredCallback(int uid, StreamRegister@ reg)
{
    server.say("Example server script: myStreamRegisteredCallback(): UID: " + uid + ", reg: " + reg.getName() + ".", TO_ALL, FROM_SERVER);
    return BROADCAST_NORMAL;
}

// playerChat
int myChatMessageCallback(int uid, const string &in msg)
{
    server.say("Example server script: myChatMessageCallback(): UID: " + uid + ", msg: '" + msg + "'.", TO_ALL, FROM_SERVER);
    return BROADCAST_NORMAL;
}

// gameCmd
void myCommandCallback(int uid, const string &in cmd)
{
    server.say("Example server script: myCommandCallback(): UID: " + uid + ", cmd: '" + cmd + "'.", TO_ALL, FROM_SERVER);
}
    