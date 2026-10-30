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
#include "../game/q_shared.h"
#include "tr_types.h"
#include "../game/bg_public.h"
#include "cg_public.h"


// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

#ifdef MISSIONPACK
#define CG_FONT_THRESHOLD 0.1
#endif

#define	POWERUP_BLINKS		5

#define	POWERUP_BLINK_TIME	1000
#define	FADE_TIME			200
#define	PULSE_TIME			200
#define	DAMAGE_DEFLECT_TIME	100
#define	DAMAGE_RETURN_TIME	400
#define DAMAGE_TIME			500
#define	LAND_DEFLECT_TIME	150
#define	LAND_RETURN_TIME	300
#define	STEP_TIME			200
#define	DUCK_TIME			100
#define	PAIN_TWITCH_TIME	200
#define	WEAPON_SELECT_TIME	1400
#define	ITEM_SCALEUP_TIME	1000
#define	ZOOM_TIME			150
#define	ITEM_BLOB_TIME		200
#define	MUZZLE_FLASH_TIME	20
#define	SINK_TIME			1000		// time for fragments to sink into ground before going away
#define	ATTACKER_HEAD_TIME	10000
#define	REWARD_TIME			3000

#define	PULSE_SCALE			1.5			// amount to scale up the icons when activating

#define	MAX_STEP_CHANGE		32

#define	MAX_VERTS_ON_POLY	10
#define	MAX_MARK_POLYS		256

#define STAT_MINUS			10	// num frame for '-' stats digit

#define	ICON_SIZE			48
#define	CHAR_WIDTH			32
#define	CHAR_HEIGHT			48
#define	TEXT_ICON_SPACE		4

#define	TEAMCHAT_WIDTH		80
#define TEAMCHAT_HEIGHT		8

// very large characters
#define	GIANT_WIDTH			32
#define	GIANT_HEIGHT		48

#define	NUM_CROSSHAIRS		10

#define TEAM_OVERLAY_MAXNAME_WIDTH	12
#define TEAM_OVERLAY_MAXLOCATION_WIDTH	16

#define	DEFAULT_MODEL			"sarge"
#ifdef MISSIONPACK
#define	DEFAULT_TEAM_MODEL		"james"
#define	DEFAULT_TEAM_HEAD		"*james"
#else
#define	DEFAULT_TEAM_MODEL		"sarge"
#define	DEFAULT_TEAM_HEAD		"sarge"
#endif

#define DEFAULT_REDTEAM_NAME		"Stroggs"
#define DEFAULT_BLUETEAM_NAME		"Pagans"

typedef enum {
	FOOTSTEP_NORMAL,
	FOOTSTEP_BOOT,
	FOOTSTEP_FLESH,
	FOOTSTEP_MECH,
	FOOTSTEP_ENERGY,
	FOOTSTEP_METAL,
	FOOTSTEP_SPLASH,

	FOOTSTEP_TOTAL
} footstep_t;

typedef enum {
	IMPACTSOUND_DEFAULT,
	IMPACTSOUND_METAL,
	IMPACTSOUND_FLESH
} impactSound_t;

//=================================================

// player entities need to track more information
// than any other type of entity.

// note that not every player entity is a client entity,
// because corpses after respawn are outside the normal
// client numbering range

// when changing animation, set animationTime to frameTime + lerping time
// The current lerp will finish out, then it will lerp to the new animation
typedef struct {
	int			oldFrame;
	int			oldFrameTime;		// time when ->oldFrame was exactly on

	int			frame;
	int			frameTime;			// time when ->frame will be exactly on

	float		backlerp;

	float		yawAngle;
	qboolean	yawing;
	float		pitchAngle;
	qboolean	pitching;

	int			animationNumber;	// may include ANIM_TOGGLEBIT
	animation_t	*animation;
	int			animationTime;		// time when the first frame of the animation will be exact
} lerpFrame_t;


typedef struct {
	lerpFrame_t		legs, torso, flag;
	int				painTime;
	int				painDirection;	// flip from 0 to 1
	int				lightningFiring;

	// railgun trail spawning
	vec3_t			railgunImpact;
	qboolean		railgunFlash;

	// machinegun spinning
	float			barrelAngle;
	int				barrelTime;
	qboolean		barrelSpinning;
} playerEntity_t;

//=================================================



// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
typedef struct centity_s {
	entityState_t	currentState;	// from cg.frame
	entityState_t	nextState;		// from cg.nextFrame, if available
	qboolean		interpolate;	// true if next is valid to interpolate to
	qboolean		currentValid;	// true if cg.frame holds this entity

	int				muzzleFlashTime;	// move to playerEntity?
	int				previousEvent;
	int				teleportFlag;

	int				trailTime;		// so missile trails can handle dropped initial packets
	int				dustTrailTime;
	int				miscTime;

	int				snapShotTime;	// last time this entity was found in a snapshot

	playerEntity_t	pe;

	int				errorTime;		// decay the error from this time
	vec3_t			errorOrigin;
	vec3_t			errorAngles;
	
	qboolean		extrapolated;	// false if origin / angles is an interpolation
	vec3_t			rawOrigin;
	vec3_t			rawAngles;

	vec3_t			beamEnd;

	// exact interpolated position of entity on this frame
	vec3_t			lerpOrigin;
	vec3_t			lerpAngles;
} centity_t;


//======================================================================

// local entities are created as a result of events or predicted actions,
// and live independantly from all server transmitted entities

typedef struct markPoly_s {
	struct markPoly_s	*prevMark, *nextMark;
	int			time;
	qhandle_t	markShader;
	qboolean	alphaFade;		// fade alpha instead of rgb
	float		color[4];
	poly_t		poly;
	polyVert_t	verts[MAX_VERTS_ON_POLY];
} markPoly_t;


typedef enum {
	LE_MARK,
	LE_EXPLOSION,
	LE_SPRITE_EXPLOSION,
	LE_FRAGMENT,
	LE_MOVE_SCALE_FADE,
	LE_FALL_SCALE_FADE,
	LE_FADE_RGB,
	LE_SCALE_FADE,
	LE_SCOREPLUM,
#ifdef MISSIONPACK
	LE_KAMIKAZE,
	LE_INVULIMPACT,
	LE_INVULJUICED,
	LE_SHOWREFENTITY
#endif
} leType_t;

typedef enum {
	LEF_PUFF_DONT_SCALE  = 0x0001,			// do not scale size over time
	LEF_TUMBLE			 = 0x0002,			// tumble over time, used for ejecting shells
	LEF_SOUND1			 = 0x0004,			// sound 1 for kamikaze
	LEF_SOUND2			 = 0x0008			// sound 2 for kamikaze
} leFlag_t;

typedef enum {
	LEMT_NONE,
	LEMT_BURN,
	LEMT_BLOOD
} leMarkType_t;			// fragment local entities can leave marks on walls

typedef enum {
	LEBS_NONE,
	LEBS_BLOOD,
	LEBS_BRASS
} leBounceSoundType_t;	// fragment local entities can make sounds on impacts

typedef struct localEntity_s {
	struct localEntity_s	*prev, *next;
	leType_t		leType;
	int				leFlags;

	int				startTime;
	int				endTime;
	int				fadeInTime;

	float			lifeRate;			// 1.0 / (endTime - startTime)

	trajectory_t	pos;
	trajectory_t	angles;

	float			bounceFactor;		// 0.0 = no bounce, 1.0 = perfect

	float			color[4];

	float			radius;

	float			light;
	vec3_t			lightColor;

	leMarkType_t		leMarkType;		// mark to leave on fragment impact
	leBounceSoundType_t	leBounceSoundType;

	refEntity_t		refEntity;		
} localEntity_t;

//======================================================================


typedef struct {
	int				client;
	int				score;
	int				ping;
	int				time;
	int				scoreFlags;
	int				powerUps;
	int				accuracy;
	int				impressiveCount;
	int				excellentCount;
	int				guantletCount;
	int				defendCount;
	int				assistCount;
	int				captures;
	qboolean	perfect;
	int				team;
} score_t;

// each client has an associated clientInfo_t
// that contains media references necessary to present the
// client model and other color coded effects
// this is regenerated each time a client's configstring changes,
// usually as a result of a userinfo (name, model, etc) change
#define	MAX_CUSTOM_SOUNDS	32

typedef struct {
	qboolean		infoValid;

	char			name[MAX_QPATH];
	team_t			team;

	int				botSkill;		// 0 = not bot, 1-5 = bot

	vec3_t			color1;
	vec3_t			color2;
	// OSP's 44 bytes are split: three model colours after color2 (CG_NewClientInfo
	// fills color1 and these from "c1" with CG_OSPColorsFromString) and two
	// ints among the team info fields
	vec3_t			headColor;
	vec3_t			torsoColor;
	vec3_t			legsColor;

	int				score;			// updated by score servercmds
	int				location;		// location index for team mode
	int				health;			// you only get this info about your teammates
	int				armor;
	int				curWeapon;
	int				ammo;			// OSP: multiview "tinfo" "<ammo>_<score>_<ping>" token, shown next to the weapon in the multiview window

	int				handicap;		// "hc"
	int				wins, losses;	// in tourney mode

	int				teamTask;		// task in teamplay (offence/defence); OSP's multiview ping ("tinfo" ammo_score_ping, CG_OSPDrawMultiviewWindow)
	qboolean		teamLeader;		// true when this is a team leader
	int				osp_unk_b4;		// unknown, OSP field

	int				powerups;		// so can display quad/flag status

	int				medkitUsageTime;
	int				invulnerabilityStartTime;
	int				invulnerabilityStopTime;

	int				breathPuffTime;

	// when clientinfo is changed, the loading of models/skins/sounds
	// can be deferred until you are dead, to prevent hitches in
	// gameplay
	char			modelName[MAX_QPATH];
	char			skinName[MAX_QPATH];
	char			headModelName[MAX_QPATH];
	char			headSkinName[MAX_QPATH];
	// (OSP: no redTeam / blueTeam in OSP)
	qboolean		deferred;

	qboolean		newAnims;		// true if using the new mission pack animations
	qboolean		fixedlegs;		// true if legs yaw is always the same as torso yaw
	qboolean		fixedtorso;		// true if torso never changes yaw

	vec3_t			headOffset;		// move head in icon views
	footstep_t		footsteps;
	gender_t		gender;			// from model

	qhandle_t		legsModel;
	qhandle_t		legsSkin;

	qhandle_t		torsoModel;
	qhandle_t		torsoSkin;

	qhandle_t		headModel;
	qhandle_t		headSkin;

	qhandle_t		modelIcon;

	animation_t		animations[MAX_TOTALANIMATIONS];

	sfxHandle_t		sounds[MAX_CUSTOM_SOUNDS];

	// OSP fields after sounds
	vec3_t			customLocation;
	team_t			rt;
	team_t			st;
} clientInfo_t;


