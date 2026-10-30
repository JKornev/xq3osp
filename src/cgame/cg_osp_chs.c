/*
 * cg_osp_chs.c: CHS: buffered 2D drawing on dynamic memory, and the OSP draw primitives
 * (string renderer, pic, poly, color).
 *
 * CHS keeps lists of stretch-pic items ("slots") on the dlmalloc heap. Between CHS_Begin and
 * CHS_End every CG_OSPSetColor / CG_OSPDrawPic / CG_OSPDrawString / CG_OSPDrawPoly call is
 * recorded into the current slot instead of being drawn; CHS_Draw (from CG_Draw2D) replays
 * every slot each frame, with a fade in / fade out, until the slot's end time. Slots 0..63 are
 * fixed (the team overlay uses 6), slots from 64 up are allocated by CHS_Begin( -1, ... ).
 */
#include "cg_local.h"

// one recorded stretch pic (60 bytes, the stride in CHS_AddItem / CHS_Draw)
typedef struct {
	vec4_t		color;			// [3] is set to fade * alpha when drawn
	float		alpha;
	float		x, y, w, h;
	float		s1, t1, s2, t2;
	qhandle_t	shader;
	qboolean	setColor;		// set the colour before this item
} chsItem_t;

// one slot (36 bytes: CHS_Begin's calloc( 1, 0x24 ))
typedef struct {
	int			startTime;
	int			endTime;		// 0x7fffffff without a duration
	int			fadeIn;
	int			fadeOut;
	chsItem_t	*items;
	int			maxItems;		// grows by 12 (CHS_Alloc)
	int			numItems;
	qboolean	drawFrame;		// draw a non-persistent slot in this frame (CHS_Show)
	qboolean	persistent;		// drawn every frame
} chsSlot_t;

// OSP: file-scope globals are laid out in
// reverse order of definition
chsSlot_t	**chsSlots;
int			chsNumSlots;		// 100 (CHS_Init), +24 per CHS_AllocClear growth
int			chsMaxSlot;			// highest fixed slot used (CHS_Draw's first loop)
int			chsCurSlot;			// the slot CHS_AddItem records into
vec4_t		chsColor;			// the colour recorded with the next item
qboolean	chsColorSet;		// a colour was set (CHS_AddItem's setColor)

// OSP: the file's initialised data
int			chsWarnTime = 0;	// next "out of dynamic memory" warning (CHS_Draw / CHS_Begin)
qboolean	chsActive = qfalse;	// recording into chsCurSlot (CHS_Begin .. CHS_End)

// trap_R_SetColor, recorded while CHS is active (CG_OSPSetColor, CG_OSPDrawString, CG_OSPDrawPoly)
#define CHS_SETCOLOR( c ) \
	if ( chsActive == qtrue ) { \
		if ( c ) { \
			VectorCopy( c, chsColor ); \
			chsColor[3] = ( c )[3]; \
		} \
		chsColorSet = qtrue; \
	} else { \
		trap_R_SetColor( c ); \
		chsColorSet = qtrue; \
	}

// one hex digit of a ^X / ^x RRGGBB colour code (as cg_osp_cmds.c's OSP_IS_HEX_DIGIT)
#define CHS_IS_HEX( c ) ( ( c ) && ( ( ( c ) >= '0' && ( c ) <= '9' ) || ( ( c ) >= 'A' && ( c ) <= 'F' ) || ( ( c ) >= 'a' && ( c ) <= 'f' ) ) )
#define CHS_HEX_VALUE( c ) ( ( c ) > '9' ? ( ( c ) >= 'a' ? ( c ) - 'a' + 10 : ( c ) - 55 ) : ( c ) - '0' )

/*
CHS_Init: the slot pointer array (OSP passes calloc's arguments the wrong way round)
*/
void CHS_Init( void ) {
	chsSlots = calloc( sizeof( chsSlot_t ), 100 );
	chsNumSlots = 100;
	if ( !chsSlots ) {
		CG_Printf( "^1Warning: low initial memory for CHS\n" );
	}
}

