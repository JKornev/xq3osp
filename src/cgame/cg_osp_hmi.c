/*
 * cg_osp_hmi.c: OSP's in-game menu ("HMI"; \menu, \menuleft, \menuright, the weapon keys
 * scroll): the generic item handlers and value labels, the menu stack, HMI_Draw, selection and
 * triggering; ends with two game-type helpers.
 */
#include "cg_local.h"

void HMI_Push( hmiMenu_t *menu );
void HMI_SelectFirst( void );
void HMI_SelectLast( void );

// the submenu items' func (never called: HMI_Trigger opens the submenu itself)
void HMI_Dummy( void *data, qboolean right ) {
	CG_Printf( "Dummy: var + qboolean:%d\n", right );
}

// value label: a time cvar in milliseconds as "s.t sec."
char *HMI_SecondsLabel( vmCvar_t *cvar ) {
	static char	buf[80];
	int			ms;

	if ( !cvar ) {
		return NULL;
	}
	ms = atoi( cvar->string );
	Com_sprintf( buf, sizeof( buf ), "%i.%i sec.", ms / 1000, ms / 100 % 10 );
	return buf;
}

// value label: values[cvar->integer] (the last entry when out of range), or the cvar's string
// without a value list
char *HMI_GetValueString( vmCvar_t *cvar, char **values ) {
	int		i;
	int		n;

	if ( !cvar ) {
		return NULL;
	}
	if ( !values ) {
		return cvar->string;
	}

	n = cvar->integer;
	for ( i = 0; values[i]; i++ ) {
		if ( i == n ) {
			return values[i];
		}
	}
	return values[i - 1];
}

void HMI_SetCvar( const char *name, const char *value ) {
	char	buf[MAX_STRING_CHARS];

	Com_sprintf( buf, sizeof( buf ), "seta %s \"%s\"", name, value );
	trap_SendConsoleCommand( buf );
}

// the font menu's "Adjust Height": the font's height 1..72
void HMI_FontHeight( vmCvar_t *cvar, qboolean right ) {
	int		w, h;

	CG_OSPGetClientFontSize( cvar, &w, &h );
	if ( right ) {
		if ( h < 72 ) {
			h++;
		}
	} else if ( h > 1 ) {
		h--;
	}
	HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%ix%i", w, h ) );
}

// the font menu's "Adjust Width": the font's width 1..72
void HMI_FontWidth( vmCvar_t *cvar, qboolean right ) {
	int		w, h;

	CG_OSPGetClientFontSize( cvar, &w, &h );
	if ( right ) {
		if ( w < 72 ) {
			w++;
		}
	} else if ( w > 1 ) {
		w--;
	}
	HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%ix%i", w, h ) );
}

// cg_crosshairSize 24..48 in steps of 2
void HMI_CrosshairSize( vmCvar_t *cvar, qboolean right ) {
	int		size;

	size = cvar->integer;
	if ( right ) {
		size += 2;
		if ( size > 48 ) {
			size = 48;
		}
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%i", size ) );
	} else {
		size -= 2;
		if ( size < 24 ) {
			size = 24;
		}
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%i", size ) );
	}
}

// cg_teamChatTime in steps of 1 s, 0 at the bottom
void HMI_TeamchatTime( vmCvar_t *cvar, qboolean right ) {
	if ( right ) {
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%i", cvar->integer + 1000 ) );
	} else if ( cvar->integer <= 1000 )
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), "0" );
	else {
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%i", cvar->integer - 1000 ) );
	}
}

// cg_railTrailTime in steps of 100 ms, 0 at the bottom
void HMI_RailTrailTime( vmCvar_t *cvar, qboolean right ) {
	if ( right ) {
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%i", cvar->integer + 100 ) );
	} else if ( cvar->integer <= 100 )
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), "0" );
	else {
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%i", cvar->integer - 100 ) );
	}
}

// cg_drawCrosshairNames 0..2 (OSP bug, kept for compatibility: the cvar's integer is changed in place, and going
// left is not wrapped)
void HMI_CrosshairNamesMode( vmCvar_t *cvar, qboolean right ) {
	HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", right ? ++cvar->integer % 3 : --cvar->integer ) );
}

void HMI_ToggleCvar( vmCvar_t *cvar ) {
	HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", !cvar->integer ) );
}

// unreferenced: a value 0..3, wrapped
void HMI_CycleCvar4( vmCvar_t *cvar, qboolean right ) {
	HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", ( cvar->integer + ( right ? 1 : 3 ) ) % 4 ) );
}

// unreferenced: a value 0..5
void HMI_CycleCvar6( vmCvar_t *cvar, qboolean right ) {
	if ( right ) {
		if ( cvar->integer < 5 ) {
			HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", cvar->integer + 1 ) );
		}
	} else if ( cvar->integer > 0 ) {
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", cvar->integer - 1 ) );
	}
}

