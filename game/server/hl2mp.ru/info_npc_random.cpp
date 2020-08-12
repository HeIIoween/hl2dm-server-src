#include "cbase.h"
#include "ai_basenpc.h"
#include "mapentities.h"
#include "te_effect_dispatch.h"
#include "ai_moveprobe.h"
#include "player.h"
#include "ai_network.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "ai_node.h"
#include "eventqueue.h"

class spawnnode : public CBaseEntity
{
	DECLARE_CLASS( spawnnode, CBaseEntity );
};

LINK_ENTITY_TO_CLASS( info_spawnnode, spawnnode );

static void DispatchActivate( CBaseEntity *pEntity )
{
	bool bAsyncAnims = mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, false );
	pEntity->Activate();
	mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, bAsyncAnims );
}

class npcRandom : public CBaseEntity
{
	DECLARE_CLASS( npcRandom, CBaseEntity );
	DECLARE_DATADESC();
public:
	void Spawn( void );
	void TimerThink( void );
	bool FindRandomSpot( Vector *pResult, QAngle *pAngle );
	bool ItSafeZone( Vector pSpot );
	bool CheckSpotForRadius( Vector *pResult, const Vector &pSpot, CAI_BaseNPC *pNPC, float radius );
	void EffectSpawn( Vector vStartPost, CAI_BaseNPC *pNPC = NULL );
	
	KeyValues *manifest;
	KeyValues *sub;
	KeyValues *safezone;

	CHandle<CAI_BaseNPC> m_hLastNPC;
	float m_flRestoreRender;
	int m_nCurrentNode;
	float m_flWaveTime;
	float m_flWaveInterval;
	EHANDLE m_pLastPoint;
};

LINK_ENTITY_TO_CLASS( info_npc_random, npcRandom );

BEGIN_DATADESC( npcRandom )
	DEFINE_THINKFUNC( TimerThink ),
END_DATADESC()

void npcRandom::Spawn( void )
{
	SetThink( &npcRandom::TimerThink );
	SetNextThink( gpGlobals->curtime + 1.0 );

	m_nCurrentNode = 0;

	manifest = new KeyValues("EntityList");
	char szFullName[512];
	Q_snprintf(szFullName,sizeof(szFullName), "maps/%s_rndnpc.txt", STRING( gpGlobals->mapname) );
	manifest->LoadFromFile(filesystem, szFullName, "GAME");

	if( manifest->GetFirstSubKey() == NULL )
		manifest->LoadFromFile(filesystem, "rndnpc.txt", "GAME");

	if( manifest->GetFirstSubKey() == NULL )
		Remove();

	safezone = manifest->FindKey( "safezone" );
	if( safezone != NULL )
		manifest->RemoveSubKey( safezone );

	KeyValues *settings = manifest->FindKey( "settings" );
	if( settings != NULL ) {
		manifest->RemoveSubKey( settings );
		const char *cmd;
		if( settings->GetInt("enable") ) {
			cmd = UTIL_VarArgs("mp_rndnpc %d\n", settings->GetInt("enable") );
			engine->ServerCommand(cmd);
		}
		if( settings->FindKey("maxnpc") ) {
			cmd = UTIL_VarArgs("mp_rndnpc_maxnpc %d\n", settings->GetInt("maxnpc") );
			engine->ServerCommand(cmd);
		}
		if( settings->FindKey("visible") ) {
			cmd = UTIL_VarArgs("mp_rndnpc_visible %d\n", settings->GetInt("visible") );
			engine->ServerCommand(cmd);
		}
		if( settings->FindKey("randomspot") ) {
			cmd = UTIL_VarArgs("mp_rndnpc_randomspot %d\n", settings->GetInt("randomspot") );
			engine->ServerCommand(cmd);
		}
		if( settings->FindKey("wavetime") ) {
			cmd = UTIL_VarArgs("mp_rndnpc_wavetime %d\n", settings->GetInt("wavetime") );
			engine->ServerCommand(cmd);
		}
		if( settings->FindKey("waveinterval") ) {
			cmd = UTIL_VarArgs("mp_rndnpc_waveinterval %d\n", settings->GetInt("waveinterval") );
			engine->ServerCommand(cmd);
		}
		settings->deleteThis();
	}
	PrecacheSound("hl2mp.ru/rndspawn.wav");
}

void CC_mp_rndnpc_kill( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if( ent->GetOwnerEntity() && ent->GetOwnerEntity()->ClassMatches( "info_npc_random" ) )
			ent->TakeDamage( CTakeDamageInfo( ent, ent, ent->GetHealth()+100, DMG_GENERIC ) );
	}
}