/*
CHS_Free: frees a slot and its items
*/
void CHS_Free( int index ) {
	if ( !chsSlots ) {
		return;
	}
	if ( !chsSlots[index] ) {
		return;
	}
	if ( chsSlots[index]->items ) {
		free( chsSlots[index]->items );
	}
	free( chsSlots[index] );
	chsSlots[index] = NULL;
}

/*
CHS_Alloc: room for 12 more items in a slot
*/
int CHS_Alloc( int index ) {
	chsSlots[index]->maxItems += 12;
	chsSlots[index]->items = realloc( chsSlots[index]->items, sizeof( chsItem_t ) * chsSlots[index]->maxItems );
	if ( !chsSlots[index]->items ) {
		CG_Printf( "^1Warning: Out of dynamic memory for CHS!\n" );
		CHS_Free( index );
		return 0;
	}
	return 1;
}

/*
CHS_Draw: replays every slot, fading in / out (from CG_Draw2D)
*/
void CHS_Draw( void ) {
	chsItem_t	*item;
	int			j;
	int			i;
	chsSlot_t	*slot;
	float		fade;
	int			setColor;

	if ( !chsSlots && chsWarnTime < cg.time ) {
		CG_Printf( "^1Warning: out of dynamic memory for CHS\n" );
		chsWarnTime += 5000;
		return;
	}

	for ( i = 0; i <= chsMaxSlot; i++ ) {
		if ( !chsSlots[i] ) {
			continue;
		}
		slot = chsSlots[i];
		if ( slot->endTime < cg.time ) {
			CHS_Free( i );
			continue;
		}
		if ( slot->persistent == 0 && slot->drawFrame == 0 ) {
			continue;
		}
		slot->drawFrame = qfalse;
		if ( cg.time - slot->startTime > 0 && cg.time - slot->startTime < slot->fadeIn ) {
			fade = ( cg.time - slot->startTime ) / (float)slot->fadeIn;
			setColor = 1;
		} else if ( slot->endTime - cg.time > 0 && slot->endTime - cg.time < slot->fadeOut ) {
			fade = ( slot->endTime - cg.time ) / (float)slot->fadeOut;
			setColor = 1;
		} else {
			fade = 1.0f;
			setColor = 1;
		}
		for ( j = 0; j <= chsSlots[i]->numItems; j++ ) {
			item = &chsSlots[i]->items[j];
			if ( setColor == 1 || item->setColor ) {
				item->color[3] = fade * item->alpha;
				trap_R_SetColor( item->color );
				setColor = 0;
			}
			trap_R_DrawStretchPic( item->x, item->y, item->w, item->h, item->s1, item->t1, item->s2, item->t2, item->shader );
		}
	}

	for ( i = MAX_OSP_CHS_FIXED_SLOTS; i <= chsNumSlots; i++ ) {
		if ( !chsSlots[i] ) {
			continue;
		}
		slot = chsSlots[i];
		if ( chsSlots[i]->endTime < cg.time ) {
			CHS_Free( i );
			continue;
		}
		if ( ( 0 < cg.time - slot->startTime ? 0 : cg.time - slot->startTime ) > slot->fadeIn ) {
			fade = ( cg.time - slot->startTime ) / (float)slot->fadeIn;
		} else if ( slot->endTime - cg.time > 0 && slot->endTime - cg.time < slot->fadeOut ) {
			fade = ( slot->endTime - cg.time ) / (float)slot->fadeOut;
		} else {
			fade = 1.0f;
		}
		for ( j = 0; j <= chsSlots[i]->numItems; j++ ) {
			item = &chsSlots[i]->items[j];
			item->color[3] = fade * item->alpha;
			trap_R_SetColor( colorWhite );
			trap_R_DrawStretchPic( item->x, item->y, item->w, item->h, item->s1, item->t1, item->s2, item->t2, item->shader );
		}
	}

	trap_R_SetColor( NULL );
}