// ch_StatusbarFormat 0..12
void HMI_StatusbarLayout( vmCvar_t *cvar, qboolean right ) {
	if ( right ) {
		if ( cvar->integer < 12 ) {
			HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", cvar->integer + 1 ) );
		}
	} else if ( cvar->integer > 0 ) {
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", cvar->integer - 1 ) );
	}
}

// ch_Weaponswitch 0..9
void HMI_WeaponSwitchLayout( vmCvar_t *cvar, qboolean right ) {
	if ( right ) {
		if ( cvar->integer < 9 ) {
			HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", cvar->integer + 1 ) );
		}
	} else if ( cvar->integer > 0 ) {
		HMI_SetCvar( CG_OSPGetCvarName( cvar ), va( "%d", cvar->integer - 1 ) );
	}
}

// value label: "(<string>)"
char *HMI_StringLabel( vmCvar_t *cvar ) {
	static char	buf[128];

	Com_sprintf( buf, sizeof( buf ), "(%s)", cvar->string );
	return buf;
}

// the main menu's "Quit" and the join-team actions
void HMI_Close( void *data, qboolean right ) {
	cg.hmiDepth = 0;
	cg.hmiActive = qfalse;
	cg.hmiMenu = NULL;
}

void HMI_Push( hmiMenu_t *menu ) {
	cg.hmiItemStack[cg.hmiDepth] = cg.hmiItem;
	cg.hmiMenuStack[cg.hmiDepth++] = menu;
	if ( cg.hmiDepth > HMI_STACK_SIZE ) {
		CG_Printf( "Warning: menu stack full!\n" );
	}
}

// OSP bug, kept for compatibility: an empty stack is popped anyway (hmiMenuStack[-1] is cg.hmiMenu)
void HMI_Pop( void ) {
	if ( !cg.hmiDepth ) {
		cg.hmiMenu = NULL;
	}
	cg.hmiMenu = cg.hmiMenuStack[--cg.hmiDepth];
	cg.hmiItem = cg.hmiItemStack[cg.hmiDepth];
}

// one HMI_LABEL / HMI_ACTION item of HMI_Draw: the label and its value (labelFunc, or the value
// list), centred
#define HMI_DRAWITEM() \
	value = NULL; \
	w = CG_OSPGetNormalizedStringSize( item->label ); \
	if ( item->labelFunc ) { \
		value = item->labelFunc( item->data, item->values ); \
		if ( value ) { \
			w += CG_OSPGetNormalizedStringSize( value ); \
		} else { \
			CG_Printf( "^1warning: received NULL after pfunction\n" ); \
		} \
	} else if ( item->values ) { \
		value = HMI_GetValueString( item->data, item->values ); \
		if ( value ) { \
			w += CG_OSPGetNormalizedStringSize( value ); \
		} \
	} \
	lines = x = 320 - w * cw / 2; \
	w = CG_OSPDrawString( x, y, item->label, cw, ch, color, 0, 0 ); \
	if ( item->type == HMI_LABEL ) { \
		color = colorWhite; \
	} \
	if ( value ) { \
		w += CG_OSPDrawStringWithShadow( x + w, y, value, cw, ch, color, 0 ); \
	}

