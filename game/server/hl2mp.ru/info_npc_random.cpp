#include "cbase.h"
#include "ai_basenpc.h"
#include "mapentities.h"
#include "te_effect_dispatch.h"
#include "ai_moveprobe.h"
#include "hl2mp_player.h"
#include "ai_network.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "ai_node.h"

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
	bool FindRandomSpot( Vector *pResult );
	bool CheckSpotForRadius( Vector *pResult, const Vector &vStartPos, CAI_BaseNPC *pNPC, float radius );

	KeyValues *manifest;
	KeyValues *sub;

	bool m_bNextSub;
};

LINK_ENTITY_TO_CLASS( info_npc_random, npcRandom );

BEGIN_DATADESC( npcRandom )

	DEFINE_THINKFUNC( TimerThink ),

END_DATADESC()

void npcRandom::Spawn( void )
{
	SetThink( &npcRandom::TimerThink );
	SetNextThink( gpGlobals->curtime + 1.0 );
	m_bNextSub = false;
}

void CC_mp_rndnpc_kill( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	engine->ServerCommand( "mp_rndnpc 0" );
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if( ent->GetOwnerEntity() && ent->GetOwnerEntity()->ClassMatches( "info_npc_random" ) )
			ent->TakeDamage( CTakeDamageInfo( ent, ent, ent->GetHealth()+100, DMG_GENERIC ) );
	}
}

static ConCommand mp_rndnpc_kill( "mp_rndnpc_kill", CC_mp_rndnpc_kill, "", FCVAR_GAMEDLL );

static const char *RandomNpc[] = 
{
	"npc_fastzombie",
	"npc_zombie",
	"npc_poisonzombie",
	"npc_antlion",
	"npc_manhack",
	"npc_vortigaunt",
	"npc_headcrab",
	"npc_headcrab_fast",
	"npc_headcrab_black",
};

ConVar mp_rndnpc( "mp_rndnpc", "0" );
ConVar mp_rndnpc_maxnpc( "mp_rndnpc_maxnpc", "60" );
ConVar mp_rndnpc_visible( "mp_rndnpc_visible", "0" );

void npcRandom::TimerThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.5 );

	if( !mp_rndnpc.GetBool() )
		return;

	int maxnpc = 0;
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if( ent->GetOwnerEntity() == this )
			maxnpc++;
	}

	if( mp_rndnpc_maxnpc.GetInt() <= maxnpc )
		return;

	const char *classname = NULL;
	const char *skin = NULL;
	const char *additionalequipment = NULL;
	const char *model = NULL;
	int maxonmap = 0;
	char szFullName[512];
	Q_snprintf(szFullName,sizeof(szFullName), "maps/%s_rndnpc.txt", STRING( gpGlobals->mapname) );
	if( manifest == NULL ) {
		manifest = new KeyValues("EntityList");
		manifest->LoadFromFile(filesystem, szFullName, "GAME");
		sub = manifest->GetFirstSubKey();
	}

	if( sub == NULL )
		sub = manifest->GetFirstSubKey();

	if( sub != NULL ) {
		m_bNextSub = true;
		classname = sub->GetString("classname");
		skin = sub->GetString("skin");
		additionalequipment = sub->GetString("additionalequipment");
		model = sub->GetString("model");
		pfriend = sub->GetBool("friend");
		maxonmap = sub->GetInt("maxonmap");
		sub = sub->GetNextKey();
	}

	if( classname == NULL && !m_bNextSub ) {
		int nHeads = ARRAYSIZE( RandomNpc );
		classname = RandomNpc[rand() % nHeads];
	}

	if( maxonmap != 0 ) {
		maxnpc = 0;
		ent = NULL;
		while ( (ent = gEntList.NextEnt(ent)) != NULL )
		{
			if( ent->GetOwnerEntity() == this && FClassnameIs(ent,classname) )
				maxnpc++;
		}
		if(maxnpc >= maxonmap)
			return;
	}

	CAI_BaseNPC *pNPC = dynamic_cast< CAI_BaseNPC * >( CreateEntityByName( classname ) );
	if( pNPC ) {
		Vector pSpot;
		if( FindRandomSpot( &pSpot ) ) {
			pNPC->SetAbsOrigin( pSpot );
			pNPC->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );
			if( skin != NULL )
				pNPC->KeyValue("skin",skin);

			if( additionalequipment != NULL )
				pNPC->KeyValue("additionalequipment",additionalequipment);

			if( model != NULL )
				pNPC->KeyValue("model",model);

			pNPC->pfriend = pfriend;
			DispatchSpawn( pNPC );
			pNPC->SetOwnerEntity( this );
			DispatchActivate( pNPC );
			
			if( CheckSpotForRadius( &pSpot, pSpot + Vector( 0, 0, 5 ), pNPC, pNPC->GetHullWidth() * 1.5) ) {
				/*CBaseEntity *pEntity = CreateEntityByName( "prop_dynamic" );
				pEntity->KeyValue("model", "models/editor/ground_node.mdl");
				pEntity->SetAbsOrigin( pSpot );
				DispatchSpawn(pEntity);*/
				pNPC->SetAbsOrigin( pSpot );
				Msg("info_npc_random: Spawn %s\n",classname);
				return;
			}
		}
		UTIL_Remove( pNPC );
	}
}