// each WP_* weapon enum has an associated weaponInfo_t
// that contains media references necessary to present the
// weapon and its effects
typedef struct weaponInfo_s {
	qboolean		registered;
	gitem_t			*item;

	qhandle_t		handsModel;			// the hands don't actually draw, they just position the weapon
	qhandle_t		weaponModel;
	qhandle_t		barrelModel;
	qhandle_t		flashModel;

	vec3_t			weaponMidpoint;		// so it will rotate centered instead of by tag

	float			flashDlight;
	vec3_t			flashDlightColor;
	sfxHandle_t		flashSound[4];		// fast firing weapons randomly choose

	qhandle_t		weaponIcon;
	qhandle_t		ammoIcon;

	qhandle_t		ammoModel;

	qhandle_t		missileModel;
	sfxHandle_t		missileSound;
	void			(*missileTrailFunc)( centity_t *, const struct weaponInfo_s *wi );
	float			missileDlight;
	vec3_t			missileDlightColor;
	int				missileRenderfx;

	void			(*ejectBrassFunc)( centity_t * );

	float			trailRadius;
	float			wiTrailTime;

	sfxHandle_t		readySound;
	sfxHandle_t		firingSound;
	qboolean		loopFireSound;
} weaponInfo_t;


// each IT_* item has an associated itemInfo_t
// that constains media references necessary to present the
// item and its effects
typedef struct {
	qboolean		registered;
	qhandle_t		models[MAX_ITEM_MODELS];
	qhandle_t		icon;
} itemInfo_t;


typedef struct {
	int				itemNum;
} powerupInfo_t;


#define MAX_SKULLTRAIL		10

typedef struct {
	vec3_t positions[MAX_SKULLTRAIL];
	int numpositions;
} skulltrail_t;


#define MAX_REWARDSTACK		10
#define MAX_SOUNDBUFFER		20

// OSP limits
#define MAX_OSP_CHS_FIXED_SLOTS	64		// CHS slots below this are fixed, CHS_Begin( -1, ... ) allocates from here up
#define MAX_OSP_CUSTOM_GFX		16		// the server's custom pictures gfx/2d/custom1..16 (configstrings 824..839)
#define MAX_OSP_DECALS			32		// decal slots (configstrings 840..871)
#define MAX_OSP_SLIDING_WINDOWS	16		// sliding text windows (ospPrintContext)
#define MAX_OSP_SLIDING_LINES	24		// lines in one sliding text window
#define MAX_OSP_SLIDING_CHARS	128		// characters in one sliding text window line

// OSP's in-game menu ("HMI"; cg_osp_hmicmds.c / cg_osp_hmi.c). A menu is a title and up to
// 20 items of 28 bytes, ended by an HMI_END item (564 bytes per menu).
#define HMI_MAX_ITEMS		20
#define HMI_STACK_SIZE		12

// item types
typedef enum {
	HMI_HIDDEN = -2,		// not drawn, not counted (no menu uses it)
	HMI_END,				// -1: ends the item list
	HMI_LABEL,				// 0: text only (separators, "FontSize: ", "Really Resign?"), not selectable
	HMI_ACTION,				// 1: menuleft / menuright call func( data, right )
	HMI_SUBMENU,			// 2: opens submenu (func is HMI_Dummy and never called)
	HMI_BACK				// 3: back to the previous menu (func is never called)
} hmiItemType_t;

typedef struct hmiMenu_s hmiMenu_t;

typedef struct {
	hmiItemType_t	type;
	char		*label;
	void		(*func)();			// ( void *data, qboolean right )
	hmiMenu_t	*submenu;			// HMI_SUBMENU
	void		*data;				// a vmCvar_t *, or HMI_Changemode's gametype
	char		**values;			// NULL-terminated value names, indexed by the cvar's integer
	char		*(*labelFunc)();	// ( void *data, char **values ), the value drawn after the label
} hmiItem_t;

struct hmiMenu_s {
	char		*title;
	hmiItem_t	items[HMI_MAX_ITEMS];
};

//======================================================================

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

#define MAX_PREDICTED_EVENTS	16
 
typedef struct {
	int			clientFrame;		// incremented each frame

	int			clientNum;
	
	qboolean	demoPlayback;
	qboolean	levelShot;			// taking a level menu screenshot
	int			deferredPlayerLoading;
	qboolean	loading;			// don't defer players at initial startup
	qboolean	intermissionStarted;	// don't play voice rewards, because game will end shortly

	// there are only one or two snapshot_t that are relevent at a time
	int			latestSnapshotNum;	// the number of snapshots the client system has received
	int			latestSnapshotTime;	// the time from latestSnapshotNum, so we don't need to read the snapshot yet

	snapshot_t	*snap;				// cg.snap->serverTime <= cg.time
	snapshot_t	*nextSnap;			// cg.nextSnap->serverTime > cg.time, or NULL
	snapshot_t	activeSnapshots[2];

	float		frameInterpolation;	// (float)( cg.time - cg.frame->serverTime ) / (cg.nextFrame->serverTime - cg.frame->serverTime)

	qboolean	thisFrameTeleport;
	qboolean	nextFrameTeleport;

	int			frametime;		// cg.time - cg.oldTime

	int			time;			// this is the time value that the client
								// is rendering at.
	int			oldTime;		// time at last frame, used for missile trails and prediction checking

	int			physicsTime;	// either cg.snap->time or cg.nextSnap->time

	int			timelimitWarnings;	// 5 min, 1 min, overtime
	int			fraglimitWarnings;

	qboolean	mapRestart;			// set on a map restart to set back the weapon

	qboolean	renderingThirdPerson;		// during deaths, chasecams, etc

	// prediction state
	qboolean	hyperspace;				// true if prediction has hit a trigger_teleport
	playerState_t	predictedPlayerState;
	centity_t		predictedPlayerEntity;
	qboolean	validPPS;				// clear until the first call to CG_PredictPlayerState
	int			predictedErrorTime;
	vec3_t		predictedError;

	int			eventSequence;
	int			predictableEvents[MAX_PREDICTED_EVENTS];

	float		stepChange;				// for stair up smoothing
	int			stepTime;

	float		duckChange;				// for duck viewheight smoothing
	int			duckTime;

	float		landChange;				// for landing hard
	int			landTime;

	// input state sent to server
	int			weaponSelect;

	// auto rotating items
	vec3_t		autoAngles;
	vec3_t		autoAxis[3];
	vec3_t		autoAnglesFast;
	vec3_t		autoAxisFast[3];

	// view rendering
	refdef_t	refdef;
	vec3_t		refdefViewAngles;		// will be converted to refdef.viewaxis

	// zoom key
	qboolean	zoomed;
	int			zoomTime;
	float		zoomSensitivity;

	// information screen text during loading
	char		infoScreenText[MAX_STRING_CHARS];

	// scoreboard
	int			scoresRequestTime;
	int			numScores;
	int			selectedScore;
	int			teamScores[2];
	score_t		scores[MAX_CLIENTS];
	qboolean	showScores;
	qboolean	scoreBoardShowing;
	int			scoreFadeTime;
	char		killerName[MAX_NAME_LENGTH];
	char			spectatorList[MAX_STRING_CHARS];		// list of names
	int				spectatorLen;												// length of list
	float			spectatorWidth;											// width in device units
	int				spectatorTime;											// next time to offset
	int				spectatorPaintX;										// current paint x
	int				spectatorPaintX2;										// current paint x
	int				spectatorOffset;										// current offset from start
	int				spectatorPaintLen; 									// current offset from start

	// skull trails
	skulltrail_t	skulltrails[MAX_CLIENTS];

	// centerprinting
	int			centerPrintTime;
	int			centerPrintCharWidth;
	int			centerPrintY;
	char		centerPrint[1024];
	int			centerPrintLines;

	// low ammo warning state
	int			lowAmmoWarning;		// 1 = low, 2 = empty

	// kill timers for carnage reward
	int			lastKillTime;

	// crosshair client ID
	int			crosshairClientNum;
	int			crosshairClientTime;

	// powerup active flashing
	int			powerupActive;
	int			powerupTime;

	// attacking player
	int			attackerTime;
	int			voiceTime;

	// reward medals
	int			rewardStack;
	int			rewardTime;
	int			rewardCount[MAX_REWARDSTACK];
	qhandle_t	rewardShader[MAX_REWARDSTACK];
	qhandle_t	rewardSound[MAX_REWARDSTACK];

	// sound buffer mainly for announcer sounds
	int			soundBufferIn;
	int			soundBufferOut;
	int			soundTime;
	qhandle_t	soundBuffer[MAX_SOUNDBUFFER];

	// (OSP: no voice chat buffer in OSP: voiceChatTime, voiceChatBufferIn/Out removed)

	// warmup countdown
	int			warmup;
	int			warmupCount;

	//==========================

	int			itemPickup;
	int			itemPickupTime;
	int			itemPickupBlendTime;	// the pulse around the crosshair is timed seperately

	int			weaponSelectTime;
	int			weaponAnimation;
	int			weaponAnimationTime;

	// blend blobs
	float		damageTime;
	float		damageX, damageY, damageValue;

	// status bar head
	float		headYaw;
	float		headEndPitch;
	float		headEndYaw;
	int			headEndTime;
	float		headStartPitch;
	float		headStartYaw;
	int			headStartTime;

	// view movement
	float		v_dmg_time;
	float		v_dmg_pitch;
	float		v_dmg_roll;

	vec3_t		kick_angles;	// weapon kicks
	vec3_t		kick_origin;

	// temp working variables for player view
	float		bobfracsin;
	int			bobcycle;
	float		xyspeed;
	int     nextOrbitTime;

	//qboolean cameraMode;		// if rendering from a loaded camera


	// development tool
	refEntity_t		testModelEntity;
	char			testModelName[MAX_QPATH];
	qboolean		testGun;

	// OSP fields
	int				teamOverlayWidth;	// OSP: the team overlay's width in pixels (CG_OSPPrepareTeamOverlay); CG_AddToTeamChat / CG_OSPDrawTeamInfo leave it free with cg_drawTeamOverlay 2
	qboolean		teamOverlayChanged;	// CG_OSPDrawTeamOverlay rebuilds its CHS buffer when set (a "tinfo" arrived, a custom location was added, or an overlay setting changed), otherwise it replays it
	int				osp_unk_1e83c;		// OSP: set to 1 by CG_ConfigStringModified on score / team state changes; never read
	qboolean		customLocationsEnabled;	// CG_ParseTeamInfo, CG_OSPUpdateUserInfo
	int				numTeammates;		// valid team mates, written by the unreferenced CG_OSPCountTeammates; never read
	int				teammateNameLen;	// their longest name (at most 12), CG_OSPCountTeammates; never read
	int				osp_unk_1e84c;		// unknown, OSP field
	int				hmiActive;			// HMI menu open: CG_NextWeapon_f / CG_PrevWeapon_f scroll it, CG_OSPHUDRoutine draws HMI_Draw instead of the crosshair
	// OSP: the HMI menu state
	hmiMenu_t		*hmiMenu;			// the menu shown (NULL: HMI_Draw opens hmiMainMenu)
	hmiMenu_t		*hmiMenuStack[HMI_STACK_SIZE];	// menus to go back to (HMI_Push / HMI_Pop)
	int				hmiItemStack[HMI_STACK_SIZE];	// their selected items
	int				hmiDepth;			// entries on the stack
	int				hmiItem;			// the selected item (-1: none)
	int				hmiOpenTime;		// cg.time of \menu, for HMI_Draw's 1 s fade-in
	int				osp_unk_1e8c4;		// zeroed by CG_Init, never read
	int				osp_unk_1e8c8;		// unused
	qboolean		mvActive;			// OSP: multiview on (CG_DrawActiveFrame draws CG_OSPDrawMultiview instead of the scene; the OSP sound helpers play for the multiview's client)
	// OSP: multiview
	int				mvCursorTime;		// the cursor is drawn until then (cg.time + 5000 when it moves or the view list changes)
	int				mvCursorX;			// cursor position in 640x480 (from cg_pmove.cmd.angles[YAW])
	int				mvCursorY;			// (from cg_pmove.cmd.angles[PITCH])
	int				mvViewframeTime;	// next "viewframe" command allowed (cursor over another window)
	int				mvCurrent;			// window of the current view (0-based; "viewactive" server command)
	int				mvClientNums[32];	// client of each window ("viewlist" server command)
	int				osp_unk_1e964;		// zeroed by the multiview code; the OSP sound helpers save, zero and restore it around trap_S_StartSound (never read otherwise)
	int				osp_unk_1e968[32];		// unknown, OSP field
} cg_t;


