#include "cbase.h"
#include "info_player_spawn.h"
#include "te_effect_dispatch.h"

class iph : public CBaseEntity
{
public:
	DECLARE_CLASS( iph, CBaseEntity );
	DECLARE_DATADESC();

	void Spawn( void );
	void InputMovePlayers( inputdata_t &inputdata );
	void TimerThink( void );
	CBasePlayer *GetNearestVisiblePlayer( void );
	void TpAllPlayer( const Vector newPosition, const QAngle newAngles, CBasePlayer *ignore = NULL );
	void DisablePoint( CBaseEntity *pIgnore );
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	bool m_bTpAll;
	float m_fRadius;
	int	m_iDisabled;
};

LINK_ENTITY_TO_CLASS( info_player_hl2mpru, iph );

BEGIN_DATADESC( iph )

	DEFINE_THINKFUNC( TimerThink ),
	DEFINE_KEYFIELD( m_bTpAll, FIELD_BOOLEAN, "tpall" ),
	DEFINE_KEYFIELD( m_fRadius, FIELD_FLOAT, "radius" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "MovePlayers", InputMovePlayers ),
	DEFINE_KEYFIELD( m_iDisabled, FIELD_INTEGER, "StartDisabled" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

END_DATADESC()

void iph::Spawn( void ) 
{
	if( m_fRadius == 0.0 )
		m_fRadius = 512.0;

	SetThink( &iph::TimerThink );
	SetNextThink( gpGlobals->curtime + 0.05 );
}

void iph::InputMovePlayers( inputdata_t &inputdata ) 
{
	Vector origin = GetAbsOrigin();
	origin[2] += 8.0;
	TpAllPlayer( origin, GetAbsAngles() );
}

void iph::TimerThink( void )
{
	CBasePlayer *pPlayer = GetNearestVisiblePlayer();
	if( pPlayer && pPlayer->IsAlive() && !m_iDisabled ) {
		CSpawnPoint *pSpot = dynamic_cast<CSpawnPoint *>( CreateEntityByName("info_player_coop") );
		if( pSpot ) {
			pSpot->SetAbsOrigin( GetAbsOrigin() );
			pSpot->SetAbsAngles( GetAbsAngles() );
			pSpot->m_iDisabled = false;
			pSpot->Activate();
			DisablePoint( pSpot );
			if( m_bTpAll ) {
				TpAllPlayer( GetAbsOrigin(), GetAbsAngles(), pPlayer );
				engine->ServerCommand( "sm_savetp_clearpoint\n");
			}
			if( GetParent() )
				pSpot->SetParent( GetParent() );
			else {
				trace_t tr;
				UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 10.0 ), MASK_ALL, pPlayer, COLLISION_GROUP_NONE, &tr);
				if( tr.fraction != 1.0 && tr.m_pEnt ) {
					if( tr.m_pEnt->GetParent() )
						pSpot->SetParent( tr.m_pEnt->GetParent() );
					else
						pSpot->SetParent( tr.m_pEnt );
				}
			}
			Remove();
			return;
		}
	}
	SetNextThink( gpGlobals->curtime + 0.05 );
}

CBasePlayer *iph::GetNearestVisiblePlayer( void )
{
	Vector pos = GetAbsOrigin();
	pos[2] += 32.0;

	CBasePlayer *pPlayer = UTIL_GetNearestPlayer( pos );

	if( pPlayer ) {
		float flDist = ( pPlayer->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();

		if( sqrt( flDist ) < m_fRadius && FVisible( pPlayer, MASK_SOLID_BRUSHONLY ) )
			return pPlayer;
	}

	return NULL;
}

void iph::TpAllPlayer( const Vector newPosition, const QAngle newAngles, CBasePlayer *ignore  )
{
	for( int i=1;i<=gpGlobals->maxClients;i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
		if( pPlayer && (ignore != pPlayer) && pPlayer->IsConnected() && pPlayer->IsAlive() && !pPlayer->IsInAVehicle() ) {
			pPlayer->Teleport( &newPosition, &newAngles, NULL );
		}
	}
}

const char *gpPointList[] = {
	"info_player_coop",
	"info_player_deathmatch",
	"info_player_start",
	"info_player_combine",
	"info_player_rebel",
};

void iph::DisablePoint( CBaseEntity *pIgnore ) 
{
	int nHeads = ARRAYSIZE( gpPointList );
	int i;
	for ( i = 0; i < nHeads; ++i )
	{
		CBaseEntity *pSpawn = NULL;
		while ( ( pSpawn = gEntList.FindEntityByClassname( pSpawn, gpPointList[i] ) ) != NULL )
		{
			CSpawnPoint *pSpot = dynamic_cast<CSpawnPoint *>( pSpawn );
			if( pSpot && ( pSpot != pIgnore ) ) {
				UTIL_Remove( pSpot );
			}
		}
	}
}

void iph::InputEnable( inputdata_t &inputdata )
{
	m_iDisabled = FALSE;
}

void iph::InputDisable( inputdata_t &inputdata )
{
	m_iDisabled = TRUE;
}