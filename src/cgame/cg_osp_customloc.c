/*
 * cg_osp_customloc.c: custom locations (cg_Customloc, addpos, locs/<map>.cfg).
 */
#include "cg_local.h"

#define OSP_CUSTOMLOC_MAX		80
#define OSP_CUSTOMLOC_NAME_LEN	40

typedef struct {
	vec3_t	pos;
	char	name[OSP_CUSTOMLOC_NAME_LEN];
} customLocation_t;

// OSP: the team chat
// buffer of CG_CustomLocationsTeamChatCode (1064 bytes) and the location table
// (80 * 52 bytes)
static customLocation_t	customLocationEntry[OSP_CUSTOMLOC_MAX];

// OSP: initialised, so it lives in data
static int		numberOfCustomLocations = 0;


static void CG_OSPCustomLocationsWrite( void );

// Copies the next line of buf (len bytes left) into out, skipping '#' comment lines and empty
// lines; returns the start of the following line, or NULL at the end of the buffer.
char *CG_LoadLine( char *buf, int len, char *out, int outSize ) {
	int		remaining = len;
	char	*start = buf;
	char	*p = buf;

	if ( remaining < 1 ) {
		return NULL;
	}

	while ( *p == '#' || *p == '\n' ) {
		p = strchr( start, '\n' );
		if ( p ) {
			start = p + 1;
			remaining -= p - start;
			if ( strchr( start, '\r' ) == start ) {
				start++;
				remaining--;
			}
		} else {
			return NULL;
		}
		p = start;
		if ( remaining < 1 ) {
			return NULL;
		}
	}

	p = strchr( start, '\n' );
	if ( !p ) {
		p = start;
	}

	memset( out, 0, outSize );
	if ( p - start > outSize ) {
		CG_Printf( "Line too long.. skipping\n" );
	} else if ( p == start ) {
		if ( remaining > outSize ) {
			CG_Printf( "Last line too long.. skipping\n" );
		} else {
			memcpy( out, start, remaining );
		}
	} else if ( p < start ) {
		return NULL;
	} else {
		memcpy( out, start, p - start );
		out[p - start] = 0;
		if ( out[p - start - 1] == '\r' ) {
			out[p - start - 1] = 0;
		}
	}

	if ( p == buf ) {
		return NULL;
	}
	p++;
	if ( *p == '\r' ) {
		p++;
	}
	return p;
}

// Loads locs/<mapname>.cfg ("x y z name" lines) when cg_Customloc is set; schedules a user
// info update (cgs.nextCvarsUpdateTime) so the server learns the new state.
void CG_CustomLocationsLoad( void ) {
	char			*p;
	char			line[MAX_STRING_CHARS];
	int				matches;
	int				len;
	char			*buf;
	int				lineNum;
	const char		*s;
	char			filename[MAX_QPATH];
	fileHandle_t	f;

	numberOfCustomLocations = 0;
	cg.customLocationsEnabled = qfalse;
	if ( !cgs.nextCvarsUpdateTime ) {
		cgs.nextCvarsUpdateTime = cg.time + 1200;
	}

	if ( !cg_Customloc.integer ) {
		return;
	}

	s = CG_ConfigString( CS_SERVERINFO );
	if ( !*( s = Info_ValueForKey( s, "mapname" ) ) ) {
		CG_Printf( "^1Couldn't find mapname\n" );
		return;
	}
	Com_sprintf( filename, MAX_QPATH, "locs/%s.cfg", s );

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		CG_Printf( "^1locfile %s not found. Use \\addpos <text> to create locfile\n", filename );
		numberOfCustomLocations = 0;
		return;
	}

	buf = malloc( len + 1 );
	if ( !buf ) {
		CG_Printf( "^1not enough dynamic memory\n" );
		return;
	}

	if ( f ) {
		trap_FS_Read( buf, len, f );
		trap_FS_FCloseFile( f );
		p = buf;
		if ( len <= 0 ) {
			CG_Printf( "file error\n" );
			return;
		}
		buf[len] = 0;

		lineNum = 0;
		do {
			p = CG_LoadLine( p, len - ( p - buf ), line, MAX_STRING_CHARS );
			lineNum++;
			if ( !line[0] ) {
				continue;
			}
			matches = sscanf( line, "%f %f %f %40n",
				&customLocationEntry[numberOfCustomLocations].pos[0],
				&customLocationEntry[numberOfCustomLocations].pos[1],
				&customLocationEntry[numberOfCustomLocations].pos[2],
				customLocationEntry[numberOfCustomLocations].name );
			if ( matches != 4 ) {
				CG_Printf( "Ignored non-comment line %i\n", lineNum );
				continue;
			}
			numberOfCustomLocations++;
			if ( numberOfCustomLocations >= OSP_CUSTOMLOC_MAX ) {
				CG_Printf( "^1Maximum number of locations read (%d)\n", OSP_CUSTOMLOC_MAX );
				break;
			}
		} while ( p );

		CG_Printf( "^3>>> (%s):^5 %i custom locations enabled.\n", filename, numberOfCustomLocations );
		if ( numberOfCustomLocations > 0 ) {
			cg.customLocationsEnabled = qtrue;
		}
	}
}

// Name of the custom location nearest to pos, "unknown" without locations.
const char *CG_CustomLocationsGetName( const float *pos ) {
	vec3_t				delta;
	customLocation_t	*loc;
	int					i;
	float				dist;
	float				minDist;
	customLocation_t	*best;

	if ( !numberOfCustomLocations ) {
		goto unknown;
	}

	best = NULL;
	minDist = 201326592.0f;
	for ( i = 0; i < numberOfCustomLocations; i++ ) {
		loc = &customLocationEntry[i];
		delta[0] = loc->pos[0] - pos[0];
		delta[1] = loc->pos[1] - pos[1];
		delta[2] = loc->pos[2] - pos[2];
		dist = DotProduct( delta, delta );
		if ( dist > minDist ) {
			continue;
		}
		minDist = dist;
		best = loc;
	}
	if ( !best ) {
		goto unknown;
	}
	return best->name;

unknown:
	return "unknown";
}

