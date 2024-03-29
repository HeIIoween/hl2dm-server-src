#include "cbase.h"
#include "Sprite.h"
#include "npc_playercompanion.h"
#include "viewport_panel_names.h"
#include "te_effect_dispatch.h"

class CNPC_merchant : public CNPC_PlayerCompanion
{
public:
	DECLARE_CLASS( CNPC_merchant, CNPC_PlayerCompanion );

	void Spawn( void );
	void SelectModel();
	void Precache( void );
	bool IgnorePlayerPushing( void ) { return true; };
	Class_T Classify ( void );
	bool ShouldLookForBetterWeapon() { return false; } 
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	CHandle<CSprite> m_hSprite;
};

LINK_ENTITY_TO_CLASS( npc_merchant, CNPC_merchant );

Class_T	CNPC_merchant::Classify ( void )
{
	if( GetActiveWeapon() )
		return CLASS_PLAYER_ALLY;

	return	CLASS_NONE;
}

ConVar sk_merchant_health("sk_merchant_health","100");

//=========================================================
// Spawn
//=========================================================
void CNPC_merchant::Spawn()
{
	BaseClass::Spawn();
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetUse( &CNPC_merchant::Use );

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_DOORS_GROUP | bits_CAP_TURN_HEAD | bits_CAP_DUCK | bits_CAP_SQUAD );
	CapabilitiesAdd( bits_CAP_USE_WEAPONS );
	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_FRIENDLY_DMG_IMMUNE );
	CapabilitiesAdd( bits_CAP_AIM_GUN );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_USE_SHOT_REGULATOR );

	m_iHealth = sk_merchant_health.GetInt();

	NPCInit();
	Vector origin = EyePosition();
	origin.z += 10.0;
	m_hSprite = CSprite::SpriteCreate( "hl2mp.ru/merchant.vmt", origin, false );
	if ( m_hSprite ) {
		m_hSprite->SetRenderMode( kRenderGlow );
		m_hSprite->SetScale( 0.2f );
		m_hSprite->SetGlowProxySize( 4.0f );
		m_hSprite->SetParent( this );
		m_hSprite->TurnOn();
	}

	int iAttachment = LookupBone( "ValveBiped.Bip01_R_Hand" );
	if( GetActiveWeapon() && iAttachment > 0 ) {
		SetCollisionGroup( COLLISION_GROUP_PLAYER );
	}
	else {
		if( GetActiveWeapon() )
			Weapon_Drop( GetActiveWeapon() );
		AddEFlags( EFL_NO_DISSOLVE );
		m_takedamage = DAMAGE_NO;
		AddSpawnFlags( SF_NPC_WAIT_FOR_SCRIPT );
		CapabilitiesClear();
	}
	UTIL_SayTextAll("[Shop] The merchant on the map.");
}

void CNPC_merchant::Precache()
{
	BaseClass::Precache();
	PrecacheModel( STRING( GetModelName() ) );
	PrecacheParticleSystem("Weapon_Combine_Ion_Cannon_Explosion");
	PrecacheParticleSystem("advisor_object_charge");
}

void CNPC_merchant::SelectModel()
{
	const char *szModel = STRING( GetModelName() );
	if (!szModel || !*szModel)
		SetModelName( AllocPooledString("models/zombie/classic.mdl") );
}

ConVar sv_merchant_url("sv_merchant_url","http://hl2mp.ru/motd/buy/27400/new/");
ConVar sv_merchant_url_hide("sv_merchant_url_hide","0");
ConVar sv_merchant_url_unload("sv_merchant_url_unload","1");

void openshop( CBasePlayer *player )
{
	const char *szLanguage = engine->GetClientConVarValue( engine->IndexOfEdict( player->edict() ), "cl_language" );
	char msg[100];
	Q_snprintf( msg, sizeof(msg), "%s/index.php?key=%d&userid=%d&language=%s", sv_merchant_url.GetString(), (int)player, player->GetUserID(), szLanguage );
	KeyValues *data = new KeyValues("data");
	data->SetString( "title", "Merchant: Welcome" );		// info panel title
	data->SetString( "type", "2" );			// show userdata from stringtable entry
	data->SetString( "msg",	msg );		// use this stringtable entry
	data->SetBool( "unload", sv_merchant_url_unload.GetBool() );
	player->ShowViewPortPanel( PANEL_INFO, !sv_merchant_url_hide.GetBool(), data );
	data->deleteThis();
}

void CNPC_merchant::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;
	if( pPlayer )
		openshop( pPlayer );
}

void CC_openshop( const CCommand& args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( pPlayer )
		openshop( pPlayer );
}

static ConCommand merchant("merchant", CC_openshop, "Force open merchant menu!", FCVAR_GAMEDLL );

void CC_sv_merchant_give( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *modify = args.Arg(1)+1;
	int userid = atoi(modify);
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if( !player )
		return;

	const char *classname = args.Arg(2);
	if ( strncmp( classname, "weapon_", 7 ) == 0 ) {
		CBaseEntity *item = player->GiveNamedItem( classname );
		if( !item )
			engine->ServerCommand( "echo Merchant_0: Already own this weapon!\n");
		else {
			item->AddSpawnFlags( SF_NPC_NO_WEAPON_DROP );
			for (int i=0; i<MAX_WEAPONS; ++i) 
			{
				CBaseCombatWeapon *pWeapon = player->GetWeapon(i);
				if (!pWeapon)
					continue;

				bool pEmpty = !pWeapon->HasAmmo();
				if( !pEmpty && FClassnameIs(pWeapon, classname) ) {
					player->Weapon_Switch(pWeapon);
					break;
				}
			}
		}
	}
	else {
		int count = atoi(args.Arg(3));
		do {
			CBaseEntity *item = player->GiveNamedItem(classname);
			if( item && !item->GetOwnerEntity() )
				item->Remove();
		}
		while(count--);
	}
}

