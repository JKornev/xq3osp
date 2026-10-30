/*
 * cg_osp_draw.c: status bar ammo, sliding text windows (credits, MOTD, stats).
 */
#include "cg_local.h"

// OSP: the sliding text windows
ospSlidingPrintContext_t	ospPrintContext[MAX_OSP_SLIDING_WINDOWS];

// Ammo status bar (vertical bars): one ammo icon per held weapon from the machinegun on, with
// a bar whose height follows the ammo count; the bar is green above the weapon's "mid" value,
// blinks yellow down to "low" and blinks red below. cf_AmmoStatusbar gives the icon size,
// cf_Statusbar the bar height.
void CG_OSPDrawStatusBarAmmo1( float y ) {
	// full / mid / low ammo per weapon
	static int		weaponRanges[WP_NUM_WEAPONS][3] = {
		{ 0, 0, 0 },
		{ 0, 0, 0 },
		{ 200, 100, 50 },	// machinegun
		{ 40, 20, 10 },		// shotgun
		{ 40, 20, 10 },		// grenade launcher
		{ 40, 20, 10 },		// rocket launcher
		{ 200, 100, 50 },	// lightning gun
		{ 40, 20, 10 },		// railgun
		{ 200, 100, 50 },	// plasma gun
		{ 100, 50, 25 },	// BFG
		{ 0, 0, 0 }
	};
	static vec4_t	colors[3] = {
		{ 0.0f, 1.0f, 0.0f, 1.0f },
		{ 0.6f, 0.6f, 0.0f, 1.0f },
		{ 1.0f, 0.0f, 0.0f, 1.0f }
	};
	int		weaponIndex;
	int		ammoW;
	int		ammoH;
	int		i;
	float	wHi;
	int		h;
	float	*color;
	int		weaponHave;
	int		statusH;
	int		statusW;

	CG_OSPGetClientFontSize( &cf_Statusbar, &statusW, &statusH );
	CG_OSPGetClientFontSize( &cf_AmmoStatusbar, &ammoW, &ammoH );
	weaponHave = cg.snap->ps.stats[STAT_WEAPONS];
	i = 0;

	for ( weaponIndex = 2; weaponIndex < MAX_WEAPONS; weaponIndex++ ) {
		if ( weaponHave & ( 1 << weaponIndex ) ) {
			CG_DrawPic( i * ( ammoW + 2 ), y - ( ammoH + 2 ), ammoW, ammoH, cg_weapons[weaponIndex].weaponIcon );
			if ( weaponIndex == cg.weaponSelect ) {
				CG_DrawPic( i * ( ammoW + 2 ) - 2, y - ( ammoH + 4 ), ammoW + 4, ammoH + 4, cgs.media.selectShader );
			}

			wHi = cg.snap->ps.ammo[weaponIndex] / (float)weaponRanges[weaponIndex][0];
			if ( wHi > 1.0f ) {
				wHi = 1.0f;
			}

			if ( cg.snap->ps.ammo[weaponIndex] > weaponRanges[weaponIndex][1] ) {
				color = colors[0];
				color[3] = 1.0f;
			} else if ( cg.snap->ps.ammo[weaponIndex] > weaponRanges[weaponIndex][2] ) {
				float	t;

				color = colors[1];
				t = cg.time & 0x7ff;
				if ( t > 1024 ) {
					t = -t + 2047;
				}
				color[3] = t / 1463.0f + 0.3f;
			} else {
				float	t;

				color = colors[2];
				t = cg.time & 0x3ff;
				if ( t > 512 ) {
					t = -t + 1023;
				}
				color[3] = t / 731.0f + 0.3f;
			}

			h = wHi * ( statusH - ( ammoH + 4 ) );
			trap_R_SetColor( color );
			CG_DrawPic( i * ( ammoW + 2 ), y - ( ammoH + 4 ) - h, ammoW, h, cgs.media.teamStatusBar );
			trap_R_SetColor( NULL );
			i++;
		}
	}
}

// Ammo status bar (columns): ammo icon and "%3i" count per held weapon, in columns of
// cf_Statusbar height / cf_AmmoStatusbar height rows; the selected weapon gets a grey
// background.
void CG_OSPDrawStatusBarAmmo2( float y ) {
	int		weaponIndex;
	int		ammoW;
	int		ammoH;
	int		x;
	int		posY;
	int		i;
	int		rows;
	int		weaponHave;
	int		statusH;
	int		statusW;

	CG_OSPGetClientFontSize( &cf_Statusbar, &statusW, &statusH );
	CG_OSPGetClientFontSize( &cf_AmmoStatusbar, &ammoW, &ammoH );
	weaponHave = cg.snap->ps.stats[STAT_WEAPONS];
	i = 0;
	rows = statusH / ammoH;

	for ( weaponIndex = 2; weaponIndex < MAX_WEAPONS; weaponIndex++ ) {
		if ( weaponHave & ( 1 << weaponIndex ) ) {
			x = i / rows * ( 6 * ammoW );
			posY = y - ( rows - i % rows ) * ammoH;
			if ( weaponIndex == cg.weaponSelect ) {
				trap_R_SetColor( colorLtGrey );
				CG_DrawPic( x, posY, 5 * ammoW, ammoH, cgs.media.teamStatusBar );
			}
			CG_DrawPic( x, posY, ammoW, ammoH, cg_weapons[weaponIndex].weaponIcon );
			CG_OSPDrawString( x + 2 * ammoW, posY, va( "%3i", cg.snap->ps.ammo[weaponIndex] ), ammoW, ammoH, NULL, 0, 0 );
			i++;
		}
	}
	trap_R_SetColor( NULL );
}