// all of the model, shader, and sound references that are
// loaded at gamestate time are stored in cgMedia_t
// Other media that can be tied to clients, weapons, or items are
// stored in the clientInfo_t, itemInfo_t, weaponInfo_t, and powerupInfo_t
typedef struct {
	qhandle_t	charsetShader1;				// OSP, "gfx/2d/bigchars1"
	qhandle_t	charsetShader;
	qhandle_t	charsetProp;
	qhandle_t	charsetPropGlow;
	qhandle_t	charsetPropB;
	qhandle_t	whiteShader;

	qhandle_t	redCubeModel;
	qhandle_t	blueCubeModel;
	qhandle_t	redCubeIcon;
	qhandle_t	blueCubeIcon;
	qhandle_t	redFlagModel;
	qhandle_t	blueFlagModel;
	qhandle_t	neutralFlagModel;
	qhandle_t	redFlagShader[3];
	qhandle_t	blueFlagShader[3];
	qhandle_t	flagShader[4];

	qhandle_t	flagPoleModel;
	qhandle_t	flagFlapModel;

	qhandle_t	redFlagFlapSkin;
	qhandle_t	blueFlagFlapSkin;
	qhandle_t	neutralFlagFlapSkin;

	qhandle_t	redFlagBaseModel;
	qhandle_t	blueFlagBaseModel;
	qhandle_t	neutralFlagBaseModel;

	// OSP: id's eight Team Arena handles are outside MISSIONPACK here:
	// never registered or read
	qhandle_t	overloadBaseModel;
	qhandle_t	overloadTargetModel;
	qhandle_t	overloadLightsModel;
	qhandle_t	overloadEnergyModel;

	qhandle_t	harvesterModel;
	qhandle_t	harvesterRedSkin;
	qhandle_t	harvesterBlueSkin;
	qhandle_t	harvesterNeutralModel;

	qhandle_t	armorModelGreen;			// OSP armor_gre / armor_red
	qhandle_t	armorModel;
	qhandle_t	armorModelRed;
	qhandle_t	armorIconGreen;				// OSP iconr_green / iconr_red
	qhandle_t	armorIcon;
	qhandle_t	armorIconRed;

	qhandle_t	teamStatusBar;

	qhandle_t	deferShader;

	// gib explosions
	qhandle_t	gibAbdomen;
	qhandle_t	gibArm;
	qhandle_t	gibChest;
	qhandle_t	gibFist;
	qhandle_t	gibFoot;
	qhandle_t	gibForearm;
	qhandle_t	gibIntestine;
	qhandle_t	gibLeg;
	qhandle_t	gibSkull;
	qhandle_t	gibBrain;

	qhandle_t	smoke2;

	qhandle_t	machinegunBrassModel;
	qhandle_t	shotgunBrassModel;

	qhandle_t	railRingsShader;
	qhandle_t	railRingsShaderNoPicMip;	// OSP (cg_nomip) shaders
	qhandle_t	railCoreShader;
	qhandle_t	railCoreShaderNoPicMip;

	qhandle_t	lightningBoltShader;		// OSP "lightningBolt" (+NoPicMip)
	qhandle_t	lightningBoltShaderNoPicMip;
	qhandle_t	lightningShader;
	qhandle_t	lightningShaderNoPicMip;

	qhandle_t	friendShader;

	qhandle_t	balloonShader;
	qhandle_t	connectionShader;

	qhandle_t	selectShader;
	qhandle_t	viewBloodShader;
	qhandle_t	tracerShader;
	qhandle_t	crosshairShader[NUM_CROSSHAIRS];
	qhandle_t	crosshairShader45[NUM_CROSSHAIRS];	// OSP "gfx/2d/crosshair%c2"
	qhandle_t	lagometerShader;
	qhandle_t	backTileShader;
	qhandle_t	noammoShader;

	qhandle_t	smokePuffShader;
	qhandle_t	smokePuffNoPicMipShader;	// OSP
	qhandle_t	smokePuffRageProShader;
	qhandle_t	shotgunSmokePuffShader;
	qhandle_t	shotgunSmokePuffNoPicMipShader;
	qhandle_t	plasmaBallShader;
	qhandle_t	plasmaBallNoPicMipShader;
	qhandle_t	plasmaNewBallShader;
	qhandle_t	plasmaNewBallNoPicMipShader;
	qhandle_t	waterBubbleShader;
	qhandle_t	bloodTrailShader;
#ifdef MISSIONPACK
	qhandle_t	nailPuffShader;
	qhandle_t	blueProxMine;
#endif

	qhandle_t	numberShaders[11];

	qhandle_t	shadowMarkShader;

	qhandle_t	botSkillShaders[5];

	// wall mark shaders
	qhandle_t	wakeMarkShader;
	qhandle_t	bloodMarkShader;
	qhandle_t	bulletMarkShader;
	qhandle_t	bulletMarkNoPicMipShader;	// OSP
	qhandle_t	burnMarkShader;
	qhandle_t	burnMarkNoPicMipShader;
	qhandle_t	holeMarkShader;
	qhandle_t	holeMarkNoPicMipShader;
	qhandle_t	energyMarkShader;
	qhandle_t	energyMarkNoPicMipShader;
	qhandle_t	freezeShader;
	qhandle_t	freezeMarkShader;

	// powerup shaders
	qhandle_t	quadShader;
	qhandle_t	redQuadShader;
	qhandle_t	quadWeaponShader;
	qhandle_t	invisShader;
	qhandle_t	regenShader;
	qhandle_t	battleSuitShader;
	qhandle_t	battleWeaponShader;
	qhandle_t	hastePuffShader;
	qhandle_t	redKamikazeShader;
	qhandle_t	blueKamikazeShader;

	// weapon effect models
	qhandle_t	bulletFlashModel;
	qhandle_t	ringFlashModel;
	qhandle_t	dishFlashModel;
	qhandle_t	lightningExplosionModel;

	// weapon effect shaders
	qhandle_t	railExplosionShader;
	qhandle_t	railExplosionShaderNoPicMip;	// OSP
	qhandle_t	plasmaExplosionShader;
	qhandle_t	plasmaExplosionShaderNoPicMip;
	qhandle_t	plasmaNewExplosionShader;
	qhandle_t	plasmaNewExplosionShaderNoPicMip;
	qhandle_t	bulletExplosionShader;
	qhandle_t	bulletExplosionShaderNoPicMip;
	qhandle_t	rocketExplosionShader;
	qhandle_t	rocketExplosionShaderNoPicMip;
	qhandle_t	grenadeExplosionShader;
	qhandle_t	grenadeExplosionShaderNoPicMip;
	qhandle_t	bfgExplosionShader;
	qhandle_t	bfgExplosionShaderNoPicMip;
	qhandle_t	bloodExplosionShader;
	qhandle_t	bloodExplosionNoPicMipShader;

	// special effects models
	qhandle_t	teleportEffectModel;
	qhandle_t	teleportEffectShader;
#ifdef MISSIONPACK
	qhandle_t	kamikazeEffectModel;
	qhandle_t	kamikazeShockWave;
	qhandle_t	kamikazeHeadModel;
	qhandle_t	kamikazeHeadTrail;
	qhandle_t	guardPowerupModel;
	qhandle_t	scoutPowerupModel;
	qhandle_t	doublerPowerupModel;
	qhandle_t	ammoRegenPowerupModel;
	qhandle_t	invulnerabilityImpactModel;
	qhandle_t	invulnerabilityJuicedModel;
	qhandle_t	medkitUsageModel;
	qhandle_t	dustPuffShader;
	qhandle_t	heartShader;
#endif
	qhandle_t	invulnerabilityPowerupModel;

	// scoreboard headers
	qhandle_t	scoreboardName;
	qhandle_t	scoreboardPing;
	qhandle_t	scoreboardScore;
	qhandle_t	scoreboardTime;

	// medals shown during gameplay
	qhandle_t	medalImpressive;
	qhandle_t	medalExcellent;
	qhandle_t	medalGauntlet;
	qhandle_t	medalDefend;
	qhandle_t	medalAssist;
	qhandle_t	medalCapture;

	qhandle_t	blender180Shader;			// OSP "gfx/2d/blender_180.tga"

	// sounds
	sfxHandle_t	quadSound;
	sfxHandle_t	tracerSound;
	sfxHandle_t	selectSound;
	sfxHandle_t	useNothingSound;
	sfxHandle_t	wearOffSound;
	sfxHandle_t	footsteps[FOOTSTEP_TOTAL][4];
	sfxHandle_t	sfx_lghit1;
	sfxHandle_t	sfx_lghit2;
	sfxHandle_t	sfx_lghit3;
	sfxHandle_t	sfx_ric1;
	sfxHandle_t	sfx_ric2;
	sfxHandle_t	sfx_ric3;
	sfxHandle_t	sfx_railg;
	sfxHandle_t	sfx_rockexp;
	sfxHandle_t	sfx_plasmaexp;
#ifdef MISSIONPACK
	sfxHandle_t	sfx_proxexp;
	sfxHandle_t	sfx_nghit;
	sfxHandle_t	sfx_nghitflesh;
	sfxHandle_t	sfx_nghitmetal;
	sfxHandle_t	sfx_chghit;
	sfxHandle_t	sfx_chghitflesh;
	sfxHandle_t	sfx_chghitmetal;
	sfxHandle_t kamikazeExplodeSound;
	sfxHandle_t kamikazeImplodeSound;
	sfxHandle_t kamikazeFarSound;
	sfxHandle_t useInvulnerabilitySound;
	sfxHandle_t invulnerabilityImpactSound1;
	sfxHandle_t invulnerabilityImpactSound2;
	sfxHandle_t invulnerabilityImpactSound3;
	sfxHandle_t invulnerabilityJuicedSound;
	sfxHandle_t obeliskHitSound1;
	sfxHandle_t obeliskHitSound2;
	sfxHandle_t obeliskHitSound3;
	sfxHandle_t	obeliskRespawnSound;
	sfxHandle_t	winnerSound;
	sfxHandle_t	loserSound;
	sfxHandle_t	youSuckSound;
#endif
	sfxHandle_t	gibSound;
	sfxHandle_t	gibBounce1Sound;
	sfxHandle_t	gibBounce2Sound;
	sfxHandle_t	gibBounce3Sound;
	sfxHandle_t	teleInSound;
	sfxHandle_t	teleOutSound;
	sfxHandle_t	noAmmoSound;
	sfxHandle_t	respawnSound;
	sfxHandle_t talkSound;
	sfxHandle_t landSound;
	sfxHandle_t fallSound;
	sfxHandle_t jumpPadSound;

	sfxHandle_t oneMinuteSound;
	sfxHandle_t fiveMinuteSound;
	sfxHandle_t suddenDeathSound;

	sfxHandle_t threeFragSound;
	sfxHandle_t twoFragSound;
	sfxHandle_t oneFragSound;

	sfxHandle_t hitLowestSound;				// OSP hit sounds (hitlowest, hitlow, hithigh)
	sfxHandle_t hitLowSound;
	sfxHandle_t hitSound;
	sfxHandle_t hitHighSound;
	sfxHandle_t hitSoundHighArmor;
	sfxHandle_t hitSoundLowArmor;
	sfxHandle_t hitTeamSound;
	sfxHandle_t impressiveSound;
	sfxHandle_t excellentSound;
	sfxHandle_t deniedSound;
	sfxHandle_t humiliationSound;
	sfxHandle_t assistSound;
	sfxHandle_t defendSound;
	sfxHandle_t firstImpressiveSound;
	sfxHandle_t firstExcellentSound;
	sfxHandle_t firstHumiliationSound;

	sfxHandle_t takenLeadSound;
	sfxHandle_t tiedLeadSound;
	sfxHandle_t lostLeadSound;

	sfxHandle_t voteNow;
	sfxHandle_t votePassed;
	sfxHandle_t voteFailed;

	sfxHandle_t watrInSound;
	sfxHandle_t watrOutSound;
	sfxHandle_t watrUnSound;

	sfxHandle_t flightSound;
	sfxHandle_t medkitSound;

	sfxHandle_t weaponHoverSound;

	// teamplay sounds
	sfxHandle_t captureAwardSound;
	sfxHandle_t redScoredSound;
	sfxHandle_t blueScoredSound;
	sfxHandle_t redLeadsSound;
	sfxHandle_t blueLeadsSound;
	sfxHandle_t teamsTiedSound;

	sfxHandle_t	captureYourTeamSound;
	sfxHandle_t	captureOpponentSound;
	sfxHandle_t	returnYourTeamSound;
	sfxHandle_t	returnOpponentSound;
	sfxHandle_t	takenYourTeamSound;
	sfxHandle_t	takenOpponentSound;

	sfxHandle_t redFlagReturnedSound;
	sfxHandle_t blueFlagReturnedSound;
	sfxHandle_t neutralFlagReturnedSound;
	sfxHandle_t	enemyTookYourFlagSound;
	sfxHandle_t	enemyTookTheFlagSound;
	sfxHandle_t yourTeamTookEnemyFlagSound;
	sfxHandle_t yourTeamTookTheFlagSound;
	sfxHandle_t	youHaveFlagSound;
	sfxHandle_t yourBaseIsUnderAttackSound;
	sfxHandle_t holyShitSound;

	// tournament sounds
	sfxHandle_t	count3Sound;
	sfxHandle_t	count2Sound;
	sfxHandle_t	count1Sound;
	sfxHandle_t	countFightSound;
	sfxHandle_t	countPrepareSound;

#ifdef MISSIONPACK
	// new stuff
	qhandle_t patrolShader;
	qhandle_t assaultShader;
	qhandle_t campShader;
	qhandle_t followShader;
	qhandle_t defendShader;
	qhandle_t teamLeaderShader;
	qhandle_t retrieveShader;
	qhandle_t escortShader;
	qhandle_t flagShaders[3];
	sfxHandle_t	countPrepareTeamSound;

	sfxHandle_t ammoregenSound;
	sfxHandle_t doublerSound;
	sfxHandle_t guardSound;
	sfxHandle_t scoutSound;
#endif
	qhandle_t cursor;
	qhandle_t selectCursor;
	qhandle_t sizeCursor;

	sfxHandle_t	regenSound;
	sfxHandle_t	protectSound;
	sfxHandle_t	n_healthSound;
	sfxHandle_t	hgrenb1aSound;
	sfxHandle_t	hgrenb2aSound;
	sfxHandle_t	wstbimplSound;
	sfxHandle_t	wstbimpmSound;
	sfxHandle_t	wstbimpdSound;
	sfxHandle_t	wstbactvSound;

	// OSP media after id's
	qhandle_t	customShader[MAX_OSP_CUSTOM_GFX];			// "gfx/2d/custom%d"
	qhandle_t	grenadeCPMAModel;			// "models/ammo/grenade1.md3" / "grenadeCPMA.md3"
	qhandle_t	grenadeCPMANoPicMipShader;	// "grenadeCPMA_NPM"
} cgMedia_t;


