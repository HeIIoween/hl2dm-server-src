#include "cbase.h"
#include "hl2mp_player_fix.h"
#include "vehicle_base.h"
#include "vehicle_crane.h"
#include "charge_token.h"
#include "in_buttons.h"
#include "info_player_spawn.h"
#include "func_tank.h"
#include "te_effect_dispatch.h"

LINK_ENTITY_TO_CLASS(player, CHL2MP_Player_fix);

void CHL2MP_Player_fix::Spawn(void)
{
	BaseClass::Spawn();
	m_flNextChargeTime = 0.0f;
	m_hHealerTarget = NULL;
	CBaseEntity *ent = NULL;
	while ((ent = gEntList.NextEnt(ent)) != NULL)
	{
		if (ent->GetPlayerMP() && ent->GetPlayerMP() == this && ent->GetOwnerEntity() && FClassnameIs(ent->GetOwnerEntity(), "info_vehicle_spawn")) {
			CPropVehicleDriveable *vehicle = dynamic_cast< CPropVehicleDriveable * >(ent);
			if (vehicle && vehicle->m_hPlayer) {
				vehicle->m_hPlayer = NULL;
				vehicle->SetPlayerMP( NULL );
				UTIL_Remove(vehicle);
			}
		}
	}

	m_hHealSprite = CSprite::SpriteCreate( "materials/hl2mp.ru/medic.vmt", GetAbsOrigin() + Vector( 0, 0, 75 ), false );
	if( m_hHealSprite ) {
		m_hHealSprite->SetRenderMode( kRenderTransColor );
		m_hHealSprite->SetScale( 0.3f );
		m_hHealSprite->SetGlowProxySize( 4.0f );
		m_hHealSprite->SetParent( this );
		DispatchSpawn( m_hHealSprite );
		m_hHealSprite->TurnOff();
	}
}

void CHL2MP_Player_fix::DeathNotice ( CBaseEntity *pVictim ) 
{
	if( pVictim->IsNPC() && !FClassnameIs( pVictim, "hornet" ) )
		ClientPrint( this, HUD_PRINTTALK, UTIL_VarArgs( "Your npc %s, is dead", nameReplace( pVictim ) ) );
}

ConVar sv_healer("sv_healer", "1");
ConVar sv_healer_auto("sv_healer_auto", "1");
ConVar sv_healer_charge_interval("sv_healer_charge_interval", "1.0");
ConVar sv_test_bot("sv_test_bot", "0");
ConVar sv_npchp_x("sv_npchp_x","0.01");
ConVar sv_npchp_y("sv_npchp_y","0.56");
ConVar sv_npchp("sv_npchp","1");
ConVar sv_player_pickup("sv_player_pickup", "0");

ConVar sv_weaponmenu_url("sv_weaponmenu_url","http://127.0.0.1/motd/weapon/27400");
ConVar sv_weaponmenu_url_hide("sv_weaponmenu_url_hide","0");
ConVar sv_weaponmenu_url_unload("sv_weaponmenu_url_unload","1");

#include "weapon_hl2mpbasehlmpcombatweapon.h"
void CC_sv_menuweapon_list( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int userid = atoi(args.Arg(1));
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if(!player)
		return;

	for (int i=0; i<MAX_WEAPONS; ++i) 
	{
		CBaseCombatWeapon *pWeapon = player->GetWeapon(i);
		if (!pWeapon)
			continue;
		bool pEmpty = !pWeapon->HasAmmo();
		engine->ServerCommand( UTIL_VarArgs( "echo pWeapon\necho \"%s\"\necho pEmpty\necho %d\n",pWeapon->GetClassname(),pEmpty ) );
	}
}

static ConCommand sv_menuweapon_list( "sv_menuweapon_list", CC_sv_menuweapon_list, "", FCVAR_GAMEDLL );

#define SF_NPC_NO_WEAPON_DROP ( 1 << 13 )