/*
CHS_AllocClear: a free dynamic slot (64 and up); grows the slot array by 24
*/
int CHS_AllocClear( void ) {
	int i;

	for ( i = MAX_OSP_CHS_FIXED_SLOTS; i < chsNumSlots; i++ ) {
		if ( !chsSlots[i] ) {
			return i;
		}
	}
	chsNumSlots += 24;
	chsSlots = realloc( chsSlots, chsNumSlots * sizeof( chsSlot_t * ) );
	memset( chsSlots[chsNumSlots - 24], 0, 24 * 4 );	// OSP bug, kept for compatibility: clears through the new (garbage) pointer
	if ( !chsSlots ) {
		CG_Printf( "^1Warning: Out of dynamic memory for CHS!\n" );
		return -1;
	}
	return i;
}

/*
CHS_Show: draw a non-persistent slot's recorded items in this frame (the team overlay
calls it every frame and records only when its info changed)
*/
void CHS_Show( int index ) {
	if ( index < 0 || index > MAX_OSP_CHS_FIXED_SLOTS ) {
		CG_Printf( "^3Internal CHS_Begin error\n" );
		chsActive = qfalse;
		return;
	}
	if ( !chsSlots[index] ) {
		return;
	}
	chsSlots[index]->drawFrame = qtrue;
}

/*
CHS_Begin: starts recording into a slot (-1: a new dynamic slot); a duration of 0 never ends
*/
void CHS_Begin( int index, int duration, int fadeIn, int fadeOut, int persistent ) {
	if ( !chsSlots && chsWarnTime < cg.time ) {
		CG_Printf( "^1Warning: out of dynamic memory for CHS\n" );
		chsWarnTime += 5000;
		return;
	}
	if ( index == -1 ) {
		index = CHS_AllocClear();
		if ( index == -1 ) {
			return;
		}
		chsSlots[index]->items = NULL;
	} else if ( index >= MAX_OSP_CHS_FIXED_SLOTS ) {
		CG_Printf( "^3Internal CHS_Begin error\n" );
		chsActive = qfalse;
		return;
	} else {
		chsMaxSlot = ( chsMaxSlot < index ) ? index : chsMaxSlot;
	}
	if ( !chsSlots[index] ) {
		chsSlots[index] = calloc( 1, sizeof( chsSlot_t ) );
		if ( !chsSlots[index] ) {
			CG_Printf( "^1Warning: Out of dynamic memory for CHS!\n" );
			chsActive = qfalse;
			return;
		}
	}
	chsSlots[index]->items = realloc( chsSlots[index]->items, 32 * sizeof( chsItem_t ) );
	chsSlots[index]->maxItems = 32;
	chsSlots[index]->numItems = 0;
	chsSlots[index]->persistent = persistent;
	chsSlots[index]->drawFrame = qfalse;
	if ( !chsSlots[index]->items ) {
		CG_Printf( "^1Warning: Out of dynamic memory for CHS!\n" );
		CHS_Free( index );
		chsActive = qfalse;
		return;
	}
	chsActive = qtrue;
	chsCurSlot = index;
	if ( duration ) {
		chsSlots[index]->endTime = cg.time + duration;
	} else {
		chsSlots[index]->endTime = 0x7fffffff;
	}
	chsSlots[index]->startTime = cg.time;
	chsSlots[index]->fadeIn = fadeIn;
	chsSlots[index]->fadeOut = fadeOut;
}

/*
CHS_End: stops recording
*/
void CHS_End( void ) {
	chsActive = qfalse;
}

/*
CHS_AddItem: records one stretch pic with the current colour
*/
void CHS_AddItem( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	chsItem_t	*item;

	chsSlots[chsCurSlot]->numItems++;
	if ( chsSlots[chsCurSlot]->numItems == chsSlots[chsCurSlot]->maxItems ) {
		if ( !CHS_Alloc( chsCurSlot ) ) {
			chsActive = qfalse;
			return;
		}
	}
	item = &chsSlots[chsCurSlot]->items[chsSlots[chsCurSlot]->numItems];
	VectorCopy( chsColor, item->color );
	item->alpha = chsColor[3];
	item->setColor = ( chsSlots[chsCurSlot]->numItems == 0 ) ? qtrue : chsColorSet;
	item->x = x;
	item->y = y;
	item->w = w;
	item->h = h;
	item->s1 = s1;
	item->t1 = t1;
	item->s2 = s2;
	item->t2 = t2;
	item->shader = hShader;
}

