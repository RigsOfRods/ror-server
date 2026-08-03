
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
    void curlStatus(curlStatusType type, int n1, int n2, string displayname, string message) ~ Provides progress and result info, see `server.curlRequestAsync()`; for CURL_STATUS_PROGRESS, n1 = bytes downloaded, n2 = total bytes; otherwise n1 = CURL return code, n2 = HTTP result code.


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

enum curlStatusType // Used by `curlStatus()` callback.
{
    CURL_STATUS_INVALID,  //!< Should never be reported.
    CURL_STATUS_START,    //!< New CURL request started, n1/n2 both 0.
    CURL_STATUS_PROGRESS, //!< Download in progress, n1 = bytes downloaded, n2 = total bytes.
    CURL_STATUS_SUCCESS,  //!< CURL request finished, n1 = CURL return code, n2 = HTTP result code, message = received payload.
    CURL_STATUS_FAILURE,  //!< CURL request finished, n1 = CURL return code, n2 = HTTP result code, message = CURL error string.
};

TO_ALL = -1 // constant for functions that receive an uid for sending something

The GenericDocument tokenizer/parser API
----------------------------------------

Adopted from the game (client) to have a common data format that the game can edit/export and server can load.
Primary motivation is races, see https://github.com/RigsOfRods/rigs-of-rods/pull/3395.
Usage: start by creating empty GenericDocumentClass. You can create document by hand or load existing.
To traverse/edit tokens, you need to create GenericDocContextClass with the document as parameter.

enum TokenType
{
    TOKEN_TYPE_NONE,
    TOKEN_TYPE_LINEBREAK,    //!< Input: LF (CR is ignored); Output: platform-specific.
    TOKEN_TYPE_COMMENT,      //!< Line starting with ; (skipping whitespace).
    TOKEN_TYPE_STRING,       //!< Quoted string.
    TOKEN_TYPE_FLOAT,        //!< Numbers with or without a decimal point.
    TOKEN_TYPE_INT,          //!< Only numbers without decimal point.
    TOKEN_TYPE_BOOL,         //!< Lowercase 'true'/'false'.
    TOKEN_TYPE_KEYWORD,      //!< Unquoted string at start of line (skipping whitespace).
};

enum GenericDocumentOptions
{
    GENERIC_DOCUMENT_OPTION_ALLOW_NAKED_STRINGS, //!< Allow strings without quotes, for backwards compatibility.
    GENERIC_DOCUMENT_OPTION_ALLOW_SLASH_COMMENTS, //!< Allow comments starting with `//`. 
    GENERIC_DOCUMENT_OPTION_FIRST_LINE_IS_TITLE, //!< First non-empty & non-comment line is a naked string with spaces. 
    GENERIC_DOCUMENT_OPTION_ALLOW_SEPARATOR_COLON, //!< Allow ':' as separator between tokens.
    GENERIC_DOCUMENT_OPTION_PARENTHESES_CAPTURE_SPACES, //!< If non-empty NAKED string encounters '(', following spaces will be captured until matching ')' is found.    
    GENERIC_DOCUMENT_OPTION_ALLOW_BRACED_KEYWORDS, //!< Allow INI-like '[keyword]' tokens.
    GENERIC_DOCUMENT_OPTION_ALLOW_SEPARATOR_EQUALS, //!< Allow '=' as separator between tokens.
    GENERIC_DOCUMENT_OPTION_ALLOW_HASH_COMMENTS //!< Allow comments starting with `#`.     
};

class GenericDocumentClass
{
    bool loadFromFile(string filename, int options = 0);     // Loads and parses a document from dedicated server script directory.
    bool saveToFile(string filename);                        // Saves the document to dedicated server script directory.
};

class GenericDocContextClass
{
    GenericDocContext(GenericDocument@ d);

    // Traversal
    bool moveNext();
    uint getPos();
    bool seekNextLine();
    int countLineArgs();
    bool endOfFile(int offset = 0);
    TokenType tokenType(int offset = 0);
    
    // Token getter functions:
    // * DATATYPE string ~ TOKENTYPE String, Keyword, Comment
    // * DATATYPE float ~ TOKENTYPE Float, Int
    // * DATATYPE bool ~ TOKENTYPE Bool
    // * no DATATYPE ~ TOKENTYPE LineBreak
    