// OSP sliding text window (CG_OSPDrawLeftSlidingWindow); ospPrintContext[16] is in cg_osp_draw.c
typedef struct {
	int		charHeight;
	int		charWidth;
	int		maxStringLen;
	int		hideBeforeRealtime;
	int		numberOfStrings;
	int		hideBeforeCGTime;
	int		timeAppearance;
	int		timeShow;
	int		timeHiding;
	int		showFromCGTime;
	float	windowPosY;			// y of the window's bottom line; the callers pass float constants
	char	string[MAX_OSP_SLIDING_LINES][MAX_OSP_SLIDING_CHARS];
	vec4_t	borderColor;
	vec4_t	bodyColor;
} ospSlidingPrintContext_t;

extern ospSlidingPrintContext_t	ospPrintContext[MAX_OSP_SLIDING_WINDOWS];	// cg_osp_draw.c

// OSP decal (44 bytes, the stride in the decal commands): enabled / size / shader are set
// by the decal commands (decalenable, decalinc 10..800, decalgfxnext); the decal
// configstring and decaldump format is "%i %i %f ..." = shader, size, angle, origin, dir.
// rotation is a float (decalrotclock / decalrotcounter turn it by 0.5, CG_OSPAddDecals adds
// it to the surface's angle); dir is the surface normal and origin the trace end point of
// CG_OSPAddDecals' decaledit trace, passed to CG_ImpactMark( shader, origin, dir, angle, ... )
typedef struct osp_decal_s {
	int		enabled;
	int		size;
	int		shader;			// index into cgs.decalOSPShader, -1 in decal 0: decals disabled
	float	rotation;		// decaledit rotation (decalrotclock / decalrotcounter)
	float	angle;			// mark orientation (rotation + surface angle)
	vec3_t	dir;			// surface normal
	vec3_t	origin;			// position on the surface
} osp_decal_t;

