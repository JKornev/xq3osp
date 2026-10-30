/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_consolecmds.c -- text commands typed in at the local console, or
// executed by a key binding

#include "cg_local.h"
#include "../ui/ui_shared.h"
#ifdef MISSIONPACK
extern menuDef_t *menuScoreboard;
#endif



void CG_TargetCommand_f( void ) {
	int		targetNum;
	char	test[4];

	targetNum = CG_CrosshairPlayer();
	if (!targetNum ) {
		return;
	}

	trap_Argv( 1, test, 4 );
	trap_SendConsoleCommand( va( "gc %i %i", targetNum, atoi( test ) ) );
}



/*
=================
CG_SizeUp_f

Keybinding command
=================
*/
static void CG_SizeUp_f (void) {
	trap_Cvar_Set("cg_viewsize", va("%i",(int)(cg_viewsize.integer+10)));
}


/*
=================
CG_SizeDown_f

Keybinding command
=================
*/
static void CG_SizeDown_f (void) {
	trap_Cvar_Set("cg_viewsize", va("%i",(int)(cg_viewsize.integer-10)));
}


/*
=============
CG_Viewpos_f

Debugging command to print the current position
=============
*/
static void CG_Viewpos_f (void) {
	CG_Printf ("(%i %i %i) : %i\n", (int)cg.refdef.vieworg[0],
		(int)cg.refdef.vieworg[1], (int)cg.refdef.vieworg[2], 
		(int)cg.refdefViewAngles[YAW]);
}


static void CG_ScoresDown_f( void ) {

#ifdef MISSIONPACK
		CG_BuildSpectatorString();
#endif
	// OSP requests no scores in demos and keeps the request time
	// two seconds ahead
	if ( !cg.demoPlayback && cg.scoresRequestTime < cg.time ) {
		// the scores are more than two seconds out of data,
		// so request new ones
		cg.scoresRequestTime = cg.time + 2000;
		if ( !cg.demoPlayback ) {
			trap_SendClientCommand( "score" );
		}

		// leave the current scores up if they were already
		// displayed, but if this is the first hit, clear them out
		if ( !cg.showScores ) {
			cg.numScores = 0;
		}
	}
	cg.showScores = qtrue;
}

static void CG_ScoresUp_f( void ) {
	if ( cg.showScores ) {
		cg.showScores = qfalse;
		cg.scoreFadeTime = cg.time;
	}
}

#ifdef MISSIONPACK
extern menuDef_t *menuScoreboard;
void Menu_Reset();			// FIXME: add to right include file

static void CG_LoadHud_f( void) {
  char buff[1024];
	const char *hudSet;
  memset(buff, 0, sizeof(buff));

	String_Init();
	Menu_Reset();
	
	trap_Cvar_VariableStringBuffer("cg_hudFiles", buff, sizeof(buff));
	hudSet = buff;
	if (hudSet[0] == '\0') {
		hudSet = "ui/hud.txt";
	}

	CG_LoadMenus(hudSet);
  menuScoreboard = NULL;
}


static void CG_scrollScoresDown_f( void) {
	if (menuScoreboard && cg.scoreBoardShowing) {
		Menu_ScrollFeeder(menuScoreboard, FEEDER_SCOREBOARD, qtrue);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_REDTEAM_LIST, qtrue);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_BLUETEAM_LIST, qtrue);
	}
}


static void CG_scrollScoresUp_f( void) {
	if (menuScoreboard && cg.scoreBoardShowing) {
		Menu_ScrollFeeder(menuScoreboard, FEEDER_SCOREBOARD, qfalse);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_REDTEAM_LIST, qfalse);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_BLUETEAM_LIST, qfalse);
	}
}


static void CG_spWin_f( void) {
	trap_Cvar_Set("cg_cameraOrbit", "2");
	trap_Cvar_Set("cg_cameraOrbitDelay", "35");
	trap_Cvar_Set("cg_thirdPerson", "1");
	trap_Cvar_Set("cg_thirdPersonAngle", "0");
	trap_Cvar_Set("cg_thirdPersonRange", "100");
	CG_AddBufferedSound(cgs.media.winnerSound);
	//trap_S_StartLocalSound(cgs.media.winnerSound, CHAN_ANNOUNCER);
	CG_CenterPrint("YOU WIN!", SCREEN_HEIGHT * .30, 0);
}

