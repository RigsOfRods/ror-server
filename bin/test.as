float time=0;
int trackuser=-1;

// shortcut for us here
string userPosition(int uid)
{
	vector3 pos = server.getUserPosition(uid);
	string res = pos.x + "," + pos.y + "," + pos.z;
	return res;
}
string userString(int uid)
{
	string res = server.getUserName(uid) + " (" + uid + ")";
	return res;
}

// called when script is loaded
void main()
{
	server.log("hello world!");
	server.say("Hello!", -1, 0);
}

// called when a player disconnects
void playerDeleted(int uid, int crash)
{
	if(crash==1)
		server.log("player " + userString(uid) + " crashed D:");
	else
		server.log("player " + userString(uid) + " disconnected.");
	
	if(uid == trackuser) trackuser=-1;
}

// called when a player connects and starts playing (already chose truck)
void playerAdded(int uid)
{
	server.log("new player " + userString(uid) + " :D");
	server.say("Hey Player, welcome here!",  uid, 0);
	server.log("player " + userString(uid) + " has auth: " + server.getUserAuth(uid));
	server.log("player " + userString(uid) + " has vehicle: " + server.getUserVehicle(uid));
	trackuser=uid;
}

// called for every chat message
int playerChat(int uid, string msg)
{
	server.log("player " + userString(uid) + " said: " + msg);
	server.say("you said: '" + msg + "'", uid, 1);
	if(msg == "!restart")
	{
		server.log("custom command executed!");
		return 0; // 0 = no publish
	}
	return -1; // dont change publish mode
}

// timer callback
void frameStep(float dt)
{
	time += dt;
	float seconds = time / 1000.0f;
	
	// now that is verbose, for testing only:
	// server.log("Time passed by: " + seconds + " s");
	if(trackuser>=0)
	{
		server.log("position of user " + trackuser + ": " + userPosition(trackuser));
	}
}