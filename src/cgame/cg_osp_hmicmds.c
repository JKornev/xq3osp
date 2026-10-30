/*
 * cg_osp_hmicmds.c: the HMI menu tables (value lists and the 15 menus) and the menu actions
 * that send commands (join team, ready, timeout, callvote, change mode) or open the font
 * submenu.
 * Data order: the value lists, the first eight menus (up to the font menu), the team overlay
 * formats, HMI_Changemode's jump table, then the last seven menus, all in source order.
 */
#include "cg_local.h"

// cg_osp_hmi.c: the generic item handlers and label functions the tables use
void HMI_Dummy( void *data, qboolean right );
char *HMI_SecondsLabel( vmCvar_t *cvar );
char *HMI_GetValueString( vmCvar_t *cvar, char **values );
void HMI_FontWidth( vmCvar_t *cvar, qboolean right );
void HMI_FontHeight( vmCvar_t *cvar, qboolean right );
void HMI_CrosshairSize( vmCvar_t *cvar, qboolean right );
void HMI_TeamchatTime( vmCvar_t *cvar, qboolean right );
void HMI_RailTrailTime( vmCvar_t *cvar, qboolean right );
void HMI_CrosshairNamesMode( vmCvar_t *cvar, qboolean right );
void HMI_ToggleCvar( vmCvar_t *cvar );
void HMI_StatusbarLayout( vmCvar_t *cvar, qboolean right );
void HMI_WeaponSwitchLayout( vmCvar_t *cvar, qboolean right );
char *HMI_StringLabel( vmCvar_t *cvar );
void HMI_Close( void *data, qboolean right );
void HMI_Push( hmiMenu_t *menu );
void HMI_SelectFirst( void );

// this file, used by the tables before their definition
void HMI_CrosshairNamesFont( vmCvar_t *cvar, qboolean right );
char *HMI_FontSizeLabel( vmCvar_t *cvar );
void HMI_FontMenu( vmCvar_t *cvar, qboolean right );
void HMI_CycleTeamoverlayFormat( void *data, qboolean right );
char *HMI_TeamoverlayFormatLabel( void );
void HMI_TeamchatLines( void *data, qboolean right );
void HMI_Changemode( void *data );

// value lists (NULL-terminated, indexed by the cvar's integer; HMI_GetValueString). Three of
// them (the position labels, the weapon bar modes and the 16 slider bars) are not used by any
// menu.
static char *hmiOnOff[] = {
	"^7[^0ON ^1OFF^7]",
	"^7[^2^BON^N ^0OFF^7]",
	NULL
};

static char *hmiStatusbarLayouts[] = {
	"default",
	"OSP",
	"OSP+ammograph",
	"OSP+ammonumber",
	"CPM-style 0",
	"CPM-style 1",
	"CPM-style 2",
	"CPM-style 3",
	"CPM-style 4",
	"Q3Comp Style1",
	"Q3Comp Style2",
	"Q3Comp Style3",
	"Q3Comp Style4",
	NULL
};

static char *hmiWeaponSwitchLayouts[] = {
	"default",
	"OSP Bottom",
	"OSP Right-FLASH",
	"OSP Right-ON",
	"OSP Left-FLASH",
	"OSP Left+Ammo",
	"CPM-style Left",
	"CPM-style Right",
	"CPM-style Bottom",
	"OFF",
	NULL
};

static char *hmiPositionLabels[] = {
	"^7(^1U-RIGHT^7)",
	"^7(^1BLAH^7)",
	"^7(^1LEFT^7)",
	NULL
};

static char *hmiWeaponBarModes[] = {
	"off",
	"horz",
	"right",
	"right perm",
	"left",
	"left perm +ammo",
	NULL
};

static char *hmiSliderBars[] = {
	"^x00FF00\x80^xFFFFFF\x83^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^xFFFFFF\x83^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^xFFFFFF\x83^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^xFFFFFF\x83^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^xFFFFFF\x83^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^xFFFFFF\x83^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^xFFFFFF\x83^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^xFFFFFF\x83^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^xFFFFFF\x83^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xFFFFFF\x83^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xFFFFFF\x83^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xFFFFFF\x83^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xFFFFFF\x83^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xFFFFFF\x83^xE01F00\x81^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xFFFFFF\x83^xF00F00\x81^xFF0000\x82",
	"^x00FF00\x80^x10EF00\x81^x20DF00\x81^x30CF00\x81^x40BF00\x81^x50AF00\x81^x609F00\x81^x708F00\x81^x807F00\x81^x906F00\x81^xA05F00\x81^xB04F00\x81^xC03F00\x81^xD02F00\x81^xE01F00\x81^xF00F00\x81^xFFFFFF\x83^xFF0000\x82",
	NULL
};

