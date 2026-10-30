/*
 * cg_osp_freeze.c: freeze tag client side (thaw obituary, frozen fragments).
 */
#include "cg_local.h"

/*
=================
CG_OSPNormalizeNameCopy

Copies a name without OSP's ^X / ^x RRGGBB colour codes
=================
*/
void CG_OSPNormalizeNameCopy( const char *from, char *to ) {
	while ( *from ) {
		if ( OSP_IsHexColorString( from ) ) {
			from += 8;
		} else {
			*to++ = *from++;
		}
	}
	*to = 0;
}

/*
=================
CG_OSPObituaryFreeze

Freeze tag thaw message (CG_Obituary with MOD_UNKNOWN)
=================
*/
void CG_OSPObituaryFreeze( entityState_t *es, char *targetName ) {
	char		attackerName[MAX_QPATH];
	int			attacker;
	const char	*info;
	int			target;
	char		targetNormalized[MAX_QPATH];
	char		attackerNormalized[MAX_QPATH];
	char		*action;
	char		*message;

	target = es->otherEntityNum;
	attacker = es->otherEntityNum2;

	info = CG_ConfigString( CS_PLAYERS + attacker );
	if ( !info ) {
		return;
	}
	Q_strncpyz( attackerName, Info_ValueForKey( info, "n" ), sizeof( attackerName ) );

	if ( rand() & 1 ) {
		action = "thawed";
		message = "like a package of frozen peas";
	} else {
		int		gender = cgs.clientinfo[target].gender;

		action = "evicted";
		if ( gender == GENDER_FEMALE ) {
			message = "from her igloo";
		} else if ( gender == GENDER_NEUTER ) {
			message = "from its igloo";
		} else {
			message = "from his igloo";
		}
	}

	if ( attacker == cg.snap->ps.clientNum ) {
		CG_CenterPrint( va( "You thawed %s", targetName ), 120, 16 );
	} else if ( target == cg.snap->ps.clientNum ) {
		CG_CenterPrint( va( "%s ^7unfroze you", attackerName ), 120, 16 );
	}

	CG_OSPNormalizeNameCopy( attackerName, attackerNormalized );
	CG_OSPNormalizeNameCopy( targetName, targetNormalized );
	CG_Printf( "%s ^6%s ^7%s ^6%s.\n", attackerNormalized, action, targetNormalized, message );
}

/*
=================
CG_AddFragmentHideFrozen

Adds an ice gib once more without its freeze shader (CG_AddFragment, freeze tag)
=================
*/
void CG_AddFragmentHideFrozen( localEntity_t *le ) {
	qhandle_t	shader;

	shader = cgs.media.freezeShader;
	if ( le->refEntity.customShader == shader ) {
		le->refEntity.customShader = 0;
		trap_R_AddRefEntityToScene( &le->refEntity );
		le->refEntity.customShader = shader;
	}
}
