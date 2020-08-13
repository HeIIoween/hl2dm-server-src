#include "cbase.h"
#include "info_player_spawn.h"

LINK_ENTITY_TO_CLASS( info_player_coop, CSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_deathmatch, CSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_start, CSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_combine, CSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_rebel, CSpawnPoint );

BEGIN_DATADESC( CSpawnPoint )
	DEFINE_KEYFIELD( m_iDisabled, FIELD_INTEGER, "StartDisabled" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

void CSpawnPoint::InputEnable( inputdata_t &inputdata )
{
	//if( m_iDisabled == FALSE )
	//	return;
	//else
		m_iDisabled = FALSE;

	CBaseEntity *pSpawn = NULL;
	while( ( pSpawn = gEntList.FindEntityByClassname( pSpawn, GetClassname() ) ) != NULL )
	{
		CSpawnPoint *pSpot = dynamic_cast<CSpawnPoint *>( pSpawn );
		if( pSpot && pSpot != this )
			pSpot->m_iDisabled = TRUE;
	}
}

void CSpawnPoint::InputDisable( inputdata_t &inputdata )
{
	m_iDisabled = TRUE;
}