static ConCommand sv_merchant_give( "sv_merchant_give", CC_sv_merchant_give, "", FCVAR_GAMEDLL );

void CC_sv_merchant_client_info( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if( strcmpi(args.Arg(1),"@all" ) == 0 ) {
		for( int i=1; i<=gpGlobals->maxClients; i++ )
		{
			CBasePlayer *player = UTIL_PlayerByIndex( i );
			if( player )
				engine->ServerCommand( UTIL_VarArgs( "echo pName\necho \"%s\"\necho pUserId\necho %d\n",player->GetPlayerName(),player->GetUserID() ) );
		}
		return;
	}

	int userid = atoi( args.Arg( 1 ) );
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if( !player )
		return;
	
	engine->ServerCommand( UTIL_VarArgs( "echo pName\necho \"%s\"\necho pFrag\necho %d\necho pAdmin\necho %d\necho pKey\necho %d\necho pTeam\necho %d\n",player->GetPlayerName(),player->FragCount(),1,(int)player, player->GetTeamNumber() ) );
}

static ConCommand sv_merchant_client_info( "sv_merchant_client_info", CC_sv_merchant_client_info, "", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE );

void CC_sv_merchant_decrement_frag( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int frag = atoi(args.Arg(2));
	int userid = atoi(args.Arg(1));
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if(player)
		player->IncrementFragCount(-frag);
}

static ConCommand sv_merchant_decrement_frag( "sv_merchant_decrement_frag", CC_sv_merchant_decrement_frag, "", FCVAR_GAMEDLL );

void CC_sv_merchant_give_npc( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	const char *modify = args.Arg(1)+1;
	int userid = atoi(modify);
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if( !player )
		return;

	const char *classname = args.Arg(2);
	int limit = atoi(args.Arg(3));
	const char *weapon = args.Arg(4);
	int health = atoi(args.Arg(5));

	if( !health )
		health = 1000;

	if( classname ) {
		CBaseEntity *ent = NULL;
		int maxnpc = 0;
		while ( (ent = gEntList.NextEnt(ent)) != NULL )
		{
			if ((ent->m_iClassname != NULL_STRING	&& FStrEq(classname, STRING(ent->m_iClassname))) ||
					(ent->GetClassname()!=NULL && FStrEq(classname, ent->GetClassname())))
			{
				if (ent->GetPlayerMP() == player)
					maxnpc++;

				if(maxnpc >= limit && limit != 0) {
					engine->ServerCommand( "echo Merchant_0: You have reached the limit!\n");
					return;
				}
			}
		}
		CBaseEntity *entity = CreateEntityByName( classname );
		if ( entity != NULL )
		{
			entity->SetLocalOrigin( player->GetAbsOrigin() );
			entity->SetCollisionGroup( COLLISION_GROUP_PLAYER );
			entity->SetPlayerMP( player );
			if( weapon )
				entity->KeyValue( "additionalequipment", weapon );

			DispatchSpawn( entity );
			entity->Activate();
			DispatchParticleEffect( "Weapon_Combine_Ion_Cannon_Explosion", entity->GetAbsOrigin()+Vector(0.0, 0.0, 10.0), vec3_angle );
			DispatchParticleEffect( "advisor_object_charge", entity->GetAbsOrigin()+Vector(0.0, 0.0, 32.0), vec3_angle );
			INPCInteractive *pInteractive = dynamic_cast<INPCInteractive *>( entity );
			if( pInteractive )
				pInteractive->NotifyInteraction( NULL );

			if( entity->IsNPC() ) {
				entity->SetHealth( health );
				entity->SetMaxHealth( health );
				entity->m_takedamage = DAMAGE_YES;
				entity->AddEFlags( EFL_NO_DISSOLVE );
			}
			engine->ServerCommand( "echo Merchant_1: Full create!\n");
		}
		else
			engine->ServerCommand( "echo Merchant_0: Not create!\n");
	}
}

static ConCommand sv_merchant_give_npc( "sv_merchant_give_npc", CC_sv_merchant_give_npc, "", FCVAR_GAMEDLL );

void CC_sv_merchant_tp_npc( const CCommand &args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	bool ready = false;
	CBaseEntity *ent = NULL;
	while ( (ent = gEntList.NextEnt(ent)) != NULL )
	{
		if (ent->GetPlayerMP() && (ent->GetPlayerMP() == pPlayer) ) {
			DispatchParticleEffect( "Weapon_Combine_Ion_Cannon_Explosion", ent->GetAbsOrigin()+Vector(0.0, 0.0, 10.0), vec3_angle );
			DispatchParticleEffect( "advisor_object_charge", ent->GetAbsOrigin()+Vector(0.0, 0.0, 32.0), vec3_angle );
			ent->Teleport(&pPlayer->GetAbsOrigin(),&pPlayer->GetAbsAngles(),&vec3_origin);
			ready = true;
		}
	}
	if( !ready ) {
		UTIL_SayText("[info] Your npc/things not found!\n",pPlayer);
		return;
	}
	DispatchParticleEffect( "Weapon_Combine_Ion_Cannon_Explosion", pPlayer->GetAbsOrigin()+Vector(0.0, 0.0, 10.0), vec3_angle );
	DispatchParticleEffect( "advisor_object_charge", pPlayer->GetAbsOrigin()+Vector(0.0, 0.0, 32.0), vec3_angle );
	UTIL_SayText("[info] Your npc/things teleport!\n",pPlayer);
}

static ConCommand sv_merchant_tp_npc( "sv_merchant_tp_npc", CC_sv_merchant_tp_npc, "", FCVAR_GAMEDLL );