/*
=================
HMI_Draw

The open menu, centred: the title bar, the items (the selected one red between ">>" and "<<",
labels grey) and three help lines. Fades in during the first second.
=================
*/
void HMI_Draw( void ) {
	static vec4_t	colorGrey = { 0.66f, 0.66f, 0.66f, 1.0f };
	static vec4_t	colorRed = { 1.0f, 0.0f, 0.0f, 1.0f };
	static vec4_t	colorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
	hmiItem_t	*item;
	int			i;
	int			ch;
	int			y;
	int			w;
	hmiMenu_t	*menu;
	int			lines;
	int			x;
	int			cw;
	float		*color;
	vec3_t		bg;			// OSP bug, kept for compatibility: bg[3] (the alpha) is written past the end, into value
	char		*value;

	if ( !cg.hmiActive ) {
		return;
	}

	cw = 12;
	ch = 18;
	x = 160;
	y = 120;

	if ( cg.hmiOpenTime + 1000 > cg.time ) {
		colorGrey[3] = (float)( cg.time - ( cg.hmiOpenTime + 1000 ) ) / 2000.0f + 1.0f;
		colorWhite[3] = colorGrey[3];
		colorRed[3] = colorGrey[3];
	} else {
		colorWhite[3] = 1.0f;
		colorRed[3] = 1.0f;
		colorGrey[3] = 1.0f;
	}

	if ( !cg.hmiMenu ) {
		cg.hmiMenu = &hmiMainMenu;
	}
	menu = cg.hmiMenu;

	lines = 2;
	for ( i = 0; item = &menu->items[i], item->type != HMI_END; i++ ) {
		if ( item->type != HMI_HIDDEN ) {
			lines++;
		}
	}
	y = 240 - ( ( lines - 1 ) * ch / 2 + 3 * ch / 4 );

	// title
	w = 1.5f * ( CG_DrawStrlen( menu->title ) * cw );
	bg[0] = 1.0f;
	bg[1] = 1.0f;
	bg[2] = 0.3f;
	bg[3] = colorGrey[3] - 0.3f;
	CG_OSPSetColor( bg );
	CG_DrawPic( 0, y - 2, 640, 3 * ch / 2 + 4, cgs.media.teamStatusBar );
	CG_OSPSetColor( NULL );
	CG_OSPDrawStringWithShadow( x + 160 - w / 2, y, va( "^3%s", menu->title ), 3 * cw / 2, 3 * ch / 2, colorWhite, 0 );
	y += 5 * ch / 2;

	// items
	bg[0] = 0.6f;
	bg[1] = 1.0f;
	bg[2] = 0.3f;
	bg[3] = colorGrey[3] - 0.3f;
	CG_OSPSetColor( bg );
	CG_DrawPic( 0, y - 2, 640, ( lines - 2 ) * ch + 4, cgs.media.teamStatusBar );
	CG_OSPSetColor( NULL );

	for ( i = 0; item = &menu->items[i], item->type != HMI_END; i++ ) {
		if ( cg.hmiItem == i ) {
			color = colorRed;
		} else {
			color = colorWhite;
		}

		switch ( item->type ) {
		case HMI_HIDDEN:
			continue;
		case HMI_LABEL:
			color = colorGrey;
			HMI_DRAWITEM();
			break;
		case HMI_ACTION:
			HMI_DRAWITEM();
			break;
		case HMI_SUBMENU:
			w = CG_OSPGetNormalizedStringSize( item->label ) * cw;
			lines = x = 320 - w / 2;
			w = CG_OSPDrawString( x, y, item->label, cw, ch, color, 0, 0 );
			break;
		case HMI_BACK:
			w = CG_OSPGetNormalizedStringSize( item->label ) * cw;
			lines = x = 320 - w / 2;
			w = CG_OSPDrawString( x, y, item->label, cw, ch, color, 0, 0 );
			break;
		default:
			CG_Printf( "^1warning: undefined menuitem label %i\n", item->type );
			break;
		}

		if ( cg.hmiItem == i ) {
			CG_DrawChar( lines - 14 - 4, y + ch / 2 - 7, 14, 14, '>' );
			CG_DrawChar( lines - 14, y + ch / 2 - 7, 14, 14, '>' );
			CG_DrawChar( x + w + 4, y + ch / 2 - 7, 14, 14, '<' );
			CG_DrawChar( x + w, y + ch / 2 - 7, 14, 14, '<' );
		}
		y += ch;
	}

	// help (OSP bug, kept for compatibility: the first line is measured with another text than it draws)
	y += ch;
	bg[0] = 1.0f;
	bg[1] = 0.3f;
	bg[2] = 0.3f;
	bg[3] = colorGrey[3] - 0.3f;
	CG_OSPSetColor( bg );
	CG_DrawPic( 0, y - 2, 640, 15 * ch / 8 + 4, cgs.media.teamStatusBar );
	CG_OSPSetColor( NULL );

	w = 5 * CG_OSPGetNormalizedStringSize( "\\prevweap / \\nextweap to scroll up/down" ) * cw / 8;
	w = CG_OSPDrawStringWithShadow( 320 - w / 2, y, "^3\\weapprev ^1/ ^3\\weapnext ^7to scroll up/down", 5 * cw / 8, 5 * ch / 8, colorWhite, 0 );
	y += 5 * ch / 8;
	w = 5 * CG_OSPGetNormalizedStringSize( "\\menuleft / \\menuright to choose backward/forward" ) * cw / 8;
	w = CG_OSPDrawStringWithShadow( 320 - w / 2, y, "^3\\menuleft ^1/ ^3\\menuright ^7to choose backward/forward", 5 * cw / 8, 5 * ch / 8, colorWhite, 0 );
	y += 5 * ch / 8;
	w = 5 * CG_OSPGetNormalizedStringSize( "\\menu at anytime to exit menu" ) * cw / 8;
	w = CG_OSPDrawStringWithShadow( 320 - w / 2, y, "^3\\menu ^7at anytime to exit menu", 5 * cw / 8, 5 * ch / 8, colorWhite, 0 );
}