void CC_sv_menuweapon_equip( const CCommand &args )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int userid = atoi(args.Arg(1));
	CBasePlayer *player = UTIL_PlayerByUserId( userid );
	if( !player || !player->IsAlive() )
		return;

	const char *classname = args.Arg(2);
	
	int bSpawn = atoi( args.Arg(3) );
	
	if( bSpawn > 0 ) {
		CBaseEntity *pWeapon = player->GiveNamedItem(classname);
		if( pWeapon )
			pWeapon->AddSpawnFlags( SF_NPC_NO_WEAPON_DROP );
	}
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

static ConCommand sv_menuweapon_equip( "sv_menuweapon_equip", CC_sv_menuweapon_equip, "", FCVAR_GAMEDLL );

#include "ai_behavior_follow.h"

void CHL2MP_Player_fix::PostThink(void)
{
	BaseClass::PostThink();

	if( m_nButtons == 0 && m_fNextHealth <= gpGlobals->curtime && m_iHealth < 60  ) {
		 m_fNextHealth = gpGlobals->curtime + 2.0;
		 TakeHealth(1,DMG_GENERIC);
	}
	else if( m_nButtons != 0 ) {
		m_fNextHealth = gpGlobals->curtime + 2.0;
	}

	if( m_hHealSprite ) {
		if( (m_iHealth <= 40) && IsAlive() ) {
			m_hHealSprite->TurnOn();
		}
		else {
			m_hHealSprite->TurnOff();
		}
	}

	if( sv_test_bot.GetBool() ) {
		if( m_hBot == NULL ) {
			edict_t *pEdict = engine->CreateFakeClient( "hl2mp.ru" );
			if (pEdict)
			{
				m_hBot = ((CHL2MP_Player *)CBaseEntity::Instance( pEdict ));
				m_hBot->ClearFlags();
				m_hBot->AddFlag( FL_CLIENT | FL_FAKECLIENT );
			}
		}
	}

	if( m_StuckLast != 0 ) {
		if( m_fStuck == 0.0 )
			m_fStuck = gpGlobals->curtime + 10.0;
		
		if( m_fStuck <= gpGlobals->curtime ) {
			m_fStuck = 0.0;
			DispatchSpawn(this);
		}
		ClientPrint( this, HUD_PRINTCENTER, "You stuck, please wait 10 second!" );	
	}
	else {
		m_fStuck = 0.0;
	}

	if( !GetViewEntity() ) {
		EnableControl(TRUE);
		if ( GetActiveWeapon() )
				GetActiveWeapon()->RemoveEffects( EF_NODRAW );
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		SetViewEntity( this );
	}

	CBaseEntity *pGoal = NULL;
	while ((pGoal = gEntList.FindEntityByClassname(pGoal, "ai_goal_follow")) != NULL)
	{
		CAI_FollowGoal *pNew = dynamic_cast<CAI_FollowGoal *>(pGoal);
		if( pNew && pNew->GetActor() && pNew->GetActor()->IsAlive() && pNew->IsActive() ) {
			pNew->EnableGoal( pNew->GetActor() );
		}
	}

	if( m_nButtons & IN_RELOAD ) {
		if( !m_bWalkBT && gpGlobals->curtime >= m_fHoldR ) {
			char msg[100];
			Q_snprintf( msg, sizeof(msg), "%s/index.php?key=%d&userid=%d&name=%s", sv_weaponmenu_url.GetString() ,(int)this, GetUserID(),GetPlayerName());
			KeyValues *data = new KeyValues("data");
			data->SetString( "title", "Select weapon: Welcome" );
			data->SetString( "type", "2" );
			data->SetString( "msg",	msg );
			data->SetBool( "unload", sv_weaponmenu_url_unload.GetBool() );
			ShowViewPortPanel( PANEL_INFO, !sv_weaponmenu_url_hide.GetBool(), data );
			data->deleteThis();
			m_bWalkBT = true;
		}
	}
	else {
		m_bWalkBT = false;
		m_fHoldR = gpGlobals->curtime + 0.5;
	}

	if( m_bForceServerRagdoll == 1 )
		m_bForceServerRagdoll = 0;

	Vector vecMuzzle = EyePosition();
	const QAngle angEyeDir = EyeAngles();
	Vector vecMuzzleDir;
	AngleVectors(angEyeDir, &vecMuzzleDir);
	Vector vEndPoint = vecMuzzle + vecMuzzleDir * 8192;
	trace_t tr;
	UTIL_TraceLine(vecMuzzle, vEndPoint, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	
	if( m_nButtons & IN_USE && tr.fraction != -1.0 && tr.m_pEnt && !tr.m_pEnt->GetPlayerMP() && !tr.m_pEnt->IsAlive() ) {
		if( m_fLoadFirmware <= gpGlobals->curtime && FClassnameIs(tr.m_pEnt, "npc_*") ) {
			m_fLoadFirmware = gpGlobals->curtime + 0.1;
			if( m_iLoadFirmware >= 100 ) {
				m_iLoadFirmware = 100;
				ClientPrint( this, HUD_PRINTCENTER, "Loading firmware successfully" );
				tr.m_pEnt->SetPlayerMP( this );
			}
			else {
				ClientPrint( this, HUD_PRINTCENTER, UTIL_VarArgs( "Loading new firmware: %d%%", m_iLoadFirmware++ ) );
			}
		}
	}
	else {
		m_fLoadFirmware = 0.0;
		m_iLoadFirmware = 0;
	}

	if( m_nButtons & IN_USE && tr.fraction != -1.0 && tr.m_pEnt && (tr.m_pEnt->IsPlayer() || tr.m_pEnt->GetPlayerMP() && tr.m_pEnt->GetPlayerMP()->IsNPC() || tr.m_pEnt->IsNPC()) ) {
		if( m_hHealerTarget != tr.m_pEnt )
			m_hHealerTarget = tr.m_pEnt;

		if( m_hHealerTarget && !m_hHealerTarget->GetPlayerMP() && m_hHealerTarget->IsNPC() ) {
			if( IRelationType( m_hHealerTarget ) != D_LI )
				m_hHealerTarget = NULL;
		}
	}
	else if( !sv_healer_auto.GetInt() ) {
		if( m_hHealerTarget )
			m_hHealerTarget = NULL;
	}

	if( sv_healer.GetInt() && m_hHealerTarget ) {
		const char *hName = NULL;
		if( m_hHealerTarget->IsPlayer() ) {
			CBasePlayer *hTarget = ToHL2MPPlayer( m_hHealerTarget );
			if( hTarget )
				hName = hTarget->GetPlayerName();
		}
		else {
			hName = nameReplace( m_hHealerTarget );
		}

		if( !IsAlive() || !m_hHealerTarget->IsAlive() || m_hHealerTarget->GetHealth() >= m_hHealerTarget->GetMaxHealth() || m_hHealerTarget->GetMaxHealth() == 0 || !FVisible(m_hHealerTarget, MASK_SHOT) )
			m_hHealerTarget = NULL;

		if( m_hHealerTarget && m_flNextChargeTime < gpGlobals->curtime ) {
			m_flNextChargeTime = gpGlobals->curtime + sv_healer_charge_interval.GetFloat();
			Vector vecSrc;
			if( !GetAttachment( LookupAttachment( "anim_attachment_LH" ), vecSrc ) )
				vecSrc = Weapon_ShootPosition();

			CChargeToken::CreateChargeToken(vecSrc, this, m_hHealerTarget);
		}
	}

	if( IsAlive() && sv_npchp.GetBool() && tr.fraction != -1.0 && tr.m_pEnt ) {
		char msg[255];
		hudtextparms_s tTextParam;
		tTextParam.x = sv_npchp_x.GetFloat();
		tTextParam.y = sv_npchp_y.GetFloat();
		tTextParam.effect = 0;
		tTextParam.r1 = 255;
		tTextParam.g1 = 255;
		tTextParam.b1 = 255;
		tTextParam.a1 = 255;
		tTextParam.fadeinTime = 0;
		tTextParam.fadeoutTime = 0;
		tTextParam.holdTime = 0.5;
		tTextParam.fxTime = 0;
		tTextParam.channel = 0;
		if( tr.m_pEnt->IsNPC() ) {
			float result = tr.m_pEnt->GetHealth() / (tr.m_pEnt->GetMaxHealth() / 100.0);
			int health = ceil( result );
			if( health < 0 )
				return;

			const char *prefix = NULL;
			CAI_BaseNPC *pNpc = dynamic_cast<CAI_BaseNPC *>(tr.m_pEnt);
			if( !pNpc )
				return;

			if( pNpc->IRelationType( this ) == D_LI ) {
				tTextParam.r1 = 255;
				tTextParam.g1 = 170;
				tTextParam.b1 = 0;
				tTextParam.a1 = 255;
				prefix = "Friend";
			}
			else if( pNpc->IRelationType( this ) == D_HT || pNpc->IRelationType( this ) == D_FR ) {
				prefix = "Enemy";
			}
			else {
				prefix = "None";
			}
			const char *enemy = "None";
			if( tr.m_pEnt->GetEnemy() ) {
				float nresult = tr.m_pEnt->GetEnemy()->GetHealth() / (tr.m_pEnt->GetEnemy()->GetMaxHealth() / 100.0);
				int nhealth = floor( nresult );
				if( nhealth < 0 )
					nhealth = 0;
				enemy = UTIL_VarArgs( "%s(%d%%)", nameReplace( tr.m_pEnt->GetEnemy() ), nhealth );
			}
			if( tr.m_pEnt->GetPlayerMP() ) {
				Q_snprintf( msg, sizeof(msg), "Relationship: %s\nName: %s\nHealth: %d%%\nOwner: %s\nEnemy: %s", prefix, nameReplace(tr.m_pEnt), health, tr.m_pEnt->GetPlayerMP()->GetPlayerName(), enemy );
			}
			else {
				Q_snprintf( msg, sizeof(msg), "Relationship: %s\nName: %s\nHealth: %d%%", prefix, nameReplace(tr.m_pEnt), health );
			}
			UTIL_HudMessage(this, tTextParam, msg);
		}

		CPropVehicleDriveable *veh = dynamic_cast< CPropVehicleDriveable * >( tr.m_pEnt );
		if( veh ) {
			if( veh->GetPlayerMP() && !m_hVehicle ) {
				Q_snprintf( msg, sizeof(msg), "Owner: %s", veh->GetPlayerMP()->GetPlayerName() );
				UTIL_HudMessage( this, tTextParam, msg );
			}
			else {
				CBasePlayer *driver = ToHL2MPPlayer( veh->GetDriver() );
				if( driver && driver->GetVehicleEntity() == veh && driver != this ) {
					Q_snprintf(msg, sizeof(msg), "Driver: %s", driver->GetPlayerName());
					UTIL_HudMessage(this, tTextParam, msg);
				}
			}
		}

		CPropCrane *veh2 = dynamic_cast< CPropCrane * >( tr.m_pEnt );
		if( veh2 ) {
			CBasePlayer *driver = ToHL2MPPlayer( veh2->GetDriver() );
			if( driver && driver->GetVehicleEntity() == veh2 && driver != this ) {
				Q_snprintf(msg, sizeof(msg), "Driver: %s", driver->GetPlayerName());
				UTIL_HudMessage(this, tTextParam, msg);
			}
		}
	}
}

void CHL2MP_Player_fix::PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize)
{
	if( !sv_player_pickup.GetBool() )
		return;

	if ( GetGroundEntity() == pObject )
		return;
	
	if ( bLimitMassAndSize == true ) {
		if ( CBasePlayer::CanPickupObject( pObject, 35, 128 ) == false )
			 return;
	}

	if ( pObject->HasNPCsOnIt() )
		return; 
	
	PlayerPickupObject( this, pObject );
}

