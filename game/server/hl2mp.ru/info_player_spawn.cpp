#include "cbase.h"
#include "info_player_spawn.h"

LINK_ENTITY_TO_CLASS( info_player_coop, CHLSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_deathmatch, CHLSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_start, CHLSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_combine, CHLSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_rebel, CHLSpawnPoint );

BEGIN_DATADESC( CHLSpawnPoint )
	DEFINE_KEYFIELD( m_iDisabled, FIELD_INTEGER, "StartDisabled" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

void CHLSpawnPoint::InputEnable( inputdata_t &inputdata )
{
	//if( m_iDisabled == FALSE )
	//	return;
	//else
		m_iDisabled = FALSE;

	CBaseEntity *pSpawn = NULL;
	while( ( pSpawn = gEntList.FindEntityByClassname( pSpawn, GetClassname() ) ) != NULL )
	{
		CHLSpawnPoint *pSpot = dynamic_cast<CHLSpawnPoint *>( pSpawn );
		if( pSpot && pSpot != this )
			pSpot->m_iDisabled = TRUE;
	}
}

void CHLSpawnPoint::InputDisable( inputdata_t &inputdata )
{
	m_iDisabled = TRUE;
}