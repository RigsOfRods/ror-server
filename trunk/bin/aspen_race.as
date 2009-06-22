float time=0;
int trackuser=-1;

int aspen_race_len = 16;
vector3[] aspen_points(aspen_race_len);
void init_race_points()
{
	aspen_points[0] = vector3(1788.565918f, 13.523803f, 2188.309326f);
	aspen_points[1] = vector3(1272.392822f, 52.455555f, 2639.341553f);
	aspen_points[2] = vector3(976.396179f, 139.362503f, 2253.451660f);
	aspen_points[3] = vector3(610.334473f, 122.214699f, 2292.108643f);
	aspen_points[4] = vector3(425.260101f, 154.688690f, 1892.544434f);
	aspen_points[5] = vector3(326.887360f, 175.461609f, 1637.232178f);
	aspen_points[6] = vector3(207.137665f, 190.209656f, 1017.338806f);
	aspen_points[7] = vector3(598.222473f, 204.074921f, 721.008057f);
	aspen_points[8] = vector3(871.652405f, 185.921021f, 887.720337f);
	aspen_points[9] = vector3(1244.441284f, 101.542374f, 1086.691650f);
	aspen_points[10] = vector3(1534.553711f, 88.164223f, 755.955933f);
	aspen_points[11] = vector3(1659.338379f, 114.786842f, 259.588989f);
	aspen_points[12] = vector3(1745.098389f, 125.204323f, 457.132690f);
	aspen_points[13] = vector3(1959.770996f, 95.293251f, 363.643250f);
	aspen_points[14] = vector3(2001.199341f, 41.409641f, 802.094543f);
	aspen_points[15] = vector3(2251.579834f, 13.595664f, 1292.946655f);
}

// shortcut for us here
string userPosition(int uid)
{
	vector3 pos = server.getUserPosition(uid);
	return pos.toString();
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
	init_race_points();
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
	if(trackuser>=0 && server.getServerTerrain() == "aspen")
	{
		vector3 userpos = server.getUserPosition(trackuser);
		float minlen = 999999;
		int nearest=-1;
		for(int i=0;i<aspen_race_len;i++)
		{
			float len = userpos.distance(aspen_points[i]);
			if (len < minlen)
			{
				minlen = len;
				nearest = i;
			}
		}
		server.log("just " + minlen + "m to checkpoint " + nearest);
		server.log("just " + userpos.distance(aspen_points[0]) + "m to checkpoint 0 (" + aspen_points[0].toString() + ")");
	}
	
	
}