static void CG_spLose_f( void) {
	trap_Cvar_Set("cg_cameraOrbit", "2");
	trap_Cvar_Set("cg_cameraOrbitDelay", "35");
	trap_Cvar_Set("cg_thirdPerson", "1");
	trap_Cvar_Set("cg_thirdPersonAngle", "0");
	trap_Cvar_Set("cg_thirdPersonRange", "100");
	CG_AddBufferedSound(cgs.media.loserSound);
	//trap_S_StartLocalSound(cgs.media.loserSound, CHAN_ANNOUNCER);
	CG_CenterPrint("YOU LOSE...", SCREEN_HEIGHT * .30, 0);
}

#endif

static void CG_TellTarget_f( void ) {
	int		clientNum;
	char	command[128];
	char	message[128];

	clientNum = CG_CrosshairPlayer();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

static void CG_TellAttacker_f( void ) {
	int		clientNum;
	char	command[128];
	char	message[128];

	clientNum = CG_LastAttacker();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

#ifdef MISSIONPACK
static void CG_NextTeamMember_f( void ) {
  CG_SelectNextPlayer();
}

static void CG_PrevTeamMember_f( void ) {
  CG_SelectPrevPlayer();
}

// ASS U ME's enumeration order as far as task specific orders, OFFENSE is zero, CAMP is last
//
static void CG_NextOrder_f( void ) {
	clientInfo_t *ci = cgs.clientinfo + cg.snap->ps.clientNum;
	if (ci) {
		if (!ci->teamLeader && sortedTeamPlayers[cg_currentSelectedPlayer.integer] != cg.snap->ps.clientNum) {
			return;
		}
	}
	if (cgs.currentOrder < TEAMTASK_CAMP) {
		cgs.currentOrder++;

		if (cgs.currentOrder == TEAMTASK_RETRIEVE) {
			if (!CG_OtherTeamHasFlag()) {
				cgs.currentOrder++;
			}
		}

		if (cgs.currentOrder == TEAMTASK_ESCORT) {
			if (!CG_YourTeamHasFlag()) {
				cgs.currentOrder++;
			}
		}

	} else {
		cgs.currentOrder = TEAMTASK_OFFENSE;
	}
	cgs.orderPending = qtrue;
	cgs.orderTime = cg.time + 3000;
}


static void CG_ConfirmOrder_f (void ) {
	trap_SendConsoleCommand(va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_YES));
	trap_SendConsoleCommand("+button5; wait; -button5");
	if (cg.time < cgs.acceptOrderTime) {
		trap_SendClientCommand(va("teamtask %d\n", cgs.acceptTask));
		cgs.acceptOrderTime = 0;
	}
}

static void CG_DenyOrder_f (void ) {
	trap_SendConsoleCommand(va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_NO));
	trap_SendConsoleCommand("+button6; wait; -button6");
	if (cg.time < cgs.acceptOrderTime) {
		cgs.acceptOrderTime = 0;
	}
}

static void CG_TaskOffense_f (void ) {
	if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF) {
		trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONGETFLAG));
	} else {
		trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONOFFENSE));
	}
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_OFFENSE));
}

static void CG_TaskDefense_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONDEFENSE));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_DEFENSE));
}

static void CG_TaskPatrol_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONPATROL));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_PATROL));
}

static void CG_TaskCamp_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONCAMPING));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_CAMP));
}

static void CG_TaskFollow_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOW));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_FOLLOW));
}

static void CG_TaskRetrieve_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONRETURNFLAG));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_RETRIEVE));
}

static void CG_TaskEscort_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOWCARRIER));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_ESCORT));
}

static void CG_TaskOwnFlag_f (void ) {
	trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_IHAVEFLAG));
}

static void CG_TauntKillInsult_f (void ) {
	trap_SendConsoleCommand("cmd vsay kill_insult\n");
}

