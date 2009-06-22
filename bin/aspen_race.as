// some timers
float time=0, timer0=0, timer1=0;;

// some distances
float distanceTrigger=5;

// countdown variable
int racecountDown=-1;
int racecountDownNumbers=10;

// race state variable, 0=no race started, 1=driving to start, 2=running
int raceRunning=0;

// amount of participants in current race
int free_race_participants=0;

// array of UIDs of participants
int[] race_participants(30);
int[] race_checkpoints(30);


int aspen_race_len = 16;
vector3[] aspen_points(aspen_race_len);
void init_aspen_race_points()
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
	vector3 pos;
	if(server.getUserPosition(uid, pos)!=0)
	 return "error";
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
	if(server.getServerTerrain() == "aspen")
		init_aspen_race_points();
}

// called when a player disconnects
void playerDeleted(int uid, int crash)
{
	for(int i=0;i<free_race_participants;i++)
	{
		if(race_participants[i] == -1) continue;
		if(race_participants[i] == uid)
		{
			race_participants[i] = -1; // player left during the race
			// todo: inform other players?
		}
	}

}

// called when a player connects and starts playing (already chose truck)
void playerAdded(int uid)
{
	server.log("new player " + userString(uid) + " :D");
	server.say("^3Hey ^2" + server.getUserName(uid) + "^3, welcome here!",  uid, 0);
	server.say("^3You can start or join a race by saying ^2!race",  uid, 0);
	//server.log("player " + userString(uid) + " has auth: " + server.getUserAuth(uid));
	//server.log("player " + userString(uid) + " has vehicle: " + server.getUserVehicle(uid));
}

// called for every chat message
int playerChat(int uid, string msg)
{
	//server.log("player " + userString(uid) + " said: " + msg);
	//server.say("you said: '" + msg + "'", uid, 1);
	if(msg == "!race")
	{
		joinStartRace(uid);
		return 0; // 0 = no publish
	}
	if(msg == "!resetrace")
	{
		// reset stuff
		float time=0, timer0=0;

		distanceTrigger=5;

		racecountDown=-1;
		raceRunning=0;
		free_race_participants=0;
	}
	return -1; // dont change publish mode
}


int joinStartRace(int uid)
{
	if(raceRunning==2)
	{
		server.say("a race is currently running, please wait until it is over!", uid, 0);
		return 1;
	} else if (raceRunning==1 || raceRunning==0)
	{
		// preparation phase
		race_participants[free_race_participants] = uid;
		free_race_participants++;
		server.say("^1you are now participating in the race, proceed to the start line", uid, 0);
		string cmd = "game.setDirectionArrow(\"proceed to the start line!\", " + aspen_points[0].x + ", " + aspen_points[0].y + ", " + aspen_points[0].z + ");";
		server.cmd(uid, cmd);
		raceRunning=1;
	}
	return 0;
}

