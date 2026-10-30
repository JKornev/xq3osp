/*
 * cg_osp_scoreboard.c: OSP's two-column team scoreboard (cg_q3compScoreboard), drawn by
 * CG_DrawScoreboard.
 */
#include "cg_local.h"

// per-team sums of the drawn score lines, printed in the team boxes (the first
// declared global gets the highest address)
int		sumPingRed;
int		sumPingBlue;
int		sumScoresRed;
int		sumScoresBlue;

/*
=================
CG_OSPDrawClientScore

One score line: highlight of the own line, flag, head, score, net / thaws, ping (coloured by
its value), time and name; " Coach" and " READY" markers drawn over the line.
=================
*/
void CG_OSPDrawClientScore( int x, int y, score_t *score, float *color, float fade ) {
	char			string[1024];
	int				iconx;
	vec3_t			headAngles;
	clientInfo_t	*ci;

	if ( score->client < 0 || score->client >= cgs.maxclients ) {
		Com_Printf( "Bad score->client: %i\n", score->client );
		return;
	}

	ci = &cgs.clientinfo[ score->client ];
	iconx = x;

	// highlight your position
	if ( score->client == cg.snap->ps.clientNum ) {
		vec4_t	hcolor;

		localClient = qtrue;

		hcolor[0] = 0.7f;
		hcolor[1] = 0.7f;
		hcolor[2] = 0.7f;
		hcolor[3] = 0.2f * fade;
		CG_FillRect( x + 8, y, 304, 17, hcolor );
	}

	// draw the flag of a carrier
	if ( ci->powerups & ( 1 << PW_REDFLAG ) ) {
		CG_DrawFlagModel( iconx + 4, y, 16, 16, TEAM_RED, qfalse );
	} else if ( ci->powerups & ( 1 << PW_BLUEFLAG ) ) {
		CG_DrawFlagModel( iconx + 4, y, 16, 16, TEAM_BLUE, qfalse );
	}

	// draw the face
	trap_R_SetColor( NULL );
	VectorClear( headAngles );
	headAngles[YAW] = 180;
	CG_DrawHead( iconx + 20, y, 16, 16, score->client, headAngles );

	// draw the score line
	if ( score->ping == -1 ) {
		Com_sprintf( string, sizeof( string ), " ^2connecting^7      %s", ci->name );
		CG_OSPDrawStringWithShadow( iconx + 32, y + 2, string, 8, 12, colorWhite, 34 );
	} else {
		int		pingColor = 1;

		if ( score->ping < 40 ) {
			pingColor = 7;
		} else if ( score->ping < 70 ) {
			pingColor = 2;
		} else if ( score->ping < 120 ) {
			pingColor = 3;
		} else if ( score->ping < 190 ) {
			pingColor = 8;
		} else if ( score->ping < 400 ) {
			pingColor = 6;
		}

		Com_sprintf( string, sizeof( string ), "%3i", score->score );
		CG_OSPDrawStringWithShadow( iconx + 44, y, string, 12, 16, color, 3 );

		// team games: net score (thaws in freeze tag), red when negative
		if ( cgs.gametype == GT_TEAM ) {
			Com_sprintf( string, sizeof( string ), "^%i%3i", score->scoreFlags >= 0 ? 7 : 3, score->scoreFlags );
			CG_OSPDrawStringWithShadow( iconx + 88, y + 4, string, 8, 12, color, 4 );
		} else {
			CG_OSPDrawStringWithShadow( iconx + 88, y + 4, " 0", 8, 12, color, 2 );
		}

		Com_sprintf( string, sizeof( string ), "^%i%3i", pingColor, score->ping );
		CG_OSPDrawStringWithShadow( iconx + 116, y, string, 12, 16, color, 3 );
		Com_sprintf( string, sizeof( string ), "%3i", score->time );
		CG_OSPDrawStringWithShadow( iconx + 148, y, string, 12, 16, color, 3 );
		Com_sprintf( string, sizeof( string ), "%s", ci->name );
		CG_OSPDrawStringWithShadow( iconx + 200, y + 4, string, 8, 12, color, 14 );
	}

	// coaches (st: the coached team)
	if ( cgs.clientinfo[ score->client ].st ) {
		CG_OSPDrawStringWithShadow( iconx, y, va( " ^B%sCoach", cgs.clientinfo[ score->client ].st == TEAM_RED ? "^1" : "^4" ), 8, 12, colorWhite, 6 );
	}

	// add the "ready" marker for intermission exiting
	if ( ( cg.warmup || cg.predictedPlayerState.pm_type == PM_INTERMISSION ) &&
		( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << score->client ) ) ) {
		CG_OSPDrawStringWithShadow( iconx, y, " ^B^3READY", 8, 12, colorWhite, 6 );
	}
}