static void CG_TauntPraise_f (void ) {
	trap_SendConsoleCommand("cmd vsay praise\n");
}

static void CG_TauntTaunt_f (void ) {
	trap_SendConsoleCommand("cmd vtaunt\n");
}

static void CG_TauntDeathInsult_f (void ) {
	trap_SendConsoleCommand("cmd vsay death_insult\n");
}

static void CG_TauntGauntlet_f (void ) {
	trap_SendConsoleCommand("cmd vsay kill_guantlet\n");
}

static void CG_TaskSuicide_f (void ) {
	int		clientNum;
	char	command[128];

	clientNum = CG_CrosshairPlayer();
	if ( clientNum == -1 ) {
		return;
	}

	Com_sprintf( command, 128, "tell %i suicide", clientNum );
	trap_SendClientCommand( command );
}



/*
==================
CG_TeamMenu_f
==================
*/
/*
static void CG_TeamMenu_f( void ) {
  if (trap_Key_GetCatcher() & KEYCATCH_CGAME) {
    CG_EventHandling(CGAME_EVENT_NONE);
    trap_Key_SetCatcher(0);
  } else {
    CG_EventHandling(CGAME_EVENT_TEAMMENU);
    //trap_Key_SetCatcher(KEYCATCH_CGAME);
  }
}
*/

/*
==================
CG_EditHud_f
==================
*/
/*
static void CG_EditHud_f( void ) {
  //cls.keyCatchers ^= KEYCATCH_CGAME;
  //VM_Call (cgvm, CG_EVENT_HANDLING, (cls.keyCatchers & KEYCATCH_CGAME) ? CGAME_EVENT_EDITHUD : CGAME_EVENT_NONE);
}
*/

#endif

/*
==================
CG_StartOrbit_f
==================
*/

static void CG_StartOrbit_f( void ) {
	char var[MAX_TOKEN_CHARS];

	trap_Cvar_VariableStringBuffer( "developer", var, sizeof( var ) );
	if ( !atoi(var) ) {
		return;
	}
	if (cg_cameraOrbit.value != 0) {
		trap_Cvar_Set ("cg_cameraOrbit", "0");
		trap_Cvar_Set("cg_thirdPerson", "0");
	} else {
		trap_Cvar_Set("cg_cameraOrbit", "5");
		trap_Cvar_Set("cg_thirdPerson", "1");
		trap_Cvar_Set("cg_thirdPersonAngle", "0");
		trap_Cvar_Set("cg_thirdPersonRange", "100");
	}
}

/*
=================
CG_OSPAddStr_f

OSP: "addstr <cvar> <cvar2>" appends cvar2's value to cvar
=================
*/
void CG_OSPAddStr_f( void ) {
	char	new_value[MAX_CVAR_VALUE_STRING];
	char	var_name[MAX_CVAR_VALUE_STRING];
	char	var_value[MAX_CVAR_VALUE_STRING];

	if ( trap_Argc() < 3 ) {
		return;
	}

	trap_Argv( 1, var_name, MAX_CVAR_VALUE_STRING );
	trap_Cvar_VariableStringBuffer( var_name, var_value, MAX_CVAR_VALUE_STRING );

	trap_Argv( 2, new_value, MAX_CVAR_VALUE_STRING );
	trap_Cvar_VariableStringBuffer( new_value, new_value, MAX_CVAR_VALUE_STRING );

	Q_strcat( var_value, MAX_CVAR_VALUE_STRING, new_value );
	trap_Cvar_Set( var_name, var_value );
}

/*
=================
CG_OSPVStrDown_f / CG_OSPVStrUp_f

OSP: "+vstr <down> <up>" runs one vstr on press, the other on release
=================
*/
void CG_OSPVStrDown_f( void ) {
	char	vstr[MAX_CVAR_VALUE_STRING];

	trap_Argv( 1, vstr, MAX_CVAR_VALUE_STRING );
	trap_SendConsoleCommand( va( "vstr %s;", vstr ) );
}