const char *gpPointListSpawn[] = {
	"info_player_coop",
	"info_player_deathmatch",
	"info_player_start",
	"info_player_combine",
	"info_player_rebel"
};

bool npcRandom::FindRandomSpot( Vector *pResult )
{
	//info_node spot
	int rndspawn = random->RandomInt(1, g_pBigAINet->NumNodes());
	CAI_Node *pNode = g_pBigAINet->GetNode( rndspawn );

	if( pNode != NULL && pNode->GetType() == NODE_GROUND ) {
		int nHeads = ARRAYSIZE( gpPointListSpawn );
		int i;
		for ( i = 0; i < nHeads; ++i )
		{
			CBaseEntity *pSpawn = NULL;
			while ( ( pSpawn = gEntList.FindEntityByClassname( pSpawn, gpPointListSpawn[i] ) ) != NULL )
			{
				float flDist = ( pSpawn->GetAbsOrigin() - pNode->GetOrigin() ).LengthSqr();

				if( sqrt( flDist ) < 256.0 )
					return false;
			}
		}

		if( !mp_rndnpc_visible.GetBool() ) {
			*pResult = pNode->GetOrigin();
			return true;
		}

		for (int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient)
		{
			CBasePlayer *pEnt = UTIL_PlayerByIndex(iClient);
			if (!pEnt || pEnt->IsDisconnecting() || !pEnt->IsAlive() || pEnt->IsFakeClient() )
				continue;

			//if( pEnt->FVisible(pNode->GetOrigin() + Vector( 0.0, 0.0, 64.0 ), MASK_SHOT) )
			//	return false;
			if( pEnt->FInViewCone( pNode->GetOrigin() ) && pEnt->FVisible( pNode->GetOrigin() ) )
				return false;
		}
		*pResult = pNode->GetOrigin();
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
	if( tr.fraction != 1.0 && !tr.startsolid && strcmp("**studio**", tr.surface.name) != 0 )
		vStartPos = tr.endpos;
	else
		return false;

	for( fan.y = 0 ; fan.y < 360 ; fan.y += 18.0 )
	{
		Vector vecTest;
		Vector vecDir;

		AngleVectors( fan, &vecDir );

		vecTest = vStartPos + vecDir * radius;

		UTIL_TraceLine( vStartPos, vecTest, MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );
		if( tr.fraction != 1.0 )
			return false;
			//UTIL_TraceLine( tr.endpos, tr.endpos - Vector( 0, 0, 10 ), MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );
		else
			UTIL_TraceLine( vecTest, vecTest - Vector( 0, 0, 10 ), MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );

		if( tr.fraction == 1.0 )
			return false;
	}

	UTIL_TraceHull( vStartPos,
						vStartPos + Vector( 0, 0, 5 ),
						pNPC->GetHullMins(),
						pNPC->GetHullMaxs(),
						MASK_NPCSOLID,
						pNPC,
						COLLISION_GROUP_NONE,
						&tr );

	if( tr.fraction == 1.0 && pNPC->GetMoveProbe()->CheckStandPosition( tr.endpos, MASK_NPCSOLID ) )
	{
		*pResult = tr.endpos;
		return true;
	}
	return false;
}