// The client game static (cgs) structure hold everything
// loaded or calculated from the gamestate.  It will NOT
// be cleared when a tournement restart is done, allowing
// all clients to begin playing instantly
typedef struct {
	gameState_t		gameState;			// gamestate from server
	glconfig_t		glconfig;			// rendering configuration
	float			screenXScale;		// derived from glconfig
	float			screenYScale;
	float			screenXBias;

	int				serverCommandSequence;	// reliable command stream counter
	int				processedSnapshotNum;// the number of snapshots cgame has requested

	qboolean		localServer;		// detected on startup by checking sv_running

	// parsed from serverinfo
	gametype_t		gametype;
	int				dmflags;
	int				teamflags;
	int				fraglimit;
	int				capturelimit;
	int				timelimit;
	int				maxclients;
	char			mapname[MAX_QPATH];
	char			redTeam[MAX_QPATH];
	char			blueTeam[MAX_QPATH];

	int				voteTime;
	int				voteYes;
	int				voteNo;
	qboolean		voteModified;			// beep whenever changed
	char			voteString[MAX_STRING_TOKENS];

	// (OSP: no team voting in OSP: teamVoteTime .. teamVoteString removed, as in its qagame)

	int				levelStartTime;

	int				scores1, scores2;		// from configstrings
	int				redflag, blueflag;		// flag status from configstrings
	int				flagStatus;

	qboolean  newHud;

	//
	// locally derived information from gamestate
	//
	qhandle_t		gameModels[MAX_MODELS];
	sfxHandle_t		gameSounds[MAX_SOUNDS];

	int				numInlineModels;
	qhandle_t		inlineDrawModel[MAX_MODELS];
	vec3_t			inlineModelMidpoints[MAX_MODELS];

	clientInfo_t	clientinfo[MAX_CLIENTS];

	// teamchat width is *3 because of embedded color codes
	char			teamChatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_WIDTH*3+1];
	int				teamChatMsgTimes[TEAMCHAT_HEIGHT];
	int				teamChatPos;
	int				teamLastChatPos;

	int cursorX;
	int cursorY;
	qboolean eventHandling;
	qboolean mouseCaptured;
	qboolean sizingHud;
	void *capturedItem;
	qhandle_t activeCursor;

	// orders
	int currentOrder;
	qboolean orderPending;
	int orderTime;
	int currentVoiceClient;
	int acceptOrderTime;
	int acceptTask;
	int acceptLeader;
	char acceptVoice[MAX_NAME_LENGTH];

	// media
	cgMedia_t		media;

	// OSP fields after media
	qhandle_t		configString_0x368;	// unknown, OSP field
	fileHandle_t	logFileHandle;
	int				unknown_variable_329;	// unknown, OSP field
	int				server_mode;
	// OSP: a "pm" skin flag and the enemy rail colour
	int				pmSkinColors;			// set when a "pm" skin takes the cg_enemyColors / color1 colours (CG_NewClientInfo; read by CG_Player, cleared by CG_Init)
	vec3_t			enemyRailColor;			// first colour of cg_enemyColors (CG_UpdateCvars, CG_NewClientInfo)
	int				custom_client_2;
	int				allow_pmove;
	int				serverConfigMinimumTimenudge;
	int				serverConfigMaximumTimenudge;
	int				serverConfigMinimumMaxpackets;
	int				serverConfigMaximumMaxpackets;
	int				nextCvarsUpdateTime;
	int				cvarsUpdateDelay;
	qboolean		isOSPv1;
	int				stats_mask;
	int				stats_shift;
	qhandle_t		quadSoundTime;
	int				numberOfStringsMotd;
	char			motd[8][64];
	int				custom_gfx_number;
	int				custom_gfx[MAX_OSP_CUSTOM_GFX][8];
	qboolean		clanBaseTeamDM;
	int				gameTypeFreeze;
	qhandle_t		ospFlagStatus;		// OSP (cgs has id's flagStatus too)
	qhandle_t		ospRedflag;
	qhandle_t		ospBlueflag;
	int				numDecals;				// decals in use (CG_Init, decaladd)
	qboolean		decalEditing;			// decaledit mode
	int				decalSelected;			// decal being edited (decalnext / decalprev)
	int				decalOSPShaderCopyCount;
	osp_decal_t		decals[MAX_OSP_DECALS];				// 44 bytes each
	qhandle_t		blender180ShaderCopy[17];
	qhandle_t		decalOSPShader[17];
	qhandle_t		unknown_variable_999;	// unknown, OSP field
	int				osp_teamcount1;
	int				osp_teamcount2;
	qhandle_t		unknown_variable_1002;	// unknown, OSP field
	qhandle_t		pingPrintTime;
	float			pingMs;
	float			pingCount;		// OSP: float (CG_ReadNextSnapshot adds 1.0)
	char			pingString[16];
	qhandle_t		pingStringPositionX;
} cgs_t;

//==============================================================================

extern	cgs_t			cgs;
extern	cg_t			cg;
extern	centity_t		cg_entities[MAX_GENTITIES];
extern	weaponInfo_t	cg_weapons[MAX_WEAPONS];
extern	itemInfo_t		cg_items[MAX_ITEMS];
extern	markPoly_t		cg_markPolys[MAX_MARK_POLYS];

// OSP: the vmCvar_t of cg_main.c in OSP's declaration order. q3lcc
// places a file's bss in reverse order of first declaration, and these externs are the
// first declarations, so this order sets the cvars' layout in the QVM.
// cg_main.c defines five more below them without an extern.
extern	vmCvar_t		osp_client;
extern	vmCvar_t		cg_centertime;
extern	vmCvar_t		cg_runpitch;
extern	vmCvar_t		cg_runroll;
extern	vmCvar_t		cg_bobup;
extern	vmCvar_t		cg_bobpitch;
extern	vmCvar_t		cg_bobroll;
extern	vmCvar_t		cg_swingSpeed;
extern	vmCvar_t		cg_shadows;
extern	vmCvar_t		cg_gibs;
extern	vmCvar_t		cg_drawTimer;
extern	vmCvar_t		cg_drawFPS;
extern	vmCvar_t		cg_drawPing;
extern	vmCvar_t		cg_drawSnapshot;
extern	vmCvar_t		cg_draw3dIcons;
extern	vmCvar_t		cg_drawIcons;
extern	vmCvar_t		cg_drawAmmoWarning;
extern	vmCvar_t		cg_drawCrosshair;
extern	vmCvar_t		cg_drawCrosshairNames;
extern	vmCvar_t		cg_drawRewards;
extern	vmCvar_t		cg_drawTeamOverlay;
extern	vmCvar_t		cg_teamOverlayUserinfo;
extern	vmCvar_t		cg_crosshairX;
extern	vmCvar_t		cg_crosshairY;
extern	vmCvar_t		cg_crosshairSize;
extern	vmCvar_t		cg_crosshairHealth;
extern	vmCvar_t		cg_drawStatus;
extern	vmCvar_t		cg_draw2D;
extern	vmCvar_t		cg_animSpeed;
extern	vmCvar_t		cg_debugAnim;
extern	vmCvar_t		cg_debugPosition;
extern	vmCvar_t		cg_debugEvents;
extern	vmCvar_t		cg_railTrailTime;
extern	vmCvar_t		cg_errorDecay;
extern	vmCvar_t		cg_nopredict;
extern	vmCvar_t		cg_noPlayerAnims;
extern	vmCvar_t		cg_showmiss;
extern	vmCvar_t		cg_footsteps;
extern	vmCvar_t		cg_addMarks;
extern	vmCvar_t		cg_brassTime;
extern	vmCvar_t		cg_gun_frame;
extern	vmCvar_t		cg_gun_x;
extern	vmCvar_t		cg_gun_y;
extern	vmCvar_t		cg_gun_z;
extern	vmCvar_t		cg_drawGun;
extern	vmCvar_t		cg_viewsize;
extern	vmCvar_t		cg_tracerChance;
extern	vmCvar_t		cg_tracerWidth;
extern	vmCvar_t		cg_tracerLength;
extern	vmCvar_t		cg_autoswitch;
extern	vmCvar_t		cg_ignore;
extern	vmCvar_t		cg_simpleItems;
extern	vmCvar_t		cg_fov;
extern	vmCvar_t		cg_zoomFov;
extern	vmCvar_t		cg_thirdPersonRange;
extern	vmCvar_t		cg_thirdPersonAngle;
extern	vmCvar_t		cg_thirdPerson;
extern	vmCvar_t		cg_stereoSeparation;
extern	vmCvar_t		cg_lagometer;
extern	vmCvar_t		cg_drawAttacker;
extern	vmCvar_t		cg_synchronousClients;
extern	vmCvar_t		cg_teamChatTime;
extern	vmCvar_t		cg_teamChatHeight;
extern	vmCvar_t		cg_stats;
extern	vmCvar_t		cg_forceModel;
extern	vmCvar_t		cg_buildScript;
extern	vmCvar_t		cg_paused;
extern	vmCvar_t		cg_blood;
extern	vmCvar_t		cg_predictItems;
extern	vmCvar_t		cg_deferPlayers;
extern	vmCvar_t		cg_drawFriend;
extern	vmCvar_t		cg_teamChatsOnly;
extern	vmCvar_t		cg_noVoiceChats;
extern	vmCvar_t		cg_noVoiceText;
extern	vmCvar_t		cg_scorePlum;
extern	vmCvar_t		cg_smoothClients;
extern	vmCvar_t		pmove_fixed;
extern	vmCvar_t		pmove_msec;
extern	vmCvar_t		cg_cameraOrbit;
extern	vmCvar_t		cg_cameraOrbitDelay;
extern	vmCvar_t		cg_timescaleFadeEnd;
extern	vmCvar_t		cg_timescaleFadeSpeed;
extern	vmCvar_t		cg_timescale;
extern	vmCvar_t		cg_cameraMode;
extern	vmCvar_t		cg_smallFont;
extern	vmCvar_t		cg_bigFont;
extern	vmCvar_t		cg_noTaunt;
extern	vmCvar_t		cg_noProjectileTrail;
extern	vmCvar_t		cg_oldRail;
extern	vmCvar_t		cg_oldPlasma;
extern	vmCvar_t		cg_oldRocket;
extern	vmCvar_t		cg_enableBreath;
extern	vmCvar_t		cg_altGrenades;
extern	vmCvar_t		cg_altLightning;
extern	vmCvar_t		cg_altPlasma;
extern	vmCvar_t		cg_ammoCheck;
extern	vmCvar_t		cg_autoAction;
extern	vmCvar_t		cg_crosshairPulse;
extern	vmCvar_t		cg_Customloc;
extern	vmCvar_t		cg_damageDraw;
extern	vmCvar_t		cg_damageKick;
extern	vmCvar_t		cg_deadBodyFilter;
extern	vmCvar_t		cg_enableOSPHUD;
extern	vmCvar_t		cg_enemyColors;
extern	vmCvar_t		cg_enemyModel;
extern	vmCvar_t		cg_execVstr;
extern	vmCvar_t		cg_fallKick;
extern	vmCvar_t		cg_float;
extern	vmCvar_t		cg_followkiller;
extern	vmCvar_t		cg_followpowerup;
extern	vmCvar_t		cg_followviewcam;
extern	vmCvar_t		cg_forceColors;
extern	vmCvar_t		cg_lightningImpact;
extern	vmCvar_t		cg_MaxlocationWidth;
extern	vmCvar_t		cg_muzzleFlash;
extern	vmCvar_t		cg_nochatbeep;
extern	vmCvar_t		cg_NoMOTDDisplayDuringDemo;
extern	vmCvar_t		cg_nomip;
extern	vmCvar_t		cg_oldCTFSounds;
extern	vmCvar_t		cg_oversampleMouse;
extern	vmCvar_t		cg_q3compScoreboard;
extern	vmCvar_t		cg_scoreTransparency;
extern	vmCvar_t		cg_showPlayerLean;
extern	vmCvar_t		cg_smoke_sg;
extern	vmCvar_t		cg_smokegrowth_gl;
extern	vmCvar_t		cg_smokegrowth_rl;
extern	vmCvar_t		cg_smokeradius_gl;
extern	vmCvar_t		cg_smokeradius_rl;
extern	vmCvar_t		cg_statScrollTime;
extern	vmCvar_t		cg_swapSkins;
extern	vmCvar_t		cg_trueLightning;
extern	vmCvar_t		cg_teamRails;
extern	vmCvar_t		cg_useScreenShotJPEG;
extern	vmCvar_t		ch_3waveFont;
extern	vmCvar_t		ch_ColorLocations;
extern	vmCvar_t		ch_CrosshairColor;
extern	vmCvar_t		cf_CrosshairNames;
extern	vmCvar_t		ch_CrosshairNamesLeft;
extern	vmCvar_t		ch_drawFlagNames;
extern	vmCvar_t		ch_drawHealthMeters;
extern	vmCvar_t		ch_drawSpeed;
extern	vmCvar_t		ch_TeamBackground;
extern	vmCvar_t		ch_FilterLocationsTeamchat;
extern	vmCvar_t		cf_Following;
extern	vmCvar_t		cf_Fragmsg;
extern	vmCvar_t		ch_InverseTeamChat;
extern	vmCvar_t		ch_Obituaries;
extern	vmCvar_t		ch_Pickupitem;
extern	vmCvar_t		cf_Pickupstring;
extern	vmCvar_t		ch_recordMessage;
extern	vmCvar_t		cf_Scores;
extern	vmCvar_t		cf_Statusbar;
extern	vmCvar_t		ch_StatusbarFormat;
extern	vmCvar_t		ch_TeamchatOnly;
extern	vmCvar_t		cf_Teamchat;
extern	vmCvar_t		ch_TeamCrosshairHealth;
extern	vmCvar_t		cf_TeamCrosshairHealth;
extern	vmCvar_t		ch_Teamoverlay;
extern	vmCvar_t		cf_Teamoverlay;
extern	vmCvar_t		cf_Vote;
extern	vmCvar_t		cf_WeaponSwitch;
extern	vmCvar_t		cf_WeaponName;
extern	vmCvar_t		ch_Weaponswitch;
extern	vmCvar_t		cf_AmmoStatusbar;
extern	vmCvar_t		s_ambient;
extern	vmCvar_t		cl_maxpackets;
extern	vmCvar_t		r_shownormals;
extern	vmCvar_t		r_showtris;
extern	vmCvar_t		snaps;
extern	vmCvar_t		cl_timenudge;
extern	vmCvar_t		r_znear;
extern	vmCvar_t		com_maxfps;
extern	vmCvar_t		m_pitch;
extern	vmCvar_t		r_fullbright;
extern	vmCvar_t		r_lodCurveError;
extern	vmCvar_t		r_subdivisions;