void CG_OSPVStrUp_f( void ) {
	char	vstr[MAX_CVAR_VALUE_STRING];

	trap_Argv( 2, vstr, MAX_CVAR_VALUE_STRING );
	trap_SendConsoleCommand( va( "vstr %s;", vstr ) );
}

/*
=================
CG_OSPSelect_f

OSP: "select <w1> <w2> ..." selects the first weapon held with ammo
=================
*/
void CG_OSPSelect_f( void ) {
	int			arg_index = 1;
	int			weapon_number;
	qboolean	done = qfalse;

	if ( !cg.snap || ( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
		return;
	}

	while ( !done && ( weapon_number = atoi( CG_Argv( arg_index++ ) ) ) ) {
		if ( weapon_number > 0 && weapon_number < 11
			&& ( cg.snap->ps.stats[STAT_WEAPONS] & ( 1 << weapon_number ) )
			&& cg.snap->ps.ammo[weapon_number] ) {
			trap_SendConsoleCommand( va( "weapon %i;", weapon_number ) );
			done = qtrue;
		}
	}
}

/*
=================
CG_OSPFireDown_f / CG_OSPFireUp_f

OSP: "+fire <w1> <w2> ..." selects as "select" and attacks
=================
*/
void CG_OSPFireDown_f( void ) {
	int			arg_index = 1;
	int			weapon_number;
	qboolean	done = qfalse;

	if ( !cg.snap || ( cg.snap->ps.pm_flags & PMF_FOLLOW ) ) {
		return;
	}

	while ( !done && ( weapon_number = atoi( CG_Argv( arg_index++ ) ) ) ) {
		if ( weapon_number > 0 && weapon_number < 11
			&& ( cg.snap->ps.stats[STAT_WEAPONS] & ( 1 << weapon_number ) )
			&& cg.snap->ps.ammo[weapon_number] ) {
			trap_SendConsoleCommand( va( "weapon %i; wait 2; +attack;", weapon_number ) );
			done = qtrue;
		}
	}
}

void CG_OSPFireUp_f( void ) {
	trap_SendConsoleCommand( "-attack;" );
}

/*
=================
CG_OSPServerVersion_f

OSP: prints the server's gameversion and referenced pk3s
=================
*/
void CG_OSPServerVersion_f( void ) {
	const char	*s;
	char		buf[4096];

	s = Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "gameversion" );
	CG_Printf( "\n^3OSP Server version: ^5\"^7%s^5\"\n\n", s );

	s = Info_ValueForKey( CG_ConfigString( CS_SYSTEMINFO ), "sv_referencedPakNames" );
	CG_Printf( "^3Referenced .pk3s:\n" );

	if ( s ) {
		char	*pos, *pack;

		Q_strncpyz( buf, s, sizeof( buf ) );
		pos = buf;
		while ( ( pack = pos ) != NULL ) {
			if ( ( pos = strchr( pack, ' ' ) ) != NULL ) {
				*pos = 0;
				CG_Printf( "   ^7%s\n", pack );
				pos++;
				while ( *pos == ' ' ) {
					pos++;
				}
			}
		}
		CG_Printf( "\n" );
	} else {
		CG_Printf( "   ^7NONE found\n\n" );
	}
}

/*
static void CG_Camera_f( void ) {
	char name[1024];
	trap_Argv( 1, name, sizeof(name));
	if (trap_loadCamera(name)) {
		cg.cameraMode = qtrue;
		trap_startCamera(cg.time);
	} else {
		CG_Printf ("Unable to load camera %s\n",name);
	}
}
*/


typedef struct {
	char	*cmd;
	void	(*function)(void);
} consoleCommand_t;