int raceTick(bool secondPassed, bool threeSecondsPassed)
{
	int min_req = 1;
	if(raceRunning==1 && free_race_participants < min_req)
	{
		if(secondPassed)
			for(int i=0;i<free_race_participants;i++)
			{
				if(race_participants[i] == -1) continue;
				server.say("we are waiting for " + (free_race_participants-min_req) + " players ...", race_participants[i], 0);
			}

		// waiting for more players...
	} else if (raceRunning==1 && free_race_participants >= min_req)
	{
		// check if everyone is at the starting point
		bool ok=true;
		int playerWait=-1;
		for(int i=0;i<free_race_participants;i++)
		{
			if(race_participants[i] == -1) continue;
			vector3 userpos;
			if(server.getUserPosition(race_participants[i], userpos) != 0)
				continue;
			//server.log("userpos: "+userpos.toString() + " checkpoint: " + aspen_points[0].toString());
			float dist = userpos.distance(aspen_points[0]);
			if(dist > distanceTrigger)
			{
				if(secondPassed)
					server.say("^3you are still ^2" + (dist) + "m^3 away from the checkpoint, hurry up!", race_participants[i], 0);
				ok=false;
				playerWait=race_participants[i];
				break;
			}
		}
		if(threeSecondsPassed && !ok)
		{
			for(int i=0;i<free_race_participants;i++)
			{
				if(race_participants[i] == -1) continue;
				if(race_participants[i] == playerWait) continue;
				server.say("^3we are waiting for ^2"+server.getUserName(playerWait)+" ^3to arrive at the first checkpoint...", race_participants[i], 0);
			}
			racecountDown=-1;
			return 1;
		}
		if(secondPassed && ok)
		{
			//start race
			if(racecountDown==-1)
			{
				racecountDown = racecountDownNumbers;
				for(int i=0;i<free_race_participants;i++)
				{
					if(race_participants[i] == -1) continue;
					server.say("^2STARTING COUNTDOWN", race_participants[i], 0);
				}
			}
			float fontSize = 0.5f * (1.0f - (float(racecountDown) / float(racecountDownNumbers)));
			if(fontSize<0.05f) fontSize = 0.05f;
			string cmd = "game.flashMessage(\"^1" + racecountDown + "\", 10, "+fontSize+");";
			if(racecountDown>0 && racecountDown<3)
				cmd = "game.flashMessage(\"^3" + racecountDown + "\", 10, "+fontSize+");";
			else if(racecountDown==0)
				cmd = "game.flashMessage(\"^2GO!\", 2, "+fontSize+");";
			
			for(int i=0;i<free_race_participants;i++)
			{
				if(race_participants[i] == -1) continue;
				// update flash message
				server.cmd(race_participants[i], cmd);
				// stop the timer that gets started by LUA ...
				server.cmd(race_participants[i], "game.stopTimer()");
				race_checkpoints[i] = 0; // reset checkpoint counter
			}
			
			racecountDown--;
			if(racecountDown==-1)
			{
				// finally started ...
				for(int i=0;i<free_race_participants;i++)
				{
					if(race_participants[i] == -1) continue;
					server.cmd(race_participants[i], cmd);
					server.cmd(race_participants[i], "game.startTimer()");
					server.say("^2GO!", race_participants[i], 0);
				}
				raceRunning=2;
				racecountDown=-1;
			}
		}
		
	} else if(raceRunning==2)
	{
		// race is running check the checkpoints!
		for(int i=0;i<free_race_participants;i++)
		{
			if(race_participants[i] == -1) continue;
			vector3 userpos;
			if(server.getUserPosition(race_participants[i], userpos) != 0)
				continue;
			float dist = userpos.distance(aspen_points[race_checkpoints[i]]);
			if(dist < distanceTrigger)
			{
				race_checkpoints[i]++; // if we are 5m near a checkpoint, go to the next checkpoint!
				/*
				// todo:fix this
				// find player in front and behind that one
				int nextId = i+1;
				if(nextId >= free_race_participants) nextId = -1;
				int prevId = i-1;
				if(prevId < 0) prevId = -1;
				
				if(nextId!=-1) server.say("^2"+server.getUserName(race_participants[nextId])+"^3 is " + userpos.distance(server.getUserPosition(race_participants[nextId])) + "m in front of you", race_participants[i], 0);
				if(prevId!=-1) server.say("^2"+server.getUserName(race_participants[prevId])+"^3 is " + userpos.distance(server.getUserPosition(race_participants[prevId])) + "m behind of you", race_participants[i], 0);
				*/
				
			}
			//if(secondPassed) server.say("^2"+dist+"m to checkpoint "+race_checkpoints[i], race_participants[i], 0);
		}
	}
	return 0;
}


// timer callback
void frameStep(float dt)
{
	timer0 += dt;
	bool secondPassed=false;
	if(timer0 > 999.0f)
	{
		secondPassed=true;
		timer0=0;
		timer1++;
	}
	
	bool threeSecondsPassed=false;
	if(timer1>2)
	{
		threeSecondsPassed=true;
		timer1=0;
	}

	raceTick(secondPassed, threeSecondsPassed);
}