// OSP cvars of cg_osp_cpmhud.c (same rule)
extern	vmCvar_t		ch_crosshairTeamInfo;
extern	vmCvar_t		ch_graphs;
extern	vmCvar_t		ch_statusbarStyle;
extern	vmCvar_t		ch_teamBackground;
extern	vmCvar_t		ch_weaponList;
extern	vmCvar_t		ch_weaponListDrawAll;
extern	vmCvar_t		ch_drawPickup;
extern	vmCvar_t		ch_fragMessage;
extern	vmCvar_t		ch_weaponListBackground;
extern	vmCvar_t		ch_weaponListFlash;
#ifdef MISSIONPACK
extern	vmCvar_t		cg_redTeamName;
extern	vmCvar_t		cg_blueTeamName;
extern	vmCvar_t		cg_currentSelectedPlayer;
extern	vmCvar_t		cg_currentSelectedPlayerName;
extern	vmCvar_t		cg_singlePlayer;
extern	vmCvar_t		cg_enableDust;
extern	vmCvar_t		cg_singlePlayerActive;
extern  vmCvar_t		cg_recordSPDemo;
extern  vmCvar_t		cg_recordSPDemoName;
extern	vmCvar_t		cg_obeliskRespawnDelay;
#endif

//
// cg_main.c
//
const char *CG_ConfigString( int index );
const char *CG_Argv( int arg );

void QDECL CG_Printf( const char *msg, ... );
void QDECL CG_Error( const char *msg, ... );

void CG_StartMusic( void );

void CG_UpdateCvars( void );

int CG_CrosshairPlayer( void );
int CG_LastAttacker( void );
void CG_LoadMenus(const char *menuFile);
void CG_KeyEvent(int key, qboolean down);
void CG_MouseEvent(int x, int y);
void CG_EventHandling(int type);
void CG_RankRunFrame( void );
void CG_SetScoreSelection(void *menu);
score_t *CG_GetSelectedScore();
void CG_BuildSpectatorString();


//
// cg_view.c
//
void CG_TestModel_f (void);
void CG_TestGun_f (void);
void CG_TestModelNextFrame_f (void);
void CG_TestModelPrevFrame_f (void);
void CG_TestModelNextSkin_f (void);
void CG_TestModelPrevSkin_f (void);
void CG_ZoomDown_f( void );
void CG_ZoomUp_f( void );
void CG_AddBufferedSound( sfxHandle_t sfx);

void CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback );


//
// cg_drawtools.c
//
void CG_AdjustFrom640( float *x, float *y, float *w, float *h );
void CG_FillRect( float x, float y, float width, float height, const float *color );
void CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void CG_DrawString( float x, float y, const char *string, 
				   float charWidth, float charHeight, const float *modulate );


void CG_DrawStringExt( int x, int y, const char *string, const float *setColor, 
		qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars );
void CG_DrawBigString( int x, int y, const char *s, float alpha );
void CG_DrawBigStringColor( int x, int y, const char *s, vec4_t color );
void CG_DrawSmallString( int x, int y, const char *s, float alpha );
void CG_DrawSmallStringColor( int x, int y, const char *s, vec4_t color );

int CG_DrawStrlen( const char *str );

float	*CG_FadeColor( int startMsec, int totalMsec );
float *CG_TeamColor( int team );
void CG_TileClear( void );
void CG_ColorForHealth( vec4_t hcolor );
void CG_GetColorForHealth( int health, int armor, vec4_t hcolor );

void UI_DrawProportionalString( int x, int y, const char* str, int style, vec4_t color );
void CG_DrawRect( float x, float y, float width, float height, float size, const float *color );
void CG_DrawSides(float x, float y, float w, float h, float size);
void CG_DrawTopBottom(float x, float y, float w, float h, float size);


//
// cg_draw.c, cg_newDraw.c
//
extern	int sortedTeamPlayers[TEAM_MAXOVERLAY];
extern	int	numSortedTeamPlayers;
extern	int drawTeamOverlayModificationCount;
extern  char systemChat[256];
extern  char teamChat1[256];
extern  char teamChat2[256];

void CG_AddLagometerFrameInfo( void );
void CG_AddLagometerSnapshotInfo( snapshot_t *snap );
void CG_CenterPrint( const char *str, int y, int charWidth );
void CG_DrawHead( float x, float y, float w, float h, int clientNum, vec3_t headAngles );
void CG_DrawActive( stereoFrame_t stereoView );
void CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D );
void CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team );
void CG_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle);
void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style);
int CG_Text_Width(const char *text, float scale, int limit);
int CG_Text_Height(const char *text, float scale, int limit);
void CG_SelectPrevPlayer();
void CG_SelectNextPlayer();
float CG_GetValue(int ownerDraw);
qboolean CG_OwnerDrawVisible(int flags);
void CG_RunMenuScript(char **args);
void CG_ShowResponseHead();
void CG_SetPrintString(int type, const char *p);
void CG_InitTeamChat();
void CG_GetTeamColor(vec4_t *color);
const char *CG_GetGameStatusText();
const char *CG_GetKillerText();
void CG_Draw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles );
void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader);
void CG_CheckOrderPending();
const char *CG_GameTypeString();
qboolean CG_YourTeamHasFlag();
qboolean CG_OtherTeamHasFlag();
qhandle_t CG_StatusHandle(int task);



//
// cg_player.c
//
void CG_Player( centity_t *cent );
void CG_ResetPlayerEntity( centity_t *cent );
void CG_AddRefEntityWithPowerups( refEntity_t *ent, entityState_t *state, int team );
void CG_NewClientInfo( int clientNum );
sfxHandle_t	CG_CustomSound( int clientNum, const char *soundName );

//
// cg_predict.c
//
void CG_BuildSolidList( void );
int	CG_PointContents( const vec3_t point, int passEntityNum );
void CG_Trace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, 
					 int skipNumber, int mask );
void CG_PredictPlayerState( void );
void CG_LoadDeferredPlayers( void );


//
// cg_events.c
//
void CG_CheckEvents( centity_t *cent );
const char	*CG_PlaceString( int rank );
void CG_EntityEvent( centity_t *cent, vec3_t position );
void CG_PainEvent( centity_t *cent, int health );


//
// cg_ents.c
//
void CG_SetEntitySoundPosition( centity_t *cent );
void CG_AddPacketEntities( void );
void CG_Beam( centity_t *cent );
void CG_AdjustPositionForMover( const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out );

void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent, 
							qhandle_t parentModel, char *tagName );
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent, 
							qhandle_t parentModel, char *tagName );



//
// cg_weapons.c
//
void CG_NextWeapon_f( void );
void CG_PrevWeapon_f( void );
void CG_Weapon_f( void );

void CG_RegisterWeapon( int weaponNum );
void CG_RegisterItemVisuals( int itemNum );

void CG_FireWeapon( centity_t *cent );
void CG_MissileHitWall( int weapon, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType );
void CG_MissileHitPlayer( int weapon, vec3_t origin, vec3_t dir, int entityNum );
void CG_ShotgunFire( entityState_t *es );
void CG_Bullet( vec3_t origin, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum );

void CG_RailTrail( clientInfo_t *ci, vec3_t start, vec3_t end );
void CG_GrappleTrail( centity_t *ent, const weaponInfo_t *wi );
void CG_AddViewWeapon (playerState_t *ps);
void CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent, int team );
void CG_DrawWeaponSelect( void );

void CG_OutOfAmmoChange( void );	// should this be in pmove?

//
// cg_marks.c
//
void	CG_InitMarkPolys( void );
void	CG_AddMarks( void );
void	CG_ImpactMark( qhandle_t markShader, 
				    const vec3_t origin, const vec3_t dir, 
					float orientation, 
				    float r, float g, float b, float a, 
					qboolean alphaFade, 
					float radius, qboolean temporary, qboolean force );	// OSP's force