    DATATYPE getTokTOKENTYPE(int offset = 0);
    bool isTokTOKENTYPE(int offset = 0);
    
    // Token getter functions:
    // * DATATYPE const string&in ~ TOKENTYPE String, Keyword, Comment
    // * DATATYPE float ~ TOKENTYPE Float, Int
    // * DATATYPE bool ~ TOKENTYPE Bool
    // * no DATATYPE ~ TOKENTYPE LineBreak

    void appendTokens(int count); //!< Appends a series of `TokenType::NONE` and sets Pos at the first one added; use `setTok*` functions to fill them.
    bool insertToken(int offset = 0); //!< Inserts `TokenType::NONE`; @return false if offset is beyond EOF
    bool eraseToken(int offset = 0); //!< @return false if offset is beyond EOF
    
    void appendTokTOKENTYPE(DATATYPE val);  

    bool setTokTOKENTYPE(int offset, DATATYPE str);
};

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
    
    // Showcase the GenericDocument API
    loadExampleRacetrackFile("example-race.racetrack");
    
    server.Log("Example server script loaded!");
}

const float TIME_LOG_CHUNK = 5.f;
float g_totalTime = 0.f;
float g_timeSinceLastMsg = 0.f;

// For CURL progress, don't log each received byte, just
const float CURL_LOG_CHUNK = 0.1; // Log at least in 10% steps.
float g_prevCurlProgress = 0.f;
float g_lastCurlLoggedProgress = 0.f;

