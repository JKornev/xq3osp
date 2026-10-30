/*
 * cg_osp_colors.c: the OSP colour-string helpers (color1, cg_enemyColors, cg_teamRails).
 */
#include "cg_local.h"

/*
=================
CG_OSPColorFromNumber

OSP: colour of one character of a colour string (rail colours, cg_teamRails, ...): 0..9 fixed
colours, 10..33 ('a'..'x') 24 hues around the colour circle
=================
*/
void CG_OSPColorFromNumber( const int number, float *vector ) {
	static int	numColors = 34;

	if ( number >= 10 && number < numColors ) {
		float	tmp = ( 360 * number - 3600 ) / 24;
		float	saturation = 1.0f;
		float	sixtyDegrees = 60.0 * M_PI / 180.0;
		float	red, green, blue;

		if ( tmp <= 120 ) {
			tmp = M_PI * ( tmp / 180 );
			blue = ( 1.0f - saturation ) / 3.0f;
			red = ( saturation * cos( tmp ) / cos( sixtyDegrees - tmp ) + 1.0f ) / 3.0f;
			green = 1.0f - ( red + blue );
		} else if ( tmp <= 240 ) {
			tmp = M_PI * ( ( tmp - 120 ) / 180 );
			red = ( 1.0f - saturation ) / 3.0f;
			green = ( saturation * cos( tmp ) / cos( sixtyDegrees - tmp ) + 1.0f ) / 3.0f;
			blue = 1.0f - ( red + green );
		} else {
			tmp = M_PI * ( ( tmp - 240 ) / 180 );
			green = ( 1.0f - saturation ) / 3.0f;
			blue = ( saturation * cos( tmp ) / cos( sixtyDegrees - tmp ) + 1.0f ) / 3.0f;
			red = 1.0f - ( green + blue );
		}
		VectorSet( vector, red, green, blue );
		return;
	}

	switch ( number ) {
	case 0:
		VectorSet( vector, 1.0f, 1.0f, 1.0f );
		break;
	case 1:
		VectorSet( vector, 1.0f, 0.2f, 0.2f );
		break;
	case 2:
		VectorSet( vector, 0.2f, 1.0f, 0.2f );
		break;
	case 3:
		VectorSet( vector, 1.0f, 1.0f, 0.1f );
		break;
	case 4:
		VectorSet( vector, 0.2f, 0.2f, 1.0f );
		break;
	case 5:
		VectorSet( vector, 1.0f, 0.1f, 1.0f );
		break;
	case 6:
		VectorSet( vector, 0.1f, 1.0f, 1.0f );
		break;
	case 7:
		VectorSet( vector, 1.0f, 0.59f, 0.12f );
		break;
	case 8:
		VectorSet( vector, 0.65f, 0.2f, 0.65f );
		break;
	case 9:
		VectorSet( vector, 0.6f, 0.6f, 1.0f );
		break;
	default:
		VectorSet( vector, 1.0f, 1.0f, 1.0f );
		break;
	}
}

/*
=================
CG_OSPColorFromChar

OSP: '0'..'9' -> 0..9, letters -> 10.. (case-insensitive)
=================
*/
void CG_OSPColorFromChar( char c, float *vector ) {
	int		number;

	number = ( c >= '0' && c <= '9' ) ? c - '0' : tolower( c ) - 'a' + 10;
	CG_OSPColorFromNumber( number, vector );
}

/*
=================
CG_OSPColorsFromString

OSP: up to four colour characters, missing ones are '0' (white)
=================
*/
void CG_OSPColorsFromString( const char *s, float *a, float *b, float *c, float *d ) {
	int		i = 0;
	char	str[4] = "0000";

	while ( s && *s && i < 4 ) {
		str[i++] = *s++;
	}

	CG_OSPColorFromChar( str[0], a );
	CG_OSPColorFromChar( str[1], b );
	CG_OSPColorFromChar( str[2], c );
	CG_OSPColorFromChar( str[3], d );
}