/*
CG_OSPSetColor: trap_R_SetColor, or recorded while CHS is active (NULL keeps the colour)
*/
void CG_OSPSetColor( const float *rgba ) {
	CHS_SETCOLOR( rgba );
}

/*
CG_OSPDrawPic: CG_DrawPic, or recorded while CHS is active
*/
void CG_OSPDrawPic( float x, float y, float w, float h, qhandle_t hShader ) {
	CG_AdjustFrom640( &x, &y, &w, &h );
	if ( chsActive )
		CHS_AddItem( x, y, w, h, 0, 0, 1, 1, hShader );
	else
		trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, hShader );
}

/*
CG_OSPDrawStringWithShadow: a black shadow 1 (2 above 12 pixels) to the lower right, then the
string; returns the width drawn
*/
int CG_OSPDrawStringWithShadow( int x, int y, const char *str, int charWidth, int charHeight, const float *color, int maxChars ) {
	vec4_t	shadow = { 0, 0, 0, 1 };

	shadow[3] = color ? color[3] : 1.0f;
	CG_OSPDrawString( x + ( charWidth > 12 ? 2 : 1 ), y + ( charHeight > 12 ? 2 : 1 ), str, charWidth, charHeight, shadow, maxChars, 1 );
	return CG_OSPDrawString( x, y, str, charWidth, charHeight, color, maxChars, 0 );
}

/*
CG_OSPDrawString: OSP's string renderer; returns the width drawn. Codes: ^0..^9 colours (not in
the shadow pass, flags 1), ^Xrrggbb / ^xrrggbb colours (set in the shadow pass only, OSP bug),
^b / ^B blinking alpha, ^F .. ^f / ^f .. ^F alternating text, ^N / ^n back to the alpha of
setColor. maxChars 0 means no limit.
*/
int CG_OSPDrawString( int x, int y, const char *str, int charWidth, int charHeight, const float *setColor, int maxChars, int flags ) {
	vec4_t		color = { 1, 1, 1, 1 };
	const char	*s;
	int			ch;
	int			xx;
	float		frow, fcol;
	int			cnt;
	int			row, col;
	int			t;
	float		ax, ay;
	float		alpha;

	ax = charWidth * cgs.screenXScale;
	ay = charHeight * cgs.screenYScale;
	s = str;
	xx = x;
	cnt = 0;
	if ( setColor ) {
		color[0] = setColor[0];
		color[1] = setColor[1];
		color[2] = setColor[2];
		color[3] = setColor[3];
	}
	CHS_SETCOLOR( color );
	alpha = color[3];
	if ( maxChars == 0 ) {
		maxChars = 32767;
	}

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
			switch ( s[-1] ) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				if ( flags == 1 ) {
					break;
				}
				t = ( s[-1] - '0' ) % 10;
				color[0] = g_color_table[t][0];
				color[1] = g_color_table[t][1];
				color[2] = g_color_table[t][2];
				CHS_SETCOLOR( color );
				break;
			case 'N':
			case 'n':
normal:
				color[3] = alpha;
				CHS_SETCOLOR( color );
				break;
			case 'b':
				t = cg.time & 2047;
				if ( t > 1024 ) {
					t = -t + 2047;
				}
				color[3] = t / 1024.0f;
				CHS_SETCOLOR( color );
				break;
			case 'B':
				t = cg.time & 2047;
				if ( t > 1024 ) {
					t = -t + 2047;
				}
				color[3] = t / 1463.0f + 0.3f;
				CHS_SETCOLOR( color );
				break;
			case 'F':
flashOn:
				if ( ( cg.time & 1023 ) >= 512 ) {
					while ( *s ) {
						if ( *s == '^' && s[1] ) {
							if ( s[1] == 'N' ) {
								goto normal;
							}
							if ( s[1] == 'f' ) {
								goto flashOff;
							}
						}
						s++;
					}
				}
				break;
			case 'f':
flashOff:
				if ( ( cg.time & 1023 ) < 512 ) {
					while ( *s ) {
						if ( *s == '^' && s[1] ) {
							if ( s[1] == 'N' ) {
								goto normal;
							}
							if ( s[1] == 'F' ) {
								goto flashOn;
							}
						}
						s++;
					}
				}
				break;
			case 'X':
			case 'x':
				if ( CHS_IS_HEX( s[0] ) && CHS_IS_HEX( s[1] ) && CHS_IS_HEX( s[2] ) && CHS_IS_HEX( s[3] )
					&& CHS_IS_HEX( s[4] ) && CHS_IS_HEX( s[5] ) ) {
					color[0] = ( ( CHS_HEX_VALUE( s[0] ) << 4 ) + CHS_HEX_VALUE( s[1] ) ) / 255.0f;
					color[1] = ( ( CHS_HEX_VALUE( s[2] ) << 4 ) + CHS_HEX_VALUE( s[3] ) ) / 255.0f;
					color[2] = ( ( CHS_HEX_VALUE( s[4] ) << 4 ) + CHS_HEX_VALUE( s[5] ) ) / 255.0f;
					s += 6;
					if ( flags == 1 ) {
						CHS_SETCOLOR( color );
					}
				}
				break;
			}
			continue;
		}
		ch = *s;
		if ( ++cnt > maxChars ) {
			return xx - x;
		}
		if ( ch != ' ' ) {
			row = ch >> 4;
			col = ch & 15;
			frow = 0.0625f * row;
			fcol = 0.0625f * col;
			if ( chsActive )
				CHS_AddItem( xx * cgs.screenXScale, y * cgs.screenYScale, ax, ay, fcol, frow, fcol + 0.0625f, frow + 0.0625f,
					ch_3waveFont.integer ? cgs.media.charsetShader1 : cgs.media.charsetShader );
			else
				trap_R_DrawStretchPic( xx * cgs.screenXScale, y * cgs.screenYScale, ax, ay, fcol, frow, fcol + 0.0625f, frow + 0.0625f,
					ch_3waveFont.integer ? cgs.media.charsetShader1 : cgs.media.charsetShader );
		}
		xx += charWidth;
		s++;
	}
	CHS_SETCOLOR( colorWhite );
	return xx - x;
}