static char *hmiCenterLeft[] = {
	"CENTER",
	"^1LEFT",
	NULL
};

static char *hmiCrosshairNamesModes[] = {
	"^7[ ^1NONE ^0ANY TEAM^7 ]",
	"^7[ ^0NONE ^2ANY^0 TEAM^7 ]",
	"^7[ ^0NONE ANY^2 TEAM^7 ]",
	NULL
};

void HMI_JoinRed( void ) {
	trap_SendClientCommand( "team red" );
	HMI_Close( NULL, qfalse );
}

void HMI_JoinBlue( void ) {
	trap_SendClientCommand( "team blue" );
	HMI_Close( NULL, qfalse );
}

void HMI_JoinFree( void ) {
	trap_SendClientCommand( "team free" );
	HMI_Close( NULL, qfalse );
}

void HMI_JoinSpectator( void ) {
	trap_SendClientCommand( "team s" );
	HMI_Close( NULL, qfalse );
}

void HMI_NotImplemented( void ) {
	CG_Printf( "Function not implemented yet!\n" );
}

void HMI_Ready( void ) {
	trap_SendClientCommand( "ready" );
}

void HMI_Timeout( void ) {
	trap_SendClientCommand( "time" );
}

void HMI_TeamReady( void ) {
	trap_SendClientCommand( "teamready" );
}

// the Resign menu's "Yeah" item is an HMI_BACK item, so HMI_Trigger never calls this
void HMI_Resign( void ) {
	trap_SendClientCommand( "resign" );
}

// unreferenced
void HMI_ResignPlayer( int clientNum ) {
	trap_SendClientCommand( va( "resign %i", clientNum ) );
}

void HMI_Lock( void ) {
	trap_SendClientCommand( "lock" );
}

// unreferenced
void HMI_CallvoteKick( int clientNum ) {
	trap_SendClientCommand( va( "callvote kick %i", clientNum ) );
}

void HMI_CallvoteMapRestart( void ) {
	trap_SendClientCommand( "callvote map_restart" );
}

void HMI_CallvoteNextmap( void ) {
	trap_SendClientCommand( "callvote nextmap" );
}