// Optional, executed periodically, the parameter is delta time (time since last execution) in milliseconds.
void frameStep(float dt_millis)
{
    // reset every 5 sec
    if (g_timeSinceLastMsg >= TIME_LOG_CHUNK)
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

void curlStatus(curlStatusType type, int n1, int n2, string displayname, string message)
{
    switch (type)
    {
        case CURL_STATUS_START:
            g_prevCurlProgress = 0.f;
            g_lastCurlLoggedProgress = 0.f;
            server.say("Example server script: curlStatus(): type: CURL_STATUS_START, displayname: '" + displayname + "'", TO_ALL, FROM_SERVER);
            break;
            
        case CURL_STATUS_PROGRESS:
            g_prevCurlProgress = float(n1)/float(n2);
            if (g_prevCurlProgress - g_lastCurlLoggedProgress >= CURL_LOG_CHUNK)
            {
                server.say("Example server script: curlStatus(): type: CURL_STATUS_PROGRESS (" + (g_prevCurlProgress * 100) + "%)"
                    + ", n1(bytes dl): " + n1 + ", n2(bytes total): " + n2 + ", displayname: '" + displayname + "'", TO_ALL, FROM_SERVER);
                g_lastCurlLoggedProgress = g_prevCurlProgress;
            }
            break;
        
        case CURL_STATUS_SUCCESS:
            server.say("Example server script: curlStatus(): type: CURL_STATUS_SUCCESS"
                    + ", n1(curl result): " + n1 + ", n2(HTTP result): " + n2 + ", displayname: '" + displayname + "', message(payload): '" + message + "'", TO_ALL, FROM_SERVER);
            break;
            
        case CURL_STATUS_FAILURE:
            server.say("Example server script: curlStatus(): type: CURL_STATUS_FAILURE"
                    + ", n1(curl result): " + n1 + ", n2(HTTP result): " + n2 + ", displayname: '" + displayname + "', message(CURL error): '" + message + "'", TO_ALL, FROM_SERVER);
            break;
    }
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
    if (msg == "CURL test")
    {
        server.curlRequestAsync("https://www.rigsofrods.org", "rigsofrods.org");
    }    
    return BROADCAST_NORMAL;
}

// gameCmd
void myCommandCallback(int uid, const string &in cmd)
{
    server.say("Example server script: myCommandCallback(): UID: " + uid + ", cmd: '" + cmd + "'.", TO_ALL, FROM_SERVER);
}

// ============================================================================
//                GenericDocument (.racetrack) parsing example 
// ============================================================================    

void loadExampleRacetrackFile(string filename)
{
    GenericDocumentClass doc;
    if (!doc.loadFromFile(filename, GENERIC_DOCUMENT_OPTION_ALLOW_NAKED_STRINGS))
    {
        server.say("Example server script: could not load file '"+filename
            +"' - you need to move it from '/contrib' dir to '/storage' dir.", TO_ALL, FROM_SERVER);
        return;
    }
    
    GenericDocContextClass ctx(doc);
    
    server.say("Example server script: reading GenericDocument file '"+filename+"'", TO_ALL, FROM_SERVER);
    
    // BEGIN copypasta from game's 'races.as' file, function `racesManager::addRaceFromDefinitionFile()`
    
    bool inCheckpoints = false;
    /* RORSERVER: we ignore procedural roads for this example
    bool inProceduralRoad = false;
    */
    array<uint> checkpointTokPositions; // We must pre-count checkpoints to pick finish-obj correctly.
    int highestCheckpointNum = 0; // Multiple finish lines are supported!
    while (!ctx.endOfFile())
    {
        //game.log("DBG addRaceFromDefinitionFile() token "+genericdoc_utils::tokenTypeStr(ctx.tokenType())+" at pos "+ctx.getPos());

        if (ctx.isTokKeyword(0))
        {
            if (ctx.isTokString(1) && ctx.getTokKeyword() == "racetrack_name")
            {
                server.say(" * Race name: "+ctx.getTokString(1), TO_ALL, FROM_SERVER);
            }
            if (ctx.isTokInt(1) && ctx.getTokKeyword() == "racetrack_laps")
            {
                server.say(" * Race laps: "+ctx.getTokInt(1), TO_ALL, FROM_SERVER);
            }
            else if (ctx.isTokString(1) && ctx.getTokKeyword() == "racetrack_checkpoint_object")
            {
                server.say(" * Race checkpoint-object: "+ctx.getTokString(1), TO_ALL, FROM_SERVER);
            }
            else if (ctx.isTokString(1) && ctx.getTokKeyword() == "racetrack_start_object")
            {
                server.say(" * Race start-object: "+ctx.getTokString(1), TO_ALL, FROM_SERVER);
            }
            else if (ctx.isTokString(1) && ctx.getTokKeyword() == "racetrack_finish_object")
            {
                server.say(" * Race finish-object: "+ctx.getTokString(1), TO_ALL, FROM_SERVER);
            }
            else if (ctx.getTokKeyword() == "begin_checkpoints")
            {
                inCheckpoints = true;
                server.say(" * Race checkpoints...", TO_ALL, FROM_SERVER); 
            }
            else if (ctx.getTokKeyword() == "end_checkpoints")
            {
                inCheckpoints = false;
            }
            /* RORSERVER: we ignore procedural roads for this example
            else if (ctx.getTokKeyword() == "begin_procedural_roads")
            {
                inProceduralRoad = true;
            }
            else if (ctx.getTokKeyword() == "end_procedural_roads")
            {
                inProceduralRoad = false;
            }
        }
        else if (inProceduralRoad)
        {
            ProceduralObjectClass@ road = road_utils::ParseProceduralRoadFromFile(ctx);
            if (@road != null) // Errors already logged
            {
                this.raceList[raceID].proceduralRoads.insertLast(road);
            }
            */
        } 
        else if (inCheckpoints)
        {
            if (ctx.isTokInt(0) && ctx.isTokInt(1) // chkpNum, altpathNum
                && ctx.isTokFloat(2) && ctx.isTokFloat(3) && ctx.isTokFloat(4) // Pos XYZ
                && ctx.isTokFloat(5) && ctx.isTokFloat(6) && ctx.isTokFloat(7)) // Rot XYZ
            {
                highestCheckpointNum = (ctx.getTokInt() > highestCheckpointNum) ? ctx.getTokInt() : highestCheckpointNum;
                server.say("  ** Checkpoint: chkpNum="+ctx.getTokInt(0) +", altpathNum="+ctx.getTokInt(1) // chkpNum, altpathNum
                    +", posX="+ ctx.getTokFloat(2) +", posY="+ ctx.getTokFloat(3) +", posZ="+ ctx.getTokFloat(4) // Pos XYZ
                    +", rotX="+ ctx.getTokFloat(5) +", rotY="+ ctx.getTokFloat(6) +", rotZ="+ ctx.getTokFloat(7), // Rot XYZ
                    TO_ALL, FROM_SERVER);
            }
        }
        ctx.seekNextLine();
    }

    // END copypasta    
}
    