/*
CG_OSPDrawStringEscaped: CG_OSPDrawString with every character but the colour codes moved to
the charset's upper half (| 0x80)
*/
int CG_OSPDrawStringEscaped( int x, int y, const char *str, int charWidth, int charHeight, const float *color, int maxChars ) {
	const char	*s;
	char		*d;
	char		buf[MAX_STRING_CHARS];

	s = str;
	d = buf;
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			*d++ = *s++;
			*d++ = *s++;
		} else {
			*d++ = *s++ | 0x80;
		}
	}
	*d = 0;
	return CG_OSPDrawString( x, y, buf, charWidth, charHeight, color, maxChars, 0 );
}

/*
CG_OSPGetNormalizedStringSize: the number of printable characters (^X codes and ^Xrrggbb
skipped)
*/
int CG_OSPGetNormalizedStringSize( const char *str ) {
	const char	*s;
	int			count;

	s = str;
	count = 0;
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( ( s[1] == 'x' || s[1] == 'X' ) && CHS_IS_HEX( s[2] ) && CHS_IS_HEX( s[3] ) && CHS_IS_HEX( s[4] )
				&& CHS_IS_HEX( s[5] ) && CHS_IS_HEX( s[6] ) && CHS_IS_HEX( s[7] ) ) {
				s += 8;
			} else {
				s += 2;
			}
		} else {
			count++;
			s++;
		}
	}
	return count;
}

/*
CG_OSPDrawPoly: a filled rectangle in a colour (recorded while CHS is active); leaves the
colour white
*/
void CG_OSPDrawPoly( float x, float y, float w, float h, const float *color ) {
	CHS_SETCOLOR( color );
	CG_AdjustFrom640( &x, &y, &w, &h );
	if ( chsActive )
		CHS_AddItem( x, y, w, h, 0, 0, 0, 0, cgs.media.whiteShader );
	else
		trap_R_DrawStretchPic( x, y, w, h, 0, 0, 0, 0, cgs.media.whiteShader );
	CHS_SETCOLOR( colorWhite );
}
