//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a sniper rifle weapon.
//			
//			Primary attack: fires a single high-powered shot, then reloads.
//			Secondary attack: cycles sniper scope through zoom levels.
//
// TODO: Circular mask around crosshairs when zoomed in.
// TODO: Shell ejection.
// TODO: Finalize kickback.
// TODO: Animated zoom effect?
//
//=============================================================================//

#include "cbase.h"
#include "ammodef.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "gamerules.h"				// For g_pGameRules
#include "in_buttons.h"
#include "soundent.h"
#include "vstdlib/random.h"
#include "beam_shared.h"
#include "hl2_player.h"
#include "npc_strider.h"
#include "hl2mp_player_fix.h"
#include "beam_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SNIPER_CONE_PLAYER					vec3_origin	// Spread cone when fired by the player.
#define SNIPER_CONE_NPC						vec3_origin	// Spread cone when fired by NPCs.
#define SNIPER_BULLET_COUNT_PLAYER			1			// Fire n bullets per shot fired by the player.
#define SNIPER_BULLET_COUNT_NPC				1			// Fire n bullets per shot fired by NPCs.
#define SNIPER_TRACER_FREQUENCY_PLAYER		0			// Draw a tracer every nth shot fired by the player.
#define SNIPER_TRACER_FREQUENCY_NPC			0			// Draw a tracer every nth shot fired by NPCs.
#define SNIPER_KICKBACK						3			// Range for punchangle when firing.

#define SNIPER_ZOOM_RATE					0.4			// Interval between zoom levels in seconds.


//-----------------------------------------------------------------------------
// Discrete zoom levels for the scope.
//-----------------------------------------------------------------------------
static int g_nZoomFOV[] =
{
	25,
	5
};

class CWeaponSniperRifle : public CBaseCombatWeapon
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponSniperRifle, CBaseCombatWeapon );

	CWeaponSniperRifle(void);

	void Precache( void );
	const Vector &GetBulletSpread( void );
	const char *GetTracerType( void ) { return "StriderTracer"; }
	bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void ItemPostFrame( void );
	void PrimaryAttack( void );
	bool Reload( void );
	void Zoom( void );
	float GetFireRate( void ) { return 1; };
	CHandle<CBeam> m_pBeam;
	DECLARE_ACTTABLE();

protected:
	float m_fNextZoom;
	int m_nZoomLevel;
};

LINK_ENTITY_TO_CLASS( weapon_sniperrifle, CWeaponSniperRifle );
PRECACHE_WEAPON_REGISTER(weapon_sniperrifle);

BEGIN_DATADESC( CWeaponSniperRifle )

	DEFINE_FIELD( m_fNextZoom, FIELD_FLOAT ),
	DEFINE_FIELD( m_nZoomLevel, FIELD_INTEGER ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Maps base activities to weapons-specific ones so our characters do the right things.
//-----------------------------------------------------------------------------
acttable_t	CWeaponSniperRifle::m_acttable[] = 
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_AR2,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_AR2,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_AR2,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_AR2,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_AR2,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_AR2,					false },
};

IMPLEMENT_ACTTABLE(CWeaponSniperRifle);

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CWeaponSniperRifle::CWeaponSniperRifle( void )
{
	m_fNextZoom = gpGlobals->curtime;
	m_nZoomLevel = 0;

	m_bReloadsSingly = true;

	m_fMinRange1		= 65;
	m_fMinRange2		= 65;
	m_fMaxRange1		= 2048;
	m_fMaxRange2		= 2048;
}