//
// cg_localents.c
//
void	CG_InitLocalEntities( void );
localEntity_t	*CG_AllocLocalEntity( void );
void	CG_AddLocalEntities( void );

//
// cg_effects.c
//
localEntity_t *CG_SmokePuff( const vec3_t p, 
				   const vec3_t vel, 
				   float radius,
				   float r, float g, float b, float a,
				   float duration,
				   int startTime,
				   int fadeInTime,
				   int leFlags,
				   qhandle_t hShader );
void CG_BubbleTrail( vec3_t start, vec3_t end, float spacing );
void CG_SpawnEffect( vec3_t org );
#ifdef MISSIONPACK
void CG_KamikazeEffect( vec3_t org );
void CG_ObeliskExplode( vec3_t org, int entityNum );
void CG_ObeliskPain( vec3_t org );
void CG_InvulnerabilityImpact( vec3_t org, vec3_t angles );
void CG_InvulnerabilityJuiced( vec3_t org );
void CG_LightningBoltBeam( vec3_t start, vec3_t end );
#endif
void CG_ScorePlum( int client, vec3_t org, int score );

void CG_GibPlayer( vec3_t playerOrigin );
void CG_BigExplode( vec3_t playerOrigin );

void CG_Bleed( vec3_t origin, int entityNum );

localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
								qhandle_t hModel, qhandle_t shader, int msec,
								qboolean isSprite );

//
// cg_snapshot.c
//
void CG_ProcessSnapshots( void );

//
// cg_info.c
//
void CG_LoadingString( const char *s );
void CG_LoadingItem( int itemNum );
void CG_LoadingClient( int clientNum );
void CG_DrawInformation( void );

//
// cg_scoreboard.c
//
qboolean CG_DrawOldScoreboard( void );
void CG_DrawOldTourneyScoreboard( void );

//
// cg_consolecmds.c
//
qboolean CG_ConsoleCommand( void );
void CG_InitConsoleCommands( void );

//
// cg_servercmds.c
//
void CG_ExecuteNewServerCommands( int latestSequence );
void CG_ParseServerinfo( void );
void CG_SetConfigValues( void );
void CG_LoadVoiceChats( void );
void CG_ShaderStateChanged(void);
void CG_VoiceChatLocal( int mode, qboolean voiceOnly, int clientNum, int color, const char *cmd );
void CG_PlayBufferedVoiceChats( void );

//
// cg_playerstate.c
//
void CG_Respawn( void );
void CG_TransitionPlayerState( playerState_t *ps, playerState_t *ops );
void CG_CheckChangedPredictableEvents( playerState_t *ps );


//===============================================

//
// system traps
// These functions are how the cgame communicates with the main game system
//

// print message on the local console
void		trap_Print( const char *fmt );

// abort the game
void		trap_Error( const char *fmt );

// milliseconds should only be used for performance tuning, never
// for anything game related.  Get time from the CG_DrawActiveFrame parameter
int			trap_Milliseconds( void );

// console variable interaction
void		trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void		trap_Cvar_Update( vmCvar_t *vmCvar );
void		trap_Cvar_Set( const char *var_name, const char *value );
void		trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

// ServerCommand and ConsoleCommand parameter access
int			trap_Argc( void );
void		trap_Argv( int n, char *buffer, int bufferLength );
void		trap_Args( char *buffer, int bufferLength );

// filesystem access
// returns length of file
int			trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void		trap_FS_Read( void *buffer, int len, fileHandle_t f );
void		trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void		trap_FS_FCloseFile( fileHandle_t f );
int			trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t

// add commands to the local console as if they were typed in
// for map changing, etc.  The command is not executed immediately,
// but will be executed in order the next time console commands
// are processed
void		trap_SendConsoleCommand( const char *text );

// register a command name so the console can perform command completion.
// FIXME: replace this with a normal console command "defineCommand"?
void		trap_AddCommand( const char *cmdName );

// send a string to the server over the network
void		trap_SendClientCommand( const char *s );

// force a screen update, only used during gamestate load
void		trap_UpdateScreen( void );

// model collision
void		trap_CM_LoadMap( const char *mapname );
int			trap_CM_NumInlineModels( void );
clipHandle_t trap_CM_InlineModel( int index );		// 0 = world, 1+ = bmodels
clipHandle_t trap_CM_TempBoxModel( const vec3_t mins, const vec3_t maxs );
int			trap_CM_PointContents( const vec3_t p, clipHandle_t model );
int			trap_CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );
void		trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
					  const vec3_t mins, const vec3_t maxs,
					  clipHandle_t model, int brushmask );
void		trap_CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
					  const vec3_t mins, const vec3_t maxs,
					  clipHandle_t model, int brushmask,
					  const vec3_t origin, const vec3_t angles );

// Returns the projection of a polygon onto the solid brushes in the world
int			trap_CM_MarkFragments( int numPoints, const vec3_t *points, 
			const vec3_t projection,
			int maxPoints, vec3_t pointBuffer,
			int maxFragments, markFragment_t *fragmentBuffer );

// normal sounds will have their volume dynamically changed as their entity
// moves and the listener moves
void		trap_S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx );
void		trap_S_StopLoopingSound(int entnum);

// a local sound is always played full volume
void		trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
void		trap_S_ClearLoopingSounds( qboolean killall );
void		trap_S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void		trap_S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void		trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin );

// respatialize recalculates the volumes of sound as they should be heard by the
// given entityNum and position
void		trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed );		// returns buzz if not found
void		trap_S_StartBackgroundTrack( const char *intro, const char *loop );	// empty name stops music
void	trap_S_StopBackgroundTrack( void );


void		trap_R_LoadWorldMap( const char *mapname );

// all media should be registered during level startup to prevent
// hitches during gameplay
qhandle_t	trap_R_RegisterModel( const char *name );			// returns rgb axis if not found
qhandle_t	trap_R_RegisterSkin( const char *name );			// returns all white if not found
qhandle_t	trap_R_RegisterShader( const char *name );			// returns all white if not found
qhandle_t	trap_R_RegisterShaderNoMip( const char *name );			// returns all white if not found

// a scene is built up by calls to R_ClearScene and the various R_Add functions.
// Nothing is drawn until R_RenderScene is called.
void		trap_R_ClearScene( void );
void		trap_R_AddRefEntityToScene( const refEntity_t *re );

// polys are intended for simple wall marks, not really for doing
// significant construction
void		trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts );
void		trap_R_AddPolysToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int numPolys );
void		trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
int			trap_R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
void		trap_R_RenderScene( const refdef_t *fd );
void		trap_R_SetColor( const float *rgba );	// NULL = 1,1,1,1
void		trap_R_DrawStretchPic( float x, float y, float w, float h, 
			float s1, float t1, float s2, float t2, qhandle_t hShader );
void		trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
int			trap_R_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, 
					   float frac, const char *tagName );
void		trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset );

// The glconfig_t will not change during the life of a cgame.
// If it needs to change, the entire cgame will be restarted, because
// all the qhandle_t are then invalid.
void		trap_GetGlconfig( glconfig_t *glconfig );

// the gamestate should be grabbed at startup, and whenever a
// configstring changes
void		trap_GetGameState( gameState_t *gamestate );

// cgame will poll each frame to see if a newer snapshot has arrived
// that it is interested in.  The time is returned seperately so that
// snapshot latency can be calculated.
void		trap_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime );

// a snapshot get can fail if the snapshot (or the entties it holds) is so
// old that it has fallen out of the client system queue
qboolean	trap_GetSnapshot( int snapshotNumber, snapshot_t *snapshot );

// retrieve a text command from the server stream
// the current snapshot will hold the number of the most recent command
// qfalse can be returned if the client system handled the command
// argc() / argv() can be used to examine the parameters of the command
qboolean	trap_GetServerCommand( int serverCommandNumber );

// returns the most recent command number that can be passed to GetUserCmd
// this will always be at least one higher than the number in the current
// snapshot, and it may be quite a few higher if it is a fast computer on
// a lagged connection
int			trap_GetCurrentCmdNumber( void );	

qboolean	trap_GetUserCmd( int cmdNumber, usercmd_t *ucmd );

// used for the weapon select and zoom
void		trap_SetUserCmdValue( int stateValue, float sensitivityScale );

// aids for VM testing
void		testPrintInt( char *string, int i );
void		testPrintFloat( char *string, float f );

int			trap_MemoryRemaining( void );
void		trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
qboolean	trap_Key_IsDown( int keynum );
int			trap_Key_GetCatcher( void );
void		trap_Key_SetCatcher( int catcher );
int			trap_Key_GetKey( const char *binding );


typedef enum {
  SYSTEM_PRINT,
  CHAT_PRINT,
  TEAMCHAT_PRINT
} q3print_t; // bk001201 - warning: useless keyword or type name in empty declaration


int trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status trap_CIN_StopCinematic(int handle);
e_status trap_CIN_RunCinematic (int handle);
void trap_CIN_DrawCinematic (int handle);
void trap_CIN_SetExtents (int handle, int x, int y, int w, int h);

void trap_SnapVector( float *v );

qboolean	trap_loadCamera(const char *name);
void		trap_startCamera(int time);
qboolean	trap_getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

qboolean	trap_GetEntityToken( char *buffer, int bufferSize );

void	CG_ClearParticles (void);
void	CG_AddParticles (void);
void	CG_ParticleSnow (qhandle_t pshader, vec3_t origin, vec3_t origin2, int turb, float range, int snum);
void	CG_ParticleSmoke (qhandle_t pshader, centity_t *cent);
void	CG_AddParticleShrapnel (localEntity_t *le);
void	CG_ParticleSnowFlurry (qhandle_t pshader, centity_t *cent);
void	CG_ParticleBulletDebris (vec3_t	org, vec3_t vel, int duration);
void	CG_ParticleSparks (vec3_t org, vec3_t vel, int duration, float x, float y, float speed);
void	CG_ParticleDust (centity_t *cent, vec3_t origin, vec3_t dir);
void	CG_ParticleMisc (qhandle_t pshader, vec3_t origin, int size, int duration, float alpha);
void	CG_ParticleExplosion (char *animStr, vec3_t origin, vec3_t vel, int duration, int sizeStart, int sizeEnd);
extern qboolean		initparticles;
int CG_NewParticleArea ( int num );