// Appends a location and rewrites the map's locfile.
void CG_CustomLocationsAddEntry( vec3_t pos, const char *name ) {
	if ( numberOfCustomLocations >= OSP_CUSTOMLOC_MAX ) {
		CG_Printf( "^1Maximum of location limit reached\n" );
	} else {
		VectorCopy( pos, customLocationEntry[numberOfCustomLocations].pos );
		Q_strncpyz( customLocationEntry[numberOfCustomLocations].name, name, MAX_QPATH );
		numberOfCustomLocations++;
		CG_OSPCustomLocationsWrite();
		cg.teamOverlayChanged = 1;
	}
}

// "addpos <text>": adds the own position as a custom location (trailing spaces cut).
void CG_OSPAddPos_f( void ) {
	int		i;
	char	name[OSP_CUSTOMLOC_NAME_LEN];

	if ( trap_Argc() < 2 ) {
		CG_Printf( "usage: %s <text>\n", CG_Argv( 0 ) );
		return;
	}

	trap_Args( name, OSP_CUSTOMLOC_NAME_LEN );
	name[OSP_CUSTOMLOC_NAME_LEN - 1] = 0;
	for ( i = strlen( name ) - 1; i > 0 && name[i] == ' '; i-- ) {
		name[i] = 0;
	}
	CG_CustomLocationsAddEntry( cg_entities[cg.snap->ps.clientNum].lerpOrigin, name );
}

// Parses a team info location "a_x_y_z" (signed integers) into loc; clears loc when invalid.
void CG_CustomLocationsSetLocation( const char *info, vec3_t loc ) {
	int		i;
	float	v[4];
	int		neg;

	for ( i = 0; i < 4; i++ ) {
		v[i] = 0;
		if ( info && *info == '-' ) {
			neg = 1;
			info++;
		} else {
			neg = 0;
		}
		while ( *info && *info >= '0' && *info <= '9' ) {
			v[i] = v[i] * 10 + ( *info - '0' );
			info++;
		}
		if ( neg )
			v[i] = -v[i];
		if ( !*info && i < 3 ) {
invalid:
			VectorClear( loc );
			return;
		}
		if ( i < 3 ) {
			if ( *info == '_' ) {
				info++;
			} else {
				goto invalid;
			}
		}
	}
	loc[0] = v[1];
	loc[1] = v[2];
	loc[2] = v[3];
}

// Appends the last added location to locs/<mapname>.cfg ("x y z name" line).
static void CG_OSPCustomLocationsWrite( void ) {
	int				n;
	char			line[MAX_STRING_CHARS];
	fileHandle_t	f;
	char			filename[MAX_QPATH];

	{	// a block of its own: matches OSP's frame layout
		const char	*s;

		s = CG_ConfigString( CS_SERVERINFO );
		if ( !*( s = Info_ValueForKey( s, "mapname" ) ) ) {
			CG_Printf( "^1Couldn't find mapname\n" );
			return;
		}
		Com_sprintf( filename, MAX_QPATH, "locs/%s.cfg", s );

		trap_FS_FOpenFile( filename, &f, FS_APPEND_SYNC );
		if ( !f ) {
			CG_Printf( "*** ^3Couldn't open file %s for writing -- location not saved.\n", filename );
			return;
		}

		n = numberOfCustomLocations - 1;
		Com_sprintf( line, sizeof( line ), "%f %f %f %.64s\r\n",
			customLocationEntry[n].pos[0], customLocationEntry[n].pos[1],
			customLocationEntry[n].pos[2], customLocationEntry[n].name );
		trap_FS_Write( line, strlen( line ), f );
		trap_FS_FCloseFile( f );
		CG_Printf( "^3>>> Saved location \"%s^3\" to %s\n", customLocationEntry[n].name, filename );
	}
}

// Replaces a "$x$y$z)" position code in a team chat line by the nearest custom location
// name (cut to cg_MaxlocationWidth); returns str unchanged when there is no valid code.
char *CG_CustomLocationsTeamChatCode( char *str ) {
	static char	buf[MAX_STRING_CHARS + OSP_CUSTOMLOC_NAME_LEN];
	char		*p;
	int			i;
	vec3_t		v;
	int			neg;

	p = strstr( str, "$" );
	if ( !p ) {
		goto fail;
	}
	p++;
	Q_strncpyz( buf, str, p - str );

	for ( i = 0; i < 3; i++ ) {
		v[i] = 0;
		if ( p && *p == '-' ) {
			neg = 1;
			p++;
		} else {
			neg = 0;
		}
		while ( *p && *p >= '0' && *p <= '9' ) {
			v[i] = v[i] * 10 + ( *p - '0' );
			p++;
		}
		if ( neg )
			v[i] = -v[i];
		if ( !*p ) {
			VectorClear( v );
			break;
		}
		if ( i < 2 ) {
			if ( *p == '$' ) {
				p++;
			} else {
				goto fail;
			}
		}
	}
	if ( *p != ')' ) {
		goto fail;
	}

	Q_strncpyz( buf + strlen( buf ), CG_CustomLocationsGetName( v ), cg_MaxlocationWidth.integer + 1 );
	strcat( buf, va( "^7%s", p ) );
	return buf;

fail:
	return str;
}