// the first selectable item (-1: none)
void HMI_SelectFirst( void ) {
	int			i;
	hmiItem_t	*item;

	if ( !cg.hmiMenu ) {
		return;
	}
	for ( i = 0; ( item = &cg.hmiMenu->items[i] )->type != HMI_END; i++ ) {
		if ( item->type > 0 ) {
			cg.hmiItem = i;
			return;
		}
	}
	cg.hmiItem = -1;
}

// the last selectable item (-1: none)
void HMI_SelectLast( void ) {
	int			i;
	hmiItem_t	*item;
	int			last;

	if ( !cg.hmiMenu ) {
		return;
	}
	last = -1;
	for ( i = 0; ( item = &cg.hmiMenu->items[i] )->type != HMI_END; i++ ) {
		if ( item->type > 0 ) {
			last = i;
		}
	}
	cg.hmiItem = last;
}

// CG_NextWeapon_f (-1: the next selectable item, wrapping) / CG_PrevWeapon_f (-2: the previous
// one, wrapping)
void HMI_Scroll( int dir ) {
	int			i;
	hmiItem_t	*item;
	int			sel;
	int			n;

	if ( !cg.hmiActive ) {
		return;
	}

	switch ( dir ) {
	case -1:
		n = cg.hmiItem + 1;
		for ( i = 0; ( item = &cg.hmiMenu->items[i] )->type != HMI_END; i++ ) {
			if ( i >= n && item->type > 0 ) {
				cg.hmiItem = i;
				return;
			}
		}
		HMI_SelectFirst();
		break;
	case -2:
		n = cg.hmiItem - 1;
		for ( sel = i = 0; ( item = &cg.hmiMenu->items[i] )->type != HMI_END; i++ ) {
			if ( item->type > 0 ) {
				sel = i;
				break;
			}
		}
		if ( n < sel ) {
			HMI_SelectLast();
			return;
		}
		sel = -1;
		for ( i = 0; ( item = &cg.hmiMenu->items[i] )->type != HMI_END; i++ ) {
			if ( i > n && sel > -1 ) {
				cg.hmiItem = sel;
				return;
			}
			if ( item->type > 0 ) {
				sel = i;
			}
		}
		HMI_SelectLast();
		break;
	}
}

// \menuleft (right 0) / \menuright (right 1) on the selected item
void HMI_Trigger( int right ) {
	hmiItem_t	*item;

	if ( !cg.hmiActive ) {
		return;
	}

	item = &cg.hmiMenu->items[cg.hmiItem];
	switch ( item->type ) {
	case HMI_BACK:
		HMI_Pop();
		break;
	case HMI_ACTION: {
		void	(*func)();

		if ( !item->func ) {
			CG_Printf( "^1menuitem function pointer empty\n" );
			break;
		}
		func = item->func;
		func( item->data, right == 1 );
		break;
	}
	case HMI_SUBMENU:
		HMI_Push( cg.hmiMenu );
		cg.hmiMenu = item->submenu;
		HMI_SelectFirst();
		break;
	default:
		CG_Printf( "^1warning: menuitem trigger illegal type(%i)\n", item->type );
		break;
	case HMI_HIDDEN:
	case HMI_LABEL:
		break;
	}
}

void HMI_MenuLeft_f( void ) {
	HMI_Trigger( 0 );
}

void HMI_MenuRight_f( void ) {
	HMI_Trigger( 1 );
}

// \menu: opens / closes the menu (OSP HUD only, not in CPM)
void HMI_Menu_f( void ) {
	if ( !cg_enableOSPHUD.integer ) {
		CG_Printf( "Menu only available in OSP HUD mode\nuse: ^3cg_enableOSPHUD 1\n" );
		return;
	}
	if ( cgs.server_mode & 1 ) {
		CG_Printf( "Menu disabled in CPM.\n" );
		return;
	}

	if ( cg.hmiMenu ) {
		cg.hmiActive = qfalse;
		cg.hmiMenu = NULL;
	} else {
		cg.hmiActive = qtrue;
		cg.hmiOpenTime = cg.time;
	}
}

// unreferenced data after HMI_Trigger's jump table
int		hmiUnused1 = 0;
int		hmiUnused2 = 0;

// unreferenced
int HMI_Stub680( void ) {
	return 0;
}

// unreferenced
int HMI_Stub681( void ) {
	return 0;
}

qboolean CG_OSPIsGameTypeCA( int gametype ) {
	return BG_IsRoundBasedGametype( gametype );
}

// (sic); both branches read the same
int CG_OSPGetPresistantTeam( void ) {
	if ( !CG_OSPIsGameTypeCA( cgs.gametype ) ) {
		return cg.snap->ps.persistant[PERS_TEAM];
	}
	return cg.snap->ps.persistant[PERS_TEAM];
}