/*
=================
CG_OSPDrawTeamScores

Draws the score lines of one team (red / blue / their spectators 6 / 7 / spectators) at x, y,
at most maxScores, and returns how many were drawn. Red and blue add their lines to the sums.
Spectators are drawn in two columns; players that are not ready (clan arena: waiting for the
round) are dimmed.
=================
*/
int CG_OSPDrawTeamScores( int x, int y, int team, float fade, int maxScores ) {
	clientInfo_t	*ci;
	score_t			*score;
	int				i;
	int				count;
	vec4_t			color;
	int				row;

	color[0] = color[1] = color[2] = 1.0f;
	color[3] = fade;

	count = 0;
	for ( i = 0; i < cg.numScores && count < maxScores; i++ ) {
		score = &cg.scores[i];
		ci = &cgs.clientinfo[ score->client ];

		if ( ci->team != team && ci->rt != team ) {
			continue;
		}

		if ( team == TEAM_RED ) {
			if ( ci->team == team || CG_OSPIsGameTypeCA( cgs.gametype ) ) {
				sumPingRed += score->ping;
			}
			sumScoresRed += score->score;
		} else if ( team == TEAM_BLUE ) {
			if ( ci->team == team || CG_OSPIsGameTypeCA( cgs.gametype ) ) {
				sumPingBlue += score->ping;
			}
			sumScoresBlue += score->score;
		}

		if ( team == TEAM_SPECTATOR && ( !CG_OSPIsGameTypeCA( cgs.gametype ) || ci->rt == TEAM_SPECTATOR ) ) {
			row = count / 2;
			if ( count % 2 == 0 ) {
				CG_OSPDrawClientScore( x, y + 18 * ( row - 1 ), score, color, fade );
			} else {
				CG_OSPDrawClientScore( x + 320, y + 18 * ( row - 1 ), score, color, fade );
			}
			count++;
		} else if ( !CG_OSPIsGameTypeCA( cgs.gametype ) ) {
			if ( ci->team == team && !( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << score->client ) ) ) {
				CG_OSPDrawClientScore( x, y + 18 * count++, score, color, fade );
			} else if ( ci->team == team && ( cg.snap->ps.stats[ STAT_CLIENTS_READY ] & ( 1 << score->client ) ) ) {
				vec4_t	readyColor = { 0.5f, 0.5f, 0.5f, 0.5f };

				CG_OSPDrawClientScore( x, y + 18 * count++, score, readyColor, fade );
			}
		} else {
			if ( cgs.gametype == 5 && team != TEAM_SPECTATOR && ci->team == team ) {
				CG_OSPDrawClientScore( x, y + 18 * count++, score, color, fade );
			} else if ( cgs.gametype == 5 && ci->team == TEAM_SPECTATOR && ci->rt == team ) {
				vec4_t	waitColor = { 0.5f, 0.5f, 0.5f, 0.5f };

				CG_OSPDrawClientScore( x, y + 18 * count++, score, waitColor, fade );
			}
		}
	}

	return count;
}