//-----------------------------------------------------------------------------
// Purpose: Turns off the zoom when the rifle is holstered.
//-----------------------------------------------------------------------------
bool CWeaponSniperRifle::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if (pPlayer != NULL)
	{
		if ( m_nZoomLevel != 0 )
		{
			engine->ClientCommand( pPlayer->edict(), "r_screenoverlay 0" );
			pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
			pPlayer->StopZooming();
			pPlayer->ShowViewModel(true);
			m_nZoomLevel = 0;
		}
	}

	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded to handle the zoom functionality.
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::ItemPostFrame( void )
{
	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if( pPlayer == NULL )
		return;

	if( m_nZoomLevel != 0 && (pPlayer->m_nButtons & IN_ZOOM) )
	{
		pPlayer->m_nButtons &= ~IN_ZOOM;
	}

	if ( m_bInReload && m_nZoomLevel != 0 )
	{
		engine->ClientCommand( pPlayer->edict(), "r_screenoverlay 0" );
		pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		pPlayer->StopZooming();
		pPlayer->ShowViewModel(true);
		m_nZoomLevel = 0;
	}

	if( m_bInReload && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		FinishReload();
		m_bInReload = false;
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.3;
		return;
	}

	if( pPlayer->m_nButtons & IN_ATTACK2 )
	{
		if( m_fNextZoom <= gpGlobals->curtime )
		{
			Zoom();
			pPlayer->m_nButtons &= ~IN_ATTACK2;
		}
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Same as base reload but doesn't change the owner's next attack time. This
//			lets us zoom out while reloading. This hack is necessary because our
//			ItemPostFrame is only called when the owner's next attack time has
//			expired.
// Output : Returns true if the weapon was reloaded, false if no more ammo.
//-----------------------------------------------------------------------------
bool CWeaponSniperRifle::Reload( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if (!pOwner)
	{
		return false;
	}

	if ( m_iClip1 > 0 )
			return false;

	WeaponSound(RELOAD);
	SendWeaponAnim( ACT_VM_RELOAD );

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

	m_bInReload = true;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast safely.
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
	{
		return;
	}

	if ( gpGlobals->curtime >= m_flNextPrimaryAttack )
	{
		// MUST call sound before removing a round from the clip of a CMachineGun dvs: does this apply to the sniper rifle? I don't know.
		WeaponSound(SINGLE);
		pPlayer->DoMuzzleFlash();

		SendWeaponAnim( ACT_VM_PRIMARYATTACK );

		// player "shoot" animation
		pPlayer->SetAnimation( PLAYER_ATTACK1 );

		// Don't fire again until fire animation has completed
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;

		Vector vecSrc	 = pPlayer->Weapon_ShootPosition();
		Vector vecAiming = pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );	

		// Fire the bullets
		FireBullets( SNIPER_BULLET_COUNT_PLAYER, vecSrc, vecAiming, GetBulletSpread(), MAX_TRACE_LENGTH, GetAmmoDef()->Index( "SniperRound" ), 1, -1, -1, 0, pPlayer );

		CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.2 );

		QAngle vecPunch(random->RandomFloat( -SNIPER_KICKBACK, SNIPER_KICKBACK ), 0, 0);
		pPlayer->ViewPunch(vecPunch);
		//pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
		m_iClip1 = m_iClip1 - 1;
	}

	// Register a muzzleflash for the AI.
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
}

//-----------------------------------------------------------------------------
// Purpose: Zooms in using the sniper rifle scope.
//-----------------------------------------------------------------------------
void CWeaponSniperRifle::Zoom( void )
{
	CHL2_Player *pPlayer = (CHL2_Player *)ToBasePlayer( GetOwner() );
	if (!pPlayer || m_bInReload )
	{
		return;
	}

	if (m_nZoomLevel >= sizeof(g_nZoomFOV) / sizeof(g_nZoomFOV[0]))
	{
		engine->ClientCommand( pPlayer->edict(), "r_screenoverlay 0" );
		pPlayer->StopZooming();
		pPlayer->ShowViewModel(true);
		pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
		WeaponSound(SPECIAL2);	
		m_nZoomLevel = 0;
	}
	else
	{
		if( m_nZoomLevel == 0 ) {
			pPlayer->m_Local.m_iHideHUD |= HIDEHUD_CROSSHAIR;
			engine->ClientCommand( pPlayer->edict(), "r_screenoverlay hl2mp.ru/scope.vmt" );
			//pPlayer->StartZooming();
			pPlayer->ShowViewModel(false);
		}

		pPlayer->SetFOV( pPlayer, g_nZoomFOV[m_nZoomLevel], 0.4 );
		WeaponSound(SPECIAL1);
		m_nZoomLevel++;
	}

	m_fNextZoom = gpGlobals->curtime + SNIPER_ZOOM_RATE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : virtual const Vector&
//-----------------------------------------------------------------------------
const Vector &CWeaponSniperRifle::GetBulletSpread( void )
{
	static Vector cone = SNIPER_CONE_PLAYER;
	return cone;
}