// OSP's handlers in the cg_osp_*.c files
void CG_OSPActionDown_f( void );
void CG_OSPActionUp_f( void );
void CG_OSPAddPos_f( void );
void CG_OSPClientVersion_f( void );
void CG_OSPCredits_f( void );
void CG_OSPDecalAdd_f( void );
void CG_OSPDecalDec_f( void );
void CG_OSPDecalDisable_f( void );
void CG_OSPDecalDump_f( void );
void CG_OSPDecalEdit_f( void );
void CG_OSPDecalEnable_f( void );
void CG_OSPDecalGFXNext_f( void );
void CG_OSPDecalGFXPrev_f( void );
void CG_OSPDecalInc_f( void );
void CG_OSPDecalNext_f( void );
void CG_OSPDecalPrev_f( void );
void CG_OSPDecalRotClock_f( void );
void CG_OSPDecalRotCounter_f( void );
void CG_OSPDynamicMem_f( void );
void CG_OSPHUDNext_f( void );
void CG_OSPHUDPrev_f( void );
void CG_OSPHUDStyle_f( void );
void CG_OSPMoTD_f( void );
void CG_OSPModif1Down_f( void );
void CG_OSPModif1Up_f( void );
void CG_OSPModif2Down_f( void );
void CG_OSPModif2Up_f( void );
void CG_OSPModif3Down_f( void );
void CG_OSPModif3Up_f( void );
void CG_OSPModif4Down_f( void );
void CG_OSPModif4Up_f( void );
void CG_OSPModif5Down_f( void );
void CG_OSPModif5Up_f( void );
void CG_OSPMyName_f( void );
void CG_OSPPrintTime_f( void );
void CG_OSPWStatsDown_f( void );
void CG_OSPWStatsUp_f( void );
void HMI_MenuLeft_f( void );
void HMI_MenuRight_f( void );
void HMI_Menu_f( void );

// OSP's table (68 entries)
static consoleCommand_t	commands[] = {
	{ "hudstyle", CG_OSPHUDStyle_f },
	{ "hudnext", CG_OSPHUDNext_f },
	{ "hudprev", CG_OSPHUDPrev_f },
	{ "addstr", CG_OSPAddStr_f },
	{ "+vstr", CG_OSPVStrDown_f },
	{ "-vstr", CG_OSPVStrUp_f },
	{ "+fire", CG_OSPFireDown_f },
	{ "-fire", CG_OSPFireUp_f },
	{ "select", CG_OSPSelect_f },
	{ "clientversion", CG_OSPClientVersion_f },
	{ "credits", CG_OSPCredits_f },
	{ "motd", CG_OSPMoTD_f },
	{ "myname", CG_OSPMyName_f },
	{ "serverversion", CG_OSPServerVersion_f },
	{ "testgun", CG_TestGun_f },
	{ "testmodel", CG_TestModel_f },
	{ "nextframe", CG_TestModelNextFrame_f },
	{ "prevframe", CG_TestModelPrevFrame_f },
	{ "nextskin", CG_TestModelNextSkin_f },
	{ "prevskin", CG_TestModelPrevSkin_f },
	{ "viewpos", CG_Viewpos_f },
	{ "+scores", CG_ScoresDown_f },
	{ "-scores", CG_ScoresUp_f },
	{ "+wstats", CG_OSPWStatsDown_f },
	{ "-wstats", CG_OSPWStatsUp_f },
	{ "+zoom", CG_ZoomDown_f },
	{ "-zoom", CG_ZoomUp_f },
	{ "sizeup", CG_SizeUp_f },
	{ "sizedown", CG_SizeDown_f },
	{ "weapnext", CG_NextWeapon_f },
	{ "weapprev", CG_PrevWeapon_f },
	{ "weapon", CG_Weapon_f },
	{ "tell_target", CG_TellTarget_f },
	{ "tell_attacker", CG_TellAttacker_f },
	{ "tcmd", CG_TargetCommand_f },
	{ "startOrbit", CG_StartOrbit_f },
	{ "loaddeferred", CG_LoadDeferredPlayers },
	{ "currenttime", CG_OSPPrintTime_f },
	{ "+modif1", CG_OSPModif1Down_f },
	{ "-modif1", CG_OSPModif1Up_f },
	{ "+modif2", CG_OSPModif2Down_f },
	{ "-modif2", CG_OSPModif2Up_f },
	{ "+modif3", CG_OSPModif3Down_f },
	{ "-modif3", CG_OSPModif3Up_f },
	{ "+modif4", CG_OSPModif4Down_f },
	{ "-modif4", CG_OSPModif4Up_f },
	{ "+modif5", CG_OSPModif5Down_f },
	{ "-modif5", CG_OSPModif5Up_f },
	{ "+action", CG_OSPActionDown_f },
	{ "-action", CG_OSPActionUp_f },
	{ "menuleft", HMI_MenuLeft_f },
	{ "menuright", HMI_MenuRight_f },
	{ "menu", HMI_Menu_f },
	{ "cg_dynamicmem", CG_OSPDynamicMem_f },
	{ "addpos", CG_OSPAddPos_f },
	{ "decaladd", CG_OSPDecalAdd_f },
	{ "decaldec", CG_OSPDecalDec_f },
	{ "decaldisable", CG_OSPDecalDisable_f },
	{ "decaldump", CG_OSPDecalDump_f },
	{ "decaledit", CG_OSPDecalEdit_f },
	{ "decalenable", CG_OSPDecalEnable_f },
	{ "decalgfxnext", CG_OSPDecalGFXNext_f },
	{ "decalgfxprev", CG_OSPDecalGFXPrev_f },
	{ "decalinc", CG_OSPDecalInc_f },
	{ "decalnext", CG_OSPDecalNext_f },
	{ "decalprev", CG_OSPDecalPrev_f },
	{ "decalrotclock", CG_OSPDecalRotClock_f },
	{ "decalrotcounter", CG_OSPDecalRotCounter_f },
};