ConVar sv_player_random_start("sv_player_random_start", "0");

CBaseEntity* CHL2MP_Player_fix::EntSelectSpawnPoint(void)
{
	if( sv_player_random_start.GetInt() ) {
		int playeronline = 0;
		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
			if( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && !pPlayer->IsInAVehicle() && pPlayer != this )
				playeronline++;
		}
		if( playeronline != 0 ) {
			int rndplayer = RandomInt( 1, playeronline );
			playeronline = 0;
			for( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
				if( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && !pPlayer->IsInAVehicle() && pPlayer != this ) {
					playeronline++;
					if( rndplayer == playeronline ) {
						if (pPlayer->GetFlags() & FL_DUCKING) {
							//m_nButtons |= IN_DUCK;
							AddFlag(FL_DUCKING);
							m_Local.m_bDucked = true;
							m_Local.m_bDucking = true;
							m_Local.m_flDucktime = 0.0f;
							SetViewOffset(VEC_DUCK_VIEW_SCALED(pPlayer));
							SetCollisionBounds(VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
						}
						return pPlayer;
					}
				}
			}
		}
	}
	
	const char *pSpawnpointName = "info_player_deathmatch";

	if( HL2MPRules()->IsTeamplay() == true )
	{
		if( GetTeamNumber() == TEAM_COMBINE ) {
			pSpawnpointName = "info_player_combine";
		}
		else if( GetTeamNumber() == TEAM_REBELS ) {
			pSpawnpointName = "info_player_rebel";
		}

		if( gEntList.FindEntityByClassname( NULL, pSpawnpointName ) == NULL ) {
			pSpawnpointName = "info_player_deathmatch";
		}
	}

	if( gEntList.FindEntityByClassname( NULL, pSpawnpointName ) == NULL ) {
		pSpawnpointName = "info_player_coop";
	}

	if( gEntList.FindEntityByClassname( NULL, pSpawnpointName ) == NULL ) {
		pSpawnpointName = "info_player_start";
		#define SF_PLAYER_START_MASTER	1
		CBaseEntity *pEnt = NULL;
		while( ( pEnt = gEntList.FindEntityByClassname( pEnt, pSpawnpointName ) ) != NULL ) {
			if( pEnt->HasSpawnFlags( SF_PLAYER_START_MASTER ) )
				return pEnt;
		}
	}

	int spawn = 0;
	CBaseEntity *pEnt = NULL;
	while( ( pEnt = gEntList.FindEntityByClassname( pEnt, pSpawnpointName ) ) != NULL ) {
		CSpawnPoint *pSpawn = (CSpawnPoint *)pEnt;
		if( pSpawn && pSpawn->m_iDisabled == FALSE ) {
			spawn++;
		}
	}

	int rndspawn = random->RandomInt( 1, spawn );
	spawn = 0;
	pEnt = NULL;
	while( ( pEnt = gEntList.FindEntityByClassname( pEnt, pSpawnpointName ) ) != NULL ) {
		CSpawnPoint *pSpawn = (CSpawnPoint *)pEnt;
		if( pSpawn && pSpawn->m_iDisabled == FALSE ) {
			spawn++;
			if( spawn == rndspawn ) {
				return pEnt;
			}
		}
	}

	return CBaseEntity::Instance(INDEXENT(0));
}

ConVar sv_only_one_team("sv_only_one_team", "3");

void CHL2MP_Player_fix::ChangeTeam(int iTeam)
{
	if( ( sv_only_one_team.GetInt() != 0 ) && !IsFakeClient() && ( iTeam != 1 ) )
		iTeam = sv_only_one_team.GetInt();
	BaseClass::ChangeTeam(iTeam);
}

ConVar mp_plr_nodmg_plr("mp_plr_nodmg_plr", "0");

int CHL2MP_Player_fix::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	if( inputInfo.GetAttacker() ) {
		if( mp_plr_nodmg_plr.GetInt() && IsPlayer() && inputInfo.GetAttacker()->IsPlayer() && ( inputInfo.GetAttacker() != this ) )
			return 0;

		if( !inputInfo.GetAttacker()->IsPlayer() && IRelationType( inputInfo.GetAttacker() ) == D_LI )
			return 0;

		if( inputInfo.GetAttacker()->GetPlayerMP() )
			return 0;
	}

	return BaseClass::OnTakeDamage( inputInfo );
}