static ConCommand mp_rndnpc_kill( "mp_rndnpc_kill", CC_mp_rndnpc_kill, "", FCVAR_GAMEDLL );

ConVar mp_rndnpc( "mp_rndnpc", "0" );
ConVar mp_rndnpc_maxnpc( "mp_rndnpc_maxnpc", "50" );
ConVar mp_rndnpc_visible( "mp_rndnpc_visible", "1" );
ConVar mp_rndnpc_randomspot( "mp_rndnpc_randomspot", "1" );
ConVar mp_rndnpc_wavetime( "mp_rndnpc_wavetime", "300" );
ConVar mp_rndnpc_waveinterval( "mp_rndnpc_waveinterval", "120" );

void npcRandom::TimerThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.25 );

	if( m_hLastNPC != NULL && m_hLastNPC->IsAlive() ) {
		if( m_flRestoreRender <= gpGlobals->curtime ) {
			m_hLastNPC->m_nRenderFX = 0;
			m_hLastNPC = NULL;
		}
		else if ( m_hLastNPC->GetEffects() & EF_NODRAW ) {
			CBaseCombatWeapon *pWeapon = m_hLastNPC->GetActiveWeapon();
			if( pWeapon )
				pWeapon->RemoveEffects( EF_NODRAW );

			m_hLastNPC->RemoveEffects( EF_NODRAW );
			m_hLastNPC->m_nRenderFX = 15;
		}

		if( m_hLastNPC != NULL )
			return;
	}

	if( !mp_rndnpc.GetBool() )
		return;

	if( m_flWaveTime == 0.0 )
		m_flWaveTime = gpGlobals->curtime + mp_rndnpc_wavetime.GetFloat();

	if( m_flWaveInterval > gpGlobals->curtime ) {
		m_flWaveTime = gpGlobals->curtime + mp_rndnpc_wavetime.GetFloat();
		return;
	}

	if( m_flWaveTime < gpGlobals->curtime )
		m_flWaveInterval = gpGlobals->curtime + mp_rndnpc_waveinterval.GetFloat();

	int maxnpc = 0;
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
		if( ent->GetOwnerEntity() == this )
			maxnpc++;

	if( mp_rndnpc_maxnpc.GetInt() <= maxnpc )
		return;

	if( sub == NULL )
		sub = manifest->GetFirstSubKey();

	if( sub == NULL ) {
		Msg("ERROR info_npc_random sub=null!\n");
		Remove();
	}

	if( sub->FindKey("maxonmap") ) {
		maxnpc = 0;
		ent = NULL;
		while ( (ent = gEntList.NextEnt(ent)) != NULL )
			if( ent->GetOwnerEntity() == this && FClassnameIs(ent,sub->GetString("classname")) )
				maxnpc++;

		if( maxnpc >= sub->GetInt("maxonmap") ) {
			sub = sub->GetNextKey();
			return;
		}
	}

	Vector pSpot;
	QAngle pAngle = vec3_angle;
	if( !FindRandomSpot( &pSpot, &pAngle ) || ItSafeZone( pSpot ) )
		return;

	CAI_BaseNPC *pNPC = dynamic_cast< CAI_BaseNPC * >( CreateEntityByName( sub->GetString("classname") ) );
	if( pNPC == NULL )
		return;

	pNPC->SetAbsOrigin( pSpot );
	pNPC->SetAbsAngles( pAngle );
	pNPC->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );
	if( sub->FindKey( "skin") )
		pNPC->KeyValue( "skin",sub->GetString("skin") );

	if( sub->FindKey("additionalequipment") )
		pNPC->KeyValue( "additionalequipment", sub->GetString("additionalequipment") );

	if( sub->FindKey("model") )
		pNPC->KeyValue( "model", sub->GetString("model") );

	pNPC->pfriend = sub->GetBool("friend");
	pNPC->AddSpawnFlags( SF_NPC_FADE_CORPSE );

	DispatchSpawn( pNPC );
	pNPC->SetOwnerEntity( this );

	DispatchActivate( pNPC );

	pNPC->CapabilitiesAdd( bits_CAP_MOVE_JUMP );

	CBaseCombatWeapon *pWeapon = pNPC->GetActiveWeapon();
	if( pWeapon )
		pWeapon->AddEffects( EF_NODRAW );

	pNPC->AddEffects( EF_NODRAW );

	if( CheckSpotForRadius( &pSpot, pSpot, pNPC, pNPC->GetHullWidth() * 1.5) ) {
		m_hLastNPC = pNPC;
		m_flRestoreRender = gpGlobals->curtime + 1.5;
		pNPC->SetAbsOrigin( pSpot );
		EffectSpawn( pSpot, pNPC );
		Msg("info_npc_random: Spawn: %s NodeID: %d\n",sub->GetString("classname"),m_nCurrentNode);
		sub = sub->GetNextKey();
		SetNextThink( gpGlobals->curtime + 0.5 );
		return;
	}
	UTIL_Remove( pNPC );
}