//
// OSP functions called from id files, by file
//
// cg_osp_freeze.c
void CG_OSPObituaryFreeze( entityState_t *es, char *targetName );
void CG_AddFragmentHideFrozen( localEntity_t *le );
// cg_osp_cmds.c
void CG_OSPClientAuth_f( void );
void CG_ServerCommandStuff( void );
void CG_OSPClientRecord( void );
void CG_OSPClientScreenshot( void );
void CG_OSPWStatsDown_f( void );
void CG_OSPShowStatsInfo( void );
void CG_OSPWStatsUp_f( void );
void CG_OSPPrintXStats( void );
void CG_OSPPrintSSet( void );
void CG_OSPAstats( qboolean top );
void CG_OSPBstats( qboolean top );
// cg_osp_cpmhud.c
void CG_OSPDrawCPMHUD( void );
// cg_osp_chs.c
int CG_OSPDrawStringWithShadow( int x, int y, const char *str, int charWidth, int charHeight, const float *color, int maxChars );
// cg_osp_customloc.c
void CG_CustomLocationsSetLocation( const char *info, vec3_t loc );
char *CG_CustomLocationsTeamChatCode( char *str );
// cg_osp_hmi.c
qboolean CG_OSPIsGameTypeCA( int gametype );
// cg_osp_main.c
qboolean CG_OSPGetClientFontSize( const vmCvar_t *cvar, int *w, int *h );
void CG_OSPEventSound( const char *name );
void CG_OSPEventCustomSound( entityState_t *es, int clientNum );
void CG_OSPPlaySound( entityState_t *es, int entityNum, sfxHandle_t sfx );
void CG_OSPUpdateUserInfo( qboolean arg );
void CG_OSPMoTD_f( void );
void CG_OSPParseViewList( void );
void CG_OSPDrawMultiview( void );
void CG_OSPAddDecals( void );
void CG_OSPDrawDecalInfo( void );
void CG_OSPCvarsRestrictValues( void );
// cg_osp_draw.c
void CG_OSPDrawLeftSlidingWindowsRoutine( ospSlidingPrintContext_t *context );
// cg_weapons.c
void CG_OSPColorsFromString( const char *s, float *a, float *b, float *c, float *d );
// cg_servercmds.c (static in id)
void CG_RemoveChatEscapeChar( char *text );
// bg_promode.c
void CPM_UpdateSettings( int mode );




//
// OSP functions called from cg_main.c, cg_players.c, cg_draw.c and
// cg_weapons.c, by file
//
// cg_osp_chs.c
void CHS_Init( void );
void CHS_Free( int index );
void CHS_Draw( void );
int CG_OSPDrawString( int x, int y, const char *str, int charWidth, int charHeight, const float *color, int maxChars, int flags );
int CG_OSPGetNormalizedStringSize( const char *str );
// cg_osp_cmds.c
void CG_OSPPrintTime_f( void );
// cg_osp_cpmhud.c
float CG_OSPDrawSpeed( float y );
// cg_osp_customloc.c
void CG_CustomLocationsLoad( void );
const char *CG_CustomLocationsGetName( const float *pos );
// cg_osp_hmi.c
int CG_OSPGetPresistantTeam( void );
// cg_osp_hud.c
void CG_OSPDrawIntermission( void );
void CG_OSPHUDRoutine( void );
void CG_OSPDrawCenterString( void );
void CG_OSPDrawCrosshair( void );
// cg_osp_main.c
const char *CG_GetCTFLocation( int loc );
void CG_InitCTFLocations( void );
void CG_OSPNormalizeText( char *src, int size, char *dst );
// cg_osp_scoreboard.c
qboolean CG_OSPDrawScoretable( void );
// cg_weapons.c
void CG_OSPColorFromNumber( int number, float *vector );
void CG_OSPColorFromChar( char c, float *vector );
// cg_draw.c
float CG_OSPDrawPing( float y );
// cg_main.c
void CG_PrintLog( char *msg );

// OSP: further prototypes
//
void CG_DrawField( int x, int y, int width, int value, int charWidth, int charHeight );
float CG_DrawTimer( float y );
float CG_DrawTeamOverlay( float y, qboolean right, qboolean upper );
float CG_DrawScores( float y );
float CG_DrawPowerups( float y );

// (global in OSP: also called from the OSP HUD files cg_osp_hud.c / cg_osp_cpmhud.c)
void CG_DrawReward( void );
float CG_DrawLagometer( float y );
void CG_ScanForCrosshairEntity( void );
void CG_DrawSpectator( void );
qboolean CG_DrawScoreboard( void );
qboolean CG_DrawFollow( void );
void CG_DrawAmmoWarning( void );
void CG_DrawWarmup( void );

// OSP-only files
//
// OSP's ^X / ^x colour codes with six hex digits (RRGGBB)
#define OSP_IsHexColorString( p ) ( ( p ) && *( p ) == Q_COLOR_ESCAPE && *( ( p ) + 1 ) && 	( *( ( p ) + 1 ) == 'X' || *( ( p ) + 1 ) == 'x' ) && *( ( p ) + 2 ) && *( ( p ) + 3 ) && 	*( ( p ) + 4 ) && *( ( p ) + 5 ) && *( ( p ) + 6 ) && *( ( p ) + 7 ) )
// cg_osp_freeze.c
void CG_OSPNormalizeNameCopy( const char *from, char *to );
// cg_osp_draw.c (called from cg_osp_cmds.c, cg_osp_main.c)
int CG_OSPDrawLeftSlidingWindow( float timeAppear, float timeShow, float timeHide, float timeFade, int numLines, int lineSize, int charWidth, int charHeight, char *text, float y, float *borderColor, float *bodyColor );
// cg_osp_main.c (called from cg_osp_cmds.c)
qboolean CG_OSPIsStatsHidden( qboolean checkGametype, qboolean checkWarmup );
// called from the files above
// cg_osp_cpmhud.c
int CG_OSPSetHUDStyle( int style );
// cg_osp_chs.c
void CG_OSPSetColor( const float *rgba );
void CG_OSPDrawPic( float x, float y, float w, float h, qhandle_t hShader );
// cg_osp_malloc.c (dlmalloc)
void *malloc( size_t bytes );
void malloc_stats( void );

// cg_osp_customloc.c
char *CG_LoadLine( char *buf, int len, char *out, int outSize );
void CG_CustomLocationsAddEntry( vec3_t pos, const char *name );
// cg_osp_draw.c (called from cg_osp_hud.c)
void CG_OSPDrawStatusBarAmmo1( float y );
void CG_OSPDrawStatusBarAmmo2( float y );

extern qboolean	localClient;	// cg_scoreboard.c: the own score line was drawn
void CG_OSPDrawClientScore( int x, int y, score_t *score, float *color, float fade );
int CG_OSPDrawTeamScores( int x, int y, int team, float fade, int maxScores );
void CG_OSPDrawField( int x, int y, int width, int value );

char *CG_OSPGetCvarName( vmCvar_t *cvar );
char *CG_OSPGetCvarDefault( vmCvar_t *cvar );
void CG_OSPCvarSet( vmCvar_t *cvar, const char *value );
void CG_OSPCvarReset( vmCvar_t *cvar );
void CG_OSPPlayGameSound( int index );
qboolean CG_OSPDrawScoreboard( void );

// cg_osp_cmds.c: the auth / time helpers it calls
int		trap_RealTime( qtime_t *qtime );
void	BG_OSPAuthInit( void );
void	BG_OSPAuthUpdateName( char *str );
void	BG_OSPAuthGetSecret( char *secret );

// OSP: cg_osp_cpmhud.c, cg_osp_chs.c, cg_osp_hud.c:
// the functions and globals they share
//
// cg_osp_cpmhud.c
extern int		cpmDrawPowerups;
extern int		cpmDrawWeaponSelect;
void CG_OSPDrawPlayerIcon( float x, float y, float w, float h, int clientNum, vec3_t angles, float *a6 );
void CG_OSPDrawCPMStatusbar0( void );
void CG_OSPDrawCPMStatusbarBars( void );
void CG_OSPDrawCPMStatusbar3( void );
void CG_OSPDrawCPMStatusbar4( void );
void CG_OSPDrawCPMStatusbar5( void );
void CG_OSPDrawCPMStatusbar( void );
void CG_OSPDrawWeaponBarCPM1( int y );
void CG_OSPDrawWeaponBarCPM2( int x, int y );
int CG_OSPDrawWeaponBar_6( float y );
int CG_OSPDrawWeaponBar_7( float y );
// cg_osp_chs.c
void CHS_Show( int index );
void CHS_Begin( int index, int duration, int a2, int a3, int a4 );
void CHS_End( void );
void CG_OSPDrawPoly( float x, float y, float w, float h, const float *color );
// cg_osp_hud.c
extern vec4_t	shaftColor;
float CG_OSPDrawWeaponSelect( float x, float y, const float *color );
float CG_OSPDrawTeamOverlay( float y, qboolean right, qboolean upper );
void CG_OSPDrawCrosshairNames( void );
float CG_OSPDrawWeaponBar_2_3( float y );
void CG_OSPDrawWeaponBar_4_5( float y );
void CG_OSPDrawVote( void );
qboolean CG_OSPDrawSpectatorInfo( void );
float CG_OSPHUDDrawScores( float y );
void CG_OSPDrawQ3CompStatusbar( void );
// cg_osp_hmi.c
void HMI_Draw( void );

// OSP: the HMI menu (cg_osp_hmicmds.c / cg_osp_hmi.c)
//
// cg_servercmds.c (static in id)
void CG_AddToTeamChat( char *str );
// cg_osp_hmicmds.c
extern hmiMenu_t	hmiMainMenu;

// cg_draw.c (static in id; the CPM / OSP HUDs call them)
float CG_DrawSnapshot( float y );
float CG_DrawFPS( float y );

// cg_osp_malloc.c (dlmalloc), used by CHS
void *calloc( size_t n, size_t elem_size );
void *realloc( void *oldmem, size_t bytes );
void free( void *mem );

// cg_osp_hud.c, called by CG_OSPHUDRoutine before their definition
float CG_OSPDrawStatusBar( float y );
void CG_OSPDrawStatusBarString( int x, int y, int len, int w, int h, int val );
float CG_OSPDrawPickupItem( float y );
float CG_OSPDrawTeamInfo( float y );
float CG_OSPDrawWeaponBarOld( float y );
float CG_OSPDrawHoldableItem( float y );
void CG_OSPCountTeammates( void );
// cg_draw.c (static in id; global in OSP: CG_OSPHUDRoutine calls them; CG_DrawSnapshot / CG_DrawFPS
// are declared above)
void CG_DrawStatusBar( void );
float CG_DrawAttacker( float y );

int CG_OSPCrosshairColor( void );