/*
=================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_ConsoleCommand( void ) {
	const char	*cmd;
	int		i;

	cmd = CG_Argv(0);

	for ( i = 0 ; i < sizeof( commands ) / sizeof( commands[0] ) ; i++ ) {
		if ( !Q_stricmp( cmd, commands[i].cmd ) ) {
			commands[i].function();
			return qtrue;
		}
	}

	return qfalse;
}


/*
=================
CG_InitConsoleCommands

Let the client system know about all of our commands
so it can perform tab completion
=================
*/
void CG_InitConsoleCommands( void ) {
	int		i;

	for ( i = 0 ; i < sizeof( commands ) / sizeof( commands[0] ) ; i++ ) {
		trap_AddCommand( commands[i].cmd );
	}

	//
	// the game server will interpret these commands, which will be automatically
	// forwarded to the server after they are not recognized locally
	// OSP's list (its qagame commands; allready / addbot only on a
	// local server)
	//
	if ( cgs.localServer ) {
		trap_AddCommand( "allready" );
		trap_AddCommand( "addbot" );
	}
	trap_AddCommand( "callvote" );
	trap_AddCommand( "follow" );
	trap_AddCommand( "give" );
	trap_AddCommand( "god" );
	trap_AddCommand( "levelshot" );
	trap_AddCommand( "loaddeferred" );
	trap_AddCommand( "noclip" );
	trap_AddCommand( "notarget" );
	trap_AddCommand( "say" );
	trap_AddCommand( "say_team" );
	trap_AddCommand( "setviewpos" );
	trap_AddCommand( "team" );
	trap_AddCommand( "teamtask" );
	trap_AddCommand( "teamvote" );
	trap_AddCommand( "tell" );
	trap_AddCommand( "vote" );
	trap_AddCommand( "?" );
	trap_AddCommand( "acc" );
	trap_AddCommand( "accuracy" );
	trap_AddCommand( "autorecord" );
	trap_AddCommand( "autoscreenshot" );
	trap_AddCommand( "admin" );
	trap_AddCommand( "bottomshots" );
	trap_AddCommand( "captains" );
	trap_AddCommand( "clientversion" );
	trap_AddCommand( "coachdecline" );
	trap_AddCommand( "coachinvite" );
	trap_AddCommand( "coachkick" );
	trap_AddCommand( "coach" );
	trap_AddCommand( "commands" );
	trap_AddCommand( "cv" );
	trap_AddCommand( "drop" );
	trap_AddCommand( "follownext" );
	trap_AddCommand( "followprev" );
	trap_AddCommand( "followpowerup" );
	trap_AddCommand( "help" );
	trap_AddCommand( "hudnext" );
	trap_AddCommand( "hudprev" );
	trap_AddCommand( "hudstyle" );
	trap_AddCommand( "invite" );
	trap_AddCommand( "joincode" );
	trap_AddCommand( "kickplayer" );
	trap_AddCommand( "killstats" );
	trap_AddCommand( "kill" );
	trap_AddCommand( "lock" );
	trap_AddCommand( "maplist" );
	trap_AddCommand( "mapload" );
	trap_AddCommand( "myname" );
	trap_AddCommand( "notready" );
	trap_AddCommand( "osp" );
	trap_AddCommand( "pause" );
	trap_AddCommand( "pickplayer" );
	trap_AddCommand( "playerlist" );
	trap_AddCommand( "playernames" );
	trap_AddCommand( "players" );
	trap_AddCommand( "r_allcaptain" );
	trap_AddCommand( "r_allready" );
	trap_AddCommand( "r_help" );
	trap_AddCommand( "r_kick" );
	trap_AddCommand( "r_lockred" );
	trap_AddCommand( "r_lockblue" );
	trap_AddCommand( "r_map" );
	trap_AddCommand( "r_mpause" );
	trap_AddCommand( "r_mutespecs" );
	trap_AddCommand( "r_pause" );
	trap_AddCommand( "r_promote" );
	trap_AddCommand( "r_putred" );
	trap_AddCommand( "r_putblue" );
	trap_AddCommand( "r_remove" );
	trap_AddCommand( "r_restart" );
	trap_AddCommand( "range" );
	trap_AddCommand( "readyteam" );
	trap_AddCommand( "ready" );
	trap_AddCommand( "ref" );
	trap_AddCommand( "referee" );
	trap_AddCommand( "remove" );
	trap_AddCommand( "resign" );
	trap_AddCommand( "scores" );
	trap_AddCommand( "settings" );
	trap_AddCommand( "snapshot" );
	trap_AddCommand( "snapshotdump" );
	trap_AddCommand( "snaps" );
	trap_AddCommand( "specinvite" );
	trap_AddCommand( "speclock" );
	trap_AddCommand( "speconly" );
	trap_AddCommand( "specunlock" );
	trap_AddCommand( "statsdump" );
	trap_AddCommand( "statsred" );
	trap_AddCommand( "statsblue" );
	trap_AddCommand( "statsall" );
	trap_AddCommand( "stats" );
	trap_AddCommand( "teamready" );
	trap_AddCommand( "time" );
	trap_AddCommand( "timein" );
	trap_AddCommand( "timeout" );
	trap_AddCommand( "topshots" );
	trap_AddCommand( "unlock" );
	trap_AddCommand( "unready" );
	trap_AddCommand( "vc_follow" );
	trap_AddCommand( "vc_free" );
	trap_AddCommand( "vc_info" );
	trap_AddCommand( "vc_proximity" );
	trap_AddCommand( "viewadd" );
	trap_AddCommand( "viewall" );
	trap_AddCommand( "viewblue" );
	trap_AddCommand( "viewcam" );
	trap_AddCommand( "viewcyclenext" );
	trap_AddCommand( "viewcycleprev" );
	trap_AddCommand( "viewfollow" );
	trap_AddCommand( "viewlist" );
	trap_AddCommand( "viewnext" );
	trap_AddCommand( "viewnone" );
	trap_AddCommand( "viewprev" );
	trap_AddCommand( "viewred" );
	trap_AddCommand( "viewremove" );
	trap_AddCommand( "decaladd" );
	trap_AddCommand( "decaldec" );
	trap_AddCommand( "decaldisable" );
	trap_AddCommand( "decaldump" );
	trap_AddCommand( "decaledit" );
	trap_AddCommand( "decalenable" );
	trap_AddCommand( "decalgfxnext" );
	trap_AddCommand( "decalgfxprev" );
	trap_AddCommand( "decalinc" );
	trap_AddCommand( "decalnext" );
	trap_AddCommand( "decalprev" );
	trap_AddCommand( "decalrotclock" );
	trap_AddCommand( "decalrotcounter" );
}
