float time=0;
int trackuser=-1;

int aspen_race_len = 16;
vector3[] aspen_points(aspen_race_len);
void init_race_points()
{
	aspen_points[0] = vector3(1788.565918, 13.523803, 2188.309326);
	aspen_points[1] = vector3(1272.392822, 52.455555, 2639.341553);
	aspen_points[2] = vector3(976.396179, 139.362503, 2253.451660);
	aspen_points[3] = vector3(610.334473, 122.214699, 2292.108643);
	aspen_points[4] = vector3(425.260101, 154.688690, 1892.544434);
	aspen_points[5] = vector3(326.887360, 175.461609, 1637.232178);
	aspen_points[6] = vector3(207.137665, 190.209656, 1017.338806);
	aspen_points[7] = vector3(598.222473, 204.074921, 721.008057);
	aspen_points[8] = vector3(871.652405, 185.921021, 887.720337);
	aspen_points[9] = vector3(1244.441284, 101.542374, 1086.691650);
	aspen_points[10] = vector3(1534.553711, 88.164223, 755.955933);
	aspen_points[11] = vector3(1659.338379, 114.786842, 259.588989);
	aspen_points[12] = vector3(1745.098389, 125.204323, 457.132690);
	aspen_points[13] = vector3(1959.770996, 95.293251, 363.643250);
	aspen_points[14] = vector3(2001.199341, 41.409641, 802.094543);
	aspen_points[15] = vector3(2251.579834, 13.595664, 1292.946655);
}

// shortcut for us here
string formatPos(vector3 pos)
{
	return pos.x + "," + pos.y + "," + pos.z;
}

string userPosition(int uid)
{
	vector3 pos = server.getUserPosition(uid);
	return formatPos(pos);
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
		server.log("just " + userpos.distance(aspen_points[0]) + "m to checkpoint 0 (" + formatPos(aspen_points[0]) + ")");
	}
	
	
}