// Opens a sliding text window (the free one, or the one that hides first): copies up to
// numLines lines of lineSize bytes from text (at most 24), stores the timings (seconds) and
// colours (default border black, body (0.7, 1, 0.6) at alpha 0.45); the window appears after
// timeFade seconds. Returns the window index.
int CG_OSPDrawLeftSlidingWindow( float timeAppear, float timeShow, float timeHide, float timeFade, int numLines, int lineSize, int charWidth, int charHeight, char *text, float y, float *borderColor, float *bodyColor ) {
	int							i;
	ospSlidingPrintContext_t	*ctx;
	char						*p;
	char						normalized[MAX_OSP_SLIDING_CHARS];
	int							minTime;
	int							minTimeIndex;

	minTimeIndex = -1;
	minTime = 9999999;
	for ( i = 0; i < MAX_OSP_SLIDING_WINDOWS; i++ ) {
		if ( !ospPrintContext[i].hideBeforeCGTime ) {
			minTimeIndex = i;
			break;
		}
		if ( ospPrintContext[i].hideBeforeCGTime < minTime ) {
			minTimeIndex = i;
			minTime = ospPrintContext[i].hideBeforeCGTime;
		}
	}
	if ( minTimeIndex == -1 ) {
		minTimeIndex = 0;
	}

	ctx = &ospPrintContext[minTimeIndex];
	ctx->windowPosY = y;
	ctx->charWidth = charWidth;
	ctx->charHeight = charHeight;
	ctx->timeAppearance = 1000 * timeAppear;
	ctx->timeShow = 1000 * timeShow;
	ctx->timeHiding = 1000 * timeHide;
	ctx->borderColor[0] = !borderColor ? 0 : borderColor[0];
	ctx->borderColor[1] = !borderColor ? 0 : borderColor[1];
	ctx->borderColor[2] = !borderColor ? 0 : borderColor[2];
	ctx->borderColor[3] = 1.0f;
	ctx->bodyColor[0] = !bodyColor ? 0.7f : bodyColor[0];
	ctx->bodyColor[1] = !bodyColor ? 1.0f : bodyColor[1];
	ctx->bodyColor[2] = !bodyColor ? 0.6f : bodyColor[2];
	ctx->bodyColor[3] = 0.45f;
	ctx->maxStringLen = 0;
	ctx->numberOfStrings = 0;

	for ( i = 0, p = text; i < numLines && ctx->numberOfStrings < MAX_OSP_SLIDING_LINES; i++, p += lineSize ) {
		if ( !p ) {
			break;
		}
		Q_strncpyz( ctx->string[i], p, MAX_OSP_SLIDING_CHARS );
		CG_OSPNormalizeText( ctx->string[i], strlen( ctx->string[i] ), normalized );
		if ( strlen( normalized ) > ctx->maxStringLen ) {
			ctx->maxStringLen = strlen( normalized );
		}
		ctx->numberOfStrings++;
	}

	ctx->hideBeforeRealtime = trap_Milliseconds() + ctx->timeHiding + 1000 * timeFade;
	ctx->hideBeforeCGTime = cg.time + ctx->timeHiding + 1000 * timeFade;
	ctx->showFromCGTime = cg.time + 1000 * timeFade;
	return minTimeIndex;
}

// Draws one sliding window: it slides in from the left during timeShow, stays, and slides
// out during the last timeAppearance ms; a 1-pixel border and a translucent body behind the
// lines, which end at windowPosY.
void CG_OSPDrawLeftSlidingWindowsRoutine( ospSlidingPrintContext_t *ctx ) {
	int		i;
	float	x;
	float	y;
	int		t;

	i = trap_Milliseconds();
	t = ctx->hideBeforeRealtime - i;
	x = 6.0f - ( t < ctx->timeHiding - ctx->timeShow
		? ( t < ctx->timeAppearance
			? ( ctx->charWidth * ctx->maxStringLen + 3.0 ) * ( ctx->timeAppearance - t ) / ctx->timeAppearance
			: 0 )
		: ( ctx->charWidth * ctx->maxStringLen + 3.0 ) * ( t - ctx->timeHiding + ctx->timeShow ) / ctx->timeShow );
	y = ctx->windowPosY - ctx->numberOfStrings * ( ctx->charHeight + 1 );

	// border: top, bottom, left, right
	CG_OSPSetColor( ctx->borderColor );
	CG_DrawPic( x, y - 2.0, ctx->charWidth * ctx->maxStringLen + 3, 1.0f, cgs.media.teamStatusBar );
	CG_DrawPic( x, y + 1.0 * ( ctx->charHeight + 1.0 ) * ctx->numberOfStrings + 1.0, ctx->charWidth * ctx->maxStringLen + 3, 1.0f, cgs.media.teamStatusBar );
	CG_DrawPic( x, y - 1.0, 1.0f, ( ctx->charHeight + 1 ) * ctx->numberOfStrings + 2, cgs.media.teamStatusBar );
	CG_DrawPic( 1.0 * ( ctx->charWidth * ctx->maxStringLen ) + x + 2.0, y - 1.0, 1.0f, ( ctx->charHeight + 1 ) * ctx->numberOfStrings + 2, cgs.media.teamStatusBar );

	// body
	CG_OSPSetColor( ctx->bodyColor );
	CG_OSPDrawPic( x + 1.0, y - 1.0, ctx->charWidth * ctx->maxStringLen + 1, ( ctx->charHeight + 1 ) * ctx->numberOfStrings + 2, cgs.media.teamStatusBar );
	CG_OSPSetColor( NULL );

	for ( i = 0; i < ctx->numberOfStrings; i++ ) {
		CG_OSPDrawStringWithShadow( x + 2.0, y + i * ( ctx->charHeight + 1 ), ctx->string[i], ctx->charWidth, ctx->charHeight, colorWhite, 0 );
	}
}