/*
=================
CG_OSPDrawField

Big team score number, 1..3 digits wide (the width argument is not used).
=================
*/
void CG_OSPDrawField( int x, int y, int width, int value ) {
	if ( value >= 0 && value < 10 ) {
		CG_DrawField( x, y, 1, value, 32, 48 );
	} else if ( ( value > -10 && value < 0 ) || ( value >= 10 && value < 100 ) ) {
		CG_DrawField( x, y, 2, value, 32, 48 );
	} else {
		CG_DrawField( x, y, 3, value, 32, 48 );
	}
}

/*
=================
CG_OSPDrawScoretable

OSP's team scoreboard: the two team boxes with the team scores and sums, the red and blue
players, their spectators and the free spectators below.
=================
*/
qboolean CG_OSPDrawScoretable( void ) {
	vec4_t	colorRect;
	int		y;
	int		drewRed;
	char	*s;
	int		drewBlue;
	float	fade;
	int		max;
	char	string[128];
	float	*fadeColor;
	int		avgPing;
	int		x;
	int		w;

	sumPingRed = sumPingBlue = sumScoresRed = sumScoresBlue = 0;

	// don't draw anything if the menu or console is up
	if ( cg_paused.integer ) {
		cg.deferredPlayerLoading = 0;
		return qfalse;
	}

	if ( cgs.gametype == GT_SINGLE_PLAYER && cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		cg.deferredPlayerLoading = 0;
		return qfalse;
	}

	// don't draw scoreboard during death while warmup up
	if ( cg.warmup && !cg.showScores && cg.predictedPlayerState.pm_type != PM_INTERMISSION ) {
		return qfalse;
	}

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ||
		 cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		fade = 1.0;
		fadeColor = colorWhite;
	} else {
		fadeColor = CG_FadeColor( cg.scoreFadeTime, FADE_TIME );

		if ( !fadeColor ) {
			// next time scoreboard comes up, don't print killer
			cg.deferredPlayerLoading = 0;
			cg.killerName[0] = 0;
			return qfalse;
		}
		fade = *fadeColor;
	}

	if ( cg.demoPlayback ) {
		s = "^3Demo Playback";
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		x = ( SCREEN_WIDTH - w ) / 2;
		y = 40;
		CG_DrawBigString( x, y, s, fade );
	} else if ( cg.killerName[0] ) {
		// fragged by ... line
		s = va( "Fragged by %s", cg.killerName );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		x = ( SCREEN_WIDTH - w ) / 2;
		y = 40;
		CG_DrawBigString( x, y, s, fade );
	}

	// the team boxes with the team scores
	y = 64;
	colorRect[0] = 1.0f;
	colorRect[1] = 0.5f;
	colorRect[2] = 0.5f;
	colorRect[3] = 0.2f;
	CG_FillRect( 8, 64, 304, 48, colorRect );
	colorRect[0] = 0.5f;
	colorRect[2] = 1.0f;
	CG_FillRect( 328, 64, 304, 48, colorRect );

	CG_OSPDrawField( 8, 64, 3, cg.teamScores[0] );
	trap_R_SetColor( NULL );
	CG_OSPDrawField( 328, 64, 3, cg.teamScores[1] );
	trap_R_SetColor( NULL );

	// column headers (net score in TDM, thaws in freeze tag)
	y = 116;
	s = va( "^1Score %s Ping Min  Name", cgs.gametype == GT_TEAM ? ( cgs.gameTypeFreeze ? "THW" : "NET" ) : "PL " );
	CG_OSPDrawStringWithShadow( 40, y, s, 8, 12, colorWhite, 36 );
	s = va( "^4Score %s Ping Min  Name", cgs.gametype == GT_TEAM ? ( cgs.gameTypeFreeze ? "THW" : "NET" ) : "PL " );
	CG_OSPDrawStringWithShadow( 360, y, s, 8, 12, colorWhite, 36 );

	y = 140;
	drewRed = CG_OSPDrawTeamScores( 0, y, TEAM_RED, fade, 12 );
	drewBlue = CG_OSPDrawTeamScores( 320, y, TEAM_BLUE, fade, 12 );

	// team sums
	if ( drewRed ) {
		avgPing = sumPingRed / drewRed;
		if ( cgs.gametype >= GT_CTF ) {
			s = va( "^1Points  Players  AvgPing" );
			CG_OSPDrawStringWithShadow( 116, 64, s, 8, 16, colorWhite, 25 );
			Com_sprintf( string, sizeof( string ), "^3%3i^7  %2i  %3i", sumScoresRed, drewRed, avgPing );
			CG_OSPDrawStringWithShadow( 116, 80, string, 16, 20, colorWhite, 18 );
		} else if ( cgs.gametype == GT_TEAM ) {
			s = va( "^1Players  AvgPing" );
			CG_OSPDrawStringWithShadow( 140, 64, s, 8, 16, colorWhite, 25 );
			Com_sprintf( string, sizeof( string ), " %2i  %3i", drewRed, avgPing );
			CG_OSPDrawStringWithShadow( 140, 80, string, 16, 20, colorWhite, 18 );
		}
	}

	if ( drewBlue ) {
		avgPing = sumPingBlue / drewBlue;
		if ( cgs.gametype >= GT_CTF ) {
			s = va( "^4Points  Players  AvgPing" );
			CG_OSPDrawStringWithShadow( 436, 64, s, 8, 16, colorWhite, 25 );
			Com_sprintf( string, sizeof( string ), "^3%3i^7  %2i  %3i", sumScoresBlue, drewBlue, avgPing );
			CG_OSPDrawStringWithShadow( 436, 80, string, 16, 20, colorWhite, 18 );
		} else if ( cgs.gametype == GT_TEAM ) {
			s = va( "^4Players  AvgPing" );
			CG_OSPDrawStringWithShadow( 460, 64, s, 8, 16, colorWhite, 25 );
			Com_sprintf( string, sizeof( string ), " %2i  %3i", drewBlue, avgPing );
			CG_OSPDrawStringWithShadow( 460, 80, string, 16, 20, colorWhite, 18 );
		}
	}

	// the team spectators (teams 6 / 7) below the longer team
	max = drewRed < drewBlue ? drewBlue : drewRed;
	y += 18 * max + 18;
	drewRed = CG_OSPDrawTeamScores( 0, y, 6, fade, 8 );
	drewBlue = CG_OSPDrawTeamScores( 320, y, 7, fade, 8 );

	if ( drewRed ) {
		s = va( "^1Red Team Spectator" );
		CG_OSPDrawStringWithShadow( 60, y - 14, s, 8, 12, colorWhite, 30 );
	}
	if ( drewBlue ) {
		s = va( "^4Blue Team Spectator" );
		CG_OSPDrawStringWithShadow( 380, y - 14, s, 8, 12, colorWhite, 30 );
	}

	// the team columns
	max = drewRed < drewBlue ? drewBlue : drewRed;
	y += 18 * max + 36;
	colorRect[0] = 1.0f;
	colorRect[1] = 0.3f;
	colorRect[2] = 0.3f;
	colorRect[3] = 0.2f;
	CG_FillRect( 8, 112, 304, y - 148, colorRect );
	colorRect[0] = 0.3f;
	colorRect[2] = 1.0f;
	CG_FillRect( 328, 112, 304, y - 148, colorRect );

	// the free spectators
	drewRed = CG_OSPDrawTeamScores( 0, y, TEAM_SPECTATOR, fade, 24 );
	if ( drewRed ) {
		s = va( "Spectator" );
		CG_OSPDrawStringWithShadow( 276, y - 32, s, 8, 12, colorWhite, 30 );
		colorRect[0] = 0.5f;
		colorRect[1] = 0.5f;
		colorRect[2] = 0.5f;
		colorRect[3] = 0.2f;
		CG_FillRect( 8, y - 34, 624, 9 * drewRed + 9 + 20, colorRect );
	}

	// load any models that have been deferred
	if ( ++cg.deferredPlayerLoading > 10 ) {
		CG_LoadDeferredPlayers();
	}

	return qtrue;
}
