#include "cbase.h"
#include "triggers.h"

class tpc : public CBaseTrigger
{
public:
	DECLARE_CLASS( tpc, CBaseTrigger );
	DECLARE_DATADESC();
	void Spawn( void );
	void StartTouch(CBaseEntity *pOther);
	void EndTouch(CBaseEntity *pOther);
	void OnThink( void );

protected:
	COutputEvent m_OnAllPlayersEntered;
	COutputEvent m_OnPlayersIn;
	int m_iPlayersIn;
	int m_iPlayersInGame;
	int m_iPlayerValue;
	bool m_bUseHUD;
	bool m_iPlayerIn[MAX_PLAYERS];
};

LINK_ENTITY_TO_CLASS( trigger_player_count, tpc );
LINK_ENTITY_TO_CLASS( trigger_coop, tpc );

BEGIN_DATADESC( tpc )
	
	DEFINE_THINKFUNC( OnThink ),
	DEFINE_KEYFIELD( m_iPlayerValue, FIELD_INTEGER, "PlayerValue" ),
	DEFINE_KEYFIELD( m_bUseHUD, FIELD_INTEGER, "UseHUD" ),
	//Obsidian
	DEFINE_OUTPUT( m_OnAllPlayersEntered, "OnAllPlayersEntered"),
	//Synergy
	DEFINE_OUTPUT( m_OnPlayersIn, "OnPlayersIn"),

END_DATADESC()

void tpc::Spawn( void )
{
	BaseClass::Spawn();
	InitTrigger();
	SetThink( &tpc::OnThink );
	SetNextThink( gpGlobals->curtime + 0.05 );

	if( m_iPlayerValue == 0 )
		m_iPlayerValue = 100;
}

void tpc::StartTouch(CBaseEntity *pOther)
{
	if( !pOther || !pOther->IsPlayer() )
		return;

	m_iPlayerIn[pOther->entindex()] = true;
	m_iPlayersIn++;
	float p = m_iPlayersIn / (m_iPlayersInGame / 100.0);
	int result = floor(p);

	if( result >= m_iPlayerValue ) {
		m_OnAllPlayersEntered.FireOutput(pOther, this);
		m_OnPlayersIn.FireOutput(pOther, this);
	}
}

void tpc::EndTouch(CBaseEntity *pOther)
{
	if( !pOther || !pOther->IsPlayer() )
		return;

	m_iPlayerIn[pOther->entindex()] = false;
	m_iPlayersIn--;
}

void tpc::OnThink( void )
{
	int playeronline = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
		if ( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() ) {
			if( m_iPlayerIn[i] && m_bUseHUD ) {
				ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "Player in %d/%d", m_iPlayersIn, m_iPlayersInGame) );
			}
			playeronline++;
		}
	}
	
	m_iPlayersInGame = playeronline;

	SetNextThink( gpGlobals->curtime + 0.05 );
}