const char *gpPointListSpawn[] = {
	"info_player_coop",
	"info_player_deathmatch",
	"info_player_start",
	"info_player_combine",
	"info_player_rebel"
};

bool npcRandom::FindRandomSpot( Vector *pResult, QAngle *pAngle )
{
	bool randomspot = mp_rndnpc_randomspot.GetBool();
	m_pLastPoint = gEntList.FindEntityByClassname( m_pLastPoint, "info_spawnnode" );
	if( !m_pLastPoint )
		m_pLastPoint = gEntList.FindEntityByClassname( m_pLastPoint, "info_spawnnode" );

	if( m_pLastPoint ) {
		*pResult = m_pLastPoint->GetAbsOrigin();
		*pAngle = m_pLastPoint->GetAbsAngles();
		return true;
	}
	
	if( randomspot )
		m_nCurrentNode = random->RandomInt( 0, g_pBigAINet->NumNodes()-1 );
	else
		if( m_nCurrentNode++ >= g_pBigAINet->NumNodes() )
			m_nCurrentNode = 0;

	CAI_Node *pNode = g_pBigAINet->GetNode( m_nCurrentNode );
	if( pNode != NULL && pNode->GetType() == NODE_GROUND ) {
		*pResult = pNode->GetOrigin() + Vector( 0, 0, 16 );
		return true;
	}
	return false;
}


bool npcRandom::ItSafeZone( Vector pSpot )
{
	for ( int i = 0; i < ARRAYSIZE( gpPointListSpawn ); ++i )
	{
		CBaseEntity *pSpawn = NULL;
		while ( ( pSpawn = gEntList.FindEntityByClassname( pSpawn, gpPointListSpawn[i] ) ) != NULL )
		{
			float flDist = ( pSpawn->GetAbsOrigin() - pSpot ).LengthSqr();

			if( sqrt( flDist ) < 256.0 )
				return true;
		}
	}

	for (int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient)
	{
		CBasePlayer *pEnt = UTIL_PlayerByIndex(iClient);
		if ( !pEnt || !pEnt->IsAlive() )
			continue;

		float flDist = ( pEnt->GetAbsOrigin() - pSpot ).LengthSqr();

		if( sqrt( flDist ) < 512.0 && pEnt->FVisible( pSpot, MASK_SOLID_BRUSHONLY ) )
			return true;

		if( mp_rndnpc_visible.GetBool() && pEnt->FVisible( pSpot, MASK_SOLID_BRUSHONLY ) )
			return true;
	}

	if( safezone == NULL )
		return false;

	for ( KeyValues *sub = safezone->GetFirstSubKey(); sub != NULL ; sub = sub->GetNextKey() ) 
	{
		Vector pVector;
		UTIL_StringToVector(pVector.Base(), sub->GetString("origin") );

		float flDist = ( pSpot - pVector ).LengthSqr();

		if( sub->GetBool("visible") ) {
			trace_t tr;
			UTIL_TraceLine(pVector, pSpot, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr);
			if( tr.fraction == 1.0 )
				return true;
		}

		if( sqrt( flDist ) <= sub->GetFloat("radius") )
			return true;

	}
	return false;
}