static hmiMenu_t hmiCrosshairMenu = {
	"Crosshair Menu",
	{
		{ HMI_ACTION, "Draw crosshairnames for ", HMI_CrosshairNamesMode, NULL, &cg_drawCrosshairNames, hmiCrosshairNamesModes },
		{ HMI_ACTION, "Teammate crosshairhealth: ", HMI_ToggleCvar, NULL, &ch_TeamCrosshairHealth, hmiOnOff },
		{ HMI_ACTION, "Crosshairnames placement: ", HMI_ToggleCvar, NULL, &ch_CrosshairNamesLeft, hmiCenterLeft },
		{ HMI_ACTION, "Crosshairhealth", HMI_ToggleCvar, NULL, &cg_crosshairHealth, hmiOnOff },
		{ HMI_ACTION, "Change Crosshairsize: ", HMI_CrosshairSize, NULL, &cg_crosshairSize, NULL, HMI_GetValueString },
		{ HMI_ACTION, "Change crosshairnames font", HMI_CrosshairNamesFont, NULL, &cf_CrosshairNames },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiStatusbarMenu = {
	"Statusbar",
	{
		{ HMI_ACTION, "Statusbar Layout: ", HMI_StatusbarLayout, NULL, &ch_StatusbarFormat, hmiStatusbarLayouts },
		{ HMI_ACTION, "Weapon-Switch Layout: ", HMI_WeaponSwitchLayout, NULL, &ch_Weaponswitch, hmiWeaponSwitchLayouts },
		{ HMI_ACTION, "Color Team Locations: ", HMI_ToggleCvar, NULL, &ch_ColorLocations, hmiOnOff },
		{ HMI_ACTION, "Draw picked up item: ", HMI_ToggleCvar, NULL, &ch_Pickupitem, hmiOnOff },
		{ HMI_ACTION, "Toggle Lagometer: ", HMI_ToggleCvar, NULL, &cg_lagometer, hmiOnOff },
		{ HMI_ACTION, "FragMessage Font", HMI_FontMenu, NULL, &cf_Fragmsg },
		{ HMI_ACTION, "Item Pickup Font", HMI_FontMenu, NULL, &cf_Pickupstring },
		{ HMI_ACTION, " Scoresbox Font ", HMI_FontMenu, NULL, &cf_Scores },
		{ HMI_ACTION, " Statusbar Font ", HMI_FontMenu, NULL, &cf_Statusbar },
		{ HMI_ACTION, "Weapon Icon Font", HMI_FontMenu, NULL, &cf_WeaponSwitch },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiVisualsMenu = {
	"Gameplay Visuals",
	{
		{ HMI_ACTION, "Draw 3D icons     ", HMI_ToggleCvar, NULL, &cg_draw3dIcons, hmiOnOff },
		{ HMI_ACTION, "Draw Ammo warning ", HMI_ToggleCvar, NULL, &cg_drawAmmoWarning, hmiOnOff },
		{ HMI_ACTION, "Draw Attacker     ", HMI_ToggleCvar, NULL, &cg_drawAttacker, hmiOnOff },
		{ HMI_ACTION, "Draw Bloodsplatter", HMI_ToggleCvar, NULL, &cg_blood, hmiOnOff },
		{ HMI_ACTION, "Draw Gun          ", HMI_ToggleCvar, NULL, &cg_drawGun, hmiOnOff },
		{ HMI_ACTION, "Draw Marks        ", HMI_ToggleCvar, NULL, &cg_addMarks, hmiOnOff },
		{ HMI_ACTION, "Draw Rewards      ", HMI_ToggleCvar, NULL, &cg_drawRewards, hmiOnOff },
		{ HMI_ACTION, "Draw Player Lean  ", HMI_ToggleCvar, NULL, &cg_showPlayerLean, hmiOnOff },
		{ HMI_ACTION, "Draw Shadows      ", HMI_ToggleCvar, NULL, &cg_shadows, hmiOnOff },
		{ HMI_ACTION, "Draw Simpleitems  ", HMI_ToggleCvar, NULL, &cg_simpleItems, hmiOnOff },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiJoinTeamMenu = {
	"Join Team",
	{
		{ HMI_ACTION, "Join team ^1RED", HMI_JoinRed },
		{ HMI_ACTION, "Join team ^4BLUE", HMI_JoinBlue },
		{ HMI_ACTION, "Join team ^x777777FREE", HMI_JoinFree },
		{ HMI_ACTION, "Become ^7spectator", HMI_JoinSpectator },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiMatchMenu = {
	"Match Commands",
	{
		{ HMI_ACTION, "Ready up", HMI_Ready },
		{ HMI_ACTION, "Invite player to your team", HMI_NotImplemented },
		{ HMI_ACTION, "Show Scores", HMI_NotImplemented },
		{ HMI_ACTION, "Show statistics", HMI_NotImplemented },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiResignMenu = {
	"Resign",
	{
		{ HMI_LABEL, "Really Resign?" },
		{ HMI_BACK, "Yeah", HMI_Resign },
		{ HMI_BACK, "Nope" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiCaptainMenu = {
	"Teamcaptain Menu",
	{
		{ HMI_ACTION, "Call timeout", HMI_Timeout },
		{ HMI_ACTION, "Ready up team", HMI_TeamReady },
		{ HMI_ACTION, "Lock Team/Unlock team", HMI_Lock },
		{ HMI_SUBMENU, "Resign", HMI_Dummy, &hmiResignMenu },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

// "Change crosshairnames font": shows the own name as the crosshair name while the font menu is
// open, then opens it for cf_CrosshairNames
void HMI_CrosshairNamesFont( vmCvar_t *cvar, qboolean right ) {
	cg.crosshairClientTime = cg.time;
	cg.crosshairClientNum = cg.snap->ps.clientNum;
	HMI_FontMenu( cvar, right );
}

// the font menu's "FontSize: " value
char *HMI_FontSizeLabel( vmCvar_t *cvar ) {
	static char	buf[80];
	int			w, h;

	CG_OSPGetClientFontSize( cvar, &w, &h );
	Com_sprintf( buf, sizeof( buf ), "(%ix%i)", w, h );
	return buf;
}

// the font submenu, filled in for one cf_* cvar by HMI_FontMenu
static hmiMenu_t hmiFontMenu = {
	"Some stupid font",
	{
		{ HMI_LABEL, "FontSize: ", HMI_Dummy, NULL, NULL, NULL, HMI_FontSizeLabel },
		{ HMI_ACTION, "Adjust Width", HMI_FontWidth },
		{ HMI_ACTION, "Adjust Height", HMI_FontHeight },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

// opens the font submenu for a cf_* cvar
void HMI_FontMenu( vmCvar_t *cvar, qboolean right ) {
	static char	title[80];

	Com_sprintf( title, sizeof( title ), "%s Font", CG_OSPGetCvarName( cvar ) );
	hmiFontMenu.title = title;
	hmiFontMenu.items[0].data = cvar;
	hmiFontMenu.items[1].data = cvar;
	hmiFontMenu.items[2].data = cvar;
	HMI_Push( cg.hmiMenu );
	cg.hmiMenu = &hmiFontMenu;
	HMI_SelectFirst();
}

// ch_Teamoverlay presets cycled by the Teamoverlay menu
static char	*hmiTeamoverlayFormats[] = {
	"p12n h/aw l",
	"p12nwh/a l",
	"p12n hwa l",
	"p12n l hwa"
};
static int	hmiNumTeamoverlayFormats = 4;
static int	hmiTeamoverlayFormat = 0;

void HMI_CycleTeamoverlayFormat( void *data, qboolean right ) {
	if ( !cg_drawTeamOverlay.integer ) {
		CG_Printf( "not much use in cycling if teamoverlay is disabled, is it?\n" );
		return;
	}

	if ( right )
		hmiTeamoverlayFormat++;
	else
		hmiTeamoverlayFormat--;
	hmiTeamoverlayFormat = ( hmiTeamoverlayFormat + hmiNumTeamoverlayFormats ) % hmiNumTeamoverlayFormats;
	trap_SendConsoleCommand( va( "seta %s \"%s\"", CG_OSPGetCvarName( &ch_Teamoverlay ), hmiTeamoverlayFormats[hmiTeamoverlayFormat] ) );
}

char *HMI_TeamoverlayFormatLabel( void ) {
	static char	buf[24];

	Com_sprintf( buf, sizeof( buf ), "(%i of %i)", hmiTeamoverlayFormat + 1, hmiNumTeamoverlayFormats );
	return buf;
}

// cg_teamChatHeight 0..8, then fills the team chat with that many test lines
void HMI_TeamchatLines( void *data, qboolean right ) {
	int		i;
	int		lines;

	lines = cg_teamChatHeight.integer;
	if ( right ) {
		if ( lines < TEAMCHAT_HEIGHT ) {
			lines++;
		} else {
			lines = TEAMCHAT_HEIGHT;
		}
	} else {
		if ( lines > 0 ) {
			lines--;
		} else {
			lines = 0;
		}
	}
	trap_SendConsoleCommand( va( "seta %s \"%i\"", CG_OSPGetCvarName( &cg_teamChatHeight ), lines ) );

	for ( i = 0; i < lines; i++ ) {
		CG_AddToTeamChat( va( "Testing line %i, for testing purposes only.", i + 1 ) );
	}
}

// "Callvote Changemode": data is the gametype
void HMI_Changemode( void *data ) {
	gametype_t	gametype = (gametype_t)data;

	switch ( gametype ) {
	case GT_TOURNAMENT:
		trap_SendClientCommand( "callvote set1v1" );
		break;
	case GT_TEAM:
		trap_SendClientCommand( "callvote settdm" );
		break;
	case GT_CTF:
		trap_SendClientCommand( "callvote setctf" );
		break;
	case GT_FFA:
		trap_SendClientCommand( "callvote mode 0" );
		break;
	case 5:		// OSP's clan arena (id's GT_1FCTF slot)
		trap_SendClientCommand( "callvote setca" );
		break;
	default:
		CG_Printf( "issued wrong gametype in HMI_Changemode" );
		break;
	}
}

static hmiMenu_t hmiTeamoverlayMenu = {
	"Teamoverlay Menu",
	{
		{ HMI_ACTION, "Teamoverlay", HMI_ToggleCvar, NULL, &cg_drawTeamOverlay, hmiOnOff },
		{ HMI_ACTION, "Cycle Format", HMI_CycleTeamoverlayFormat, NULL, NULL, NULL, HMI_TeamoverlayFormatLabel },
		{ HMI_ACTION, "Colorize locations", HMI_ToggleCvar, NULL, &ch_ColorLocations, hmiOnOff },
		{ HMI_ACTION, "Font Teamoverlay", HMI_FontMenu, NULL, &cf_Teamoverlay },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiTeamchatMenu = {
	"Teamchat Menu",
	{
		{ HMI_ACTION, "Filter locations out of teamchat", HMI_ToggleCvar, NULL, &ch_FilterLocationsTeamchat, hmiOnOff },
		{ HMI_ACTION, "Disable console teamchat ", HMI_ToggleCvar, NULL, &ch_TeamchatOnly, hmiOnOff },
		{ HMI_ACTION, "Change nr. teamchatlines ", HMI_TeamchatLines, NULL, &cg_teamChatHeight, NULL, HMI_StringLabel },
		{ HMI_ACTION, "Change teamchattime: ", HMI_TeamchatTime, NULL, &cg_teamChatTime, NULL, HMI_SecondsLabel },
		{ HMI_ACTION, "Inverse teamchat ", HMI_ToggleCvar, NULL, &ch_InverseTeamChat, hmiOnOff },
		{ HMI_ACTION, "Draw Teambackgrounds", HMI_ToggleCvar, NULL, &ch_TeamBackground, hmiOnOff },
		{ HMI_ACTION, "Change teamchat font", HMI_FontMenu, NULL, &cf_Teamchat },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiGeneralMenu = {
	"General options",
	{
		{ HMI_ACTION, "Old CTF Sounds      ", HMI_ToggleCvar, NULL, &cg_oldCTFSounds, hmiOnOff },
		{ HMI_ACTION, "Alternate Scoreboard", HMI_ToggleCvar, NULL, &cg_q3compScoreboard, hmiOnOff },
		{ HMI_ACTION, "Predict Item Pickups", HMI_ToggleCvar, NULL, &cg_predictItems, hmiOnOff },
		{ HMI_ACTION, "Force Player models ", HMI_ToggleCvar, NULL, &cg_forceModel, hmiOnOff },
		{ HMI_ACTION, "Ammo Switch Check   ", HMI_ToggleCvar, NULL, &cg_ammoCheck, hmiOnOff },
		{ HMI_ACTION, "Draw Teambackground ", HMI_ToggleCvar, NULL, &ch_TeamBackground, hmiOnOff },
		{ HMI_ACTION, "Draw Obituaries     ", HMI_ToggleCvar, NULL, &ch_Obituaries, hmiOnOff },
		{ HMI_ACTION, "Team Railgun Colors ", HMI_ToggleCvar, NULL, &cg_teamRails, hmiOnOff },
		{ HMI_ACTION, "Railgun Trail time: ", HMI_RailTrailTime, NULL, &cg_railTrailTime, NULL, HMI_SecondsLabel },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiHudMenu = {
	"HUD Configuration",
	{
		{ HMI_SUBMENU, "Statusbar", HMI_Dummy, &hmiStatusbarMenu },
		{ HMI_SUBMENU, "General Options", HMI_Dummy, &hmiGeneralMenu },
		{ HMI_SUBMENU, "Change Teamoverlay", HMI_Dummy, &hmiTeamoverlayMenu },
		{ HMI_SUBMENU, "Change Teamchat", HMI_Dummy, &hmiTeamchatMenu },
		{ HMI_SUBMENU, "Change Crosshair", HMI_Dummy, &hmiCrosshairMenu },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiChangemodeMenu = {
	"Callvote Changemode",
	{
		{ HMI_ACTION, "1V1 ", HMI_Changemode, NULL, (void *)1 },
		{ HMI_ACTION, "TEAM", HMI_Changemode, NULL, (void *)3 },
		{ HMI_ACTION, "CTF ", HMI_Changemode, NULL, (void *)4 },
		{ HMI_ACTION, "FFA ", HMI_Changemode },
		{ HMI_ACTION, "CA  ", HMI_Changemode, NULL, (void *)5 },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

static hmiMenu_t hmiCallvoteMenu = {
	"Callvote menu",
	{
		{ HMI_ACTION, "Change map", HMI_NotImplemented },
		{ HMI_ACTION, "Restart map", HMI_CallvoteMapRestart },
		{ HMI_ACTION, "Next map", HMI_CallvoteNextmap },
		{ HMI_SUBMENU, "Change mode", HMI_Dummy, &hmiChangemodeMenu },
		{ HMI_LABEL, "" },
		{ HMI_BACK, "Back" },
		{ HMI_END }
	}
};

hmiMenu_t hmiMainMenu = {
	"Main Menu",
	{
		{ HMI_SUBMENU, "Change team", HMI_Dummy, &hmiJoinTeamMenu },
		{ HMI_SUBMENU, "Match commands", HMI_Dummy, &hmiMatchMenu },
		{ HMI_SUBMENU, "Teamcaptain commands", HMI_Dummy, &hmiCaptainMenu },
		{ HMI_SUBMENU, "HUD Configuration", HMI_Dummy, &hmiHudMenu },
		{ HMI_SUBMENU, "Gameplay Visuals", HMI_Dummy, &hmiVisualsMenu },
		{ HMI_SUBMENU, "Callvote", HMI_Dummy, &hmiCallvoteMenu },
		{ HMI_LABEL, "" },
		{ HMI_ACTION, "Quit", HMI_Close },
		{ HMI_END }
	}
};