bool npcRandom::CheckSpotForRadius( Vector *pResult, const Vector &pSpot, CAI_BaseNPC *pNPC, float radius )
{
	QAngle fan;
	Vector vStartPos;
	fan.x = 0;
	fan.z = 0;
	trace_t tr;

	UTIL_TraceLine(pSpot, pSpot - Vector( 0, 0, 8192 ), MASK_SOLID, pNPC, COLLISION_GROUP_NONE, &tr);
	if( tr.fraction != 1.0 && !tr.startsolid && strcmp("**studio**", tr.surface.name) != 0 ) {
		vStartPos = tr.endpos;
		vStartPos.z += 16.0;
	}
	else
		return false;

	for( fan.y = 0 ; fan.y < 360 ; fan.y += 90.0 )
	{
		Vector vecTest;
		Vector vecDir;

		AngleVectors( fan, &vecDir );

		vecTest = vStartPos + vecDir * radius;

		UTIL_TraceLine( vStartPos, vecTest, MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );
		if( tr.fraction != 1.0 )
			return false;
		else
			UTIL_TraceLine( vecTest, vecTest - Vector( 0, 0, 26 ), MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );

		if( tr.fraction == 1.0 )
			return false;
	}

	UTIL_TraceHull( vStartPos - Vector( 0, 0, 16 ),
						vStartPos - Vector( 0, 0, 6 ),
						pNPC->GetHullMins(),
						pNPC->GetHullMaxs(),
						MASK_NPCSOLID,
						pNPC,
						COLLISION_GROUP_NONE,
						&tr );

	if( tr.fraction == 1.0 && pNPC->GetMoveProbe()->CheckStandPosition( tr.endpos, MASK_NPCSOLID ) ) {
		*pResult = tr.endpos;
		return true;
	}
	return false;
}

void npcRandom::EffectSpawn( Vector vStartPost, CAI_BaseNPC *pNPC )
{
	Vector pOrigin = vStartPost;
	pOrigin.z +=32;
	UTIL_EmitAmbientSound( ENTINDEX( pNPC ), pOrigin, "hl2mp.ru/rndspawn.wav", 1.0, SNDLVL_80dB, 0, 100 );
	
	CBaseEntity *pBeam = CreateEntityByName( "env_beam" );
	if( pBeam ) {
		pBeam->KeyValue("origin", pOrigin);
		pBeam->KeyValue("BoltWidth", "1.8");
		pBeam->KeyValue("life", ".5");
		pBeam->KeyValue("LightningStart", int(pNPC));
		pBeam->KeyValue("NoiseAmplitude", "10.4");
		pBeam->KeyValue("Radius", "200");
		pBeam->KeyValue("renderamt", "150");
		pBeam->KeyValue("rendercolor", "0 255 0");
		pBeam->KeyValue("spawnflags", "34");
		pBeam->KeyValue("StrikeTime", "-.5");
		pBeam->KeyValue("targetname", int(pNPC));
		pBeam->KeyValue("texture", "sprites/lgtning.vmt");
		DispatchSpawn(pBeam);
		g_EventQueue.AddEvent( pBeam, "TurnOn", 0.0f, NULL, NULL ); 
		g_EventQueue.AddEvent( pBeam, "TurnOff", 1.0f, NULL, NULL );
		g_EventQueue.AddEvent( pBeam, "Kill", 2.5f, NULL, NULL );
	}

	CBaseEntity *pSprite = CreateEntityByName( "env_sprite" );
	if( pSprite ) {
		pSprite->KeyValue("origin", pOrigin);
		pSprite->KeyValue("model", "sprites/fexplo1.vmt");
		pSprite->KeyValue("rendercolor", "77 210 130");
		pSprite->KeyValue("renderfx", "14");
		pSprite->KeyValue("rendermode", "3");
		pSprite->KeyValue("spawnflags", "2");
		pSprite->KeyValue("framerate", "10");
		pSprite->KeyValue("GlowProxySize","32");
		pSprite->KeyValue("frame","18");
		DispatchSpawn(pSprite);
		g_EventQueue.AddEvent( pSprite, "ShowSprite", 0.0f, NULL, NULL );
		g_EventQueue.AddEvent( pSprite, "Kill", 2.5f, NULL, NULL );
	}

	CBaseEntity *pSprite2 = CreateEntityByName( "env_sprite" );
	if( pSprite2 ) {
		pSprite2->KeyValue("origin", pOrigin);
		pSprite2->KeyValue("model", "sprites/xflare1.vmt");
		pSprite2->KeyValue("rendercolor", "184 250 214");
		pSprite2->KeyValue("renderfx", "14");
		pSprite2->KeyValue("rendermode", "3");
		pSprite2->KeyValue("spawnflags", "2");
		pSprite2->KeyValue("framerate", "10");
		pSprite2->KeyValue("GlowProxySize","32");
		pSprite2->KeyValue("frame","19");
		DispatchSpawn(pSprite2);
		g_EventQueue.AddEvent( pSprite2, "ShowSprite", 0.0f, NULL, NULL );
		g_EventQueue.AddEvent( pSprite2, "Kill", 2.5f, NULL, NULL );
	}
}