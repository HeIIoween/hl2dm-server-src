#ifdef CSTRIKE_DLL
#include "cs_player.h"
#else
#include "dod_player.h"
#endif
#include "filesystem.h"
#include "viewport_panel_names.h"
#include "Sprite.h"

#ifdef CSTRIKE_DLL
class CHL2MP_Player_fix : public CCSPlayer
#else
class CHL2MP_Player_fix : public CDODPlayer
#endif
{
public:
	#ifdef CSTRIKE_DLL
		DECLARE_CLASS(CHL2MP_Player_fix, CCSPlayer);
	#else
		DECLARE_CLASS(CHL2MP_Player_fix, CDODPlayer);
	#endif
	
	//DECLARE_DATADESC();
	virtual void Spawn(void);
	virtual void PostThink(void);
	virtual void PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize);
	virtual CBaseEntity * EntSelectSpawnPoint( void );
	virtual void ChangeTeam(int iTeam);
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	virtual int OnTakeDamage( const CTakeDamageInfo &inputInfo );
	virtual void DeathNotice ( CBaseEntity *pVictim );
	virtual CBaseEntity * GetAimTarget ( void );
	float m_flNextChargeTime;
	int m_iLoadFirmware;
	float m_fLoadFirmware;
	float m_flRespawn;
	EHANDLE m_hHealerTarget;
	bool m_bWalkBT;
	float m_fHoldR;
	float m_fStuck;
	float m_fNextHealth;
	CHandle<CHL2MP_Player_fix> m_hBot;
	CHandle<CSprite> m_hHealSprite;
	EHANDLE m_hHealBarTarget;
};

inline const char *nameReplace(CBaseEntity *pSearch)
{
	const char *search = NULL;
	if( pSearch->GetCustomName() != NULL )
		return pSearch->GetCustomName();
	else
		search = pSearch->GetClassname();
	
	static KeyValues *manifest;
	static KeyValues *sub;
	if( manifest == NULL ) {
		manifest = new KeyValues("EntityInfo");
		manifest->LoadFromFile(filesystem, "entityinfo.txt", "GAME");
		Msg("[nameReplace]LOAD: entityinfo.txt\n");
	}
	for( sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
	{
		if( !Q_stricmp(sub->GetName(), search) ) {
			return sub->GetString("Name");
		}
	}
	//manifest->deleteThis();

	return search;
}

inline bool IsAdmin( CBaseEntity *pEntity ) {
	CBasePlayer *pPlayer = ToBasePlayer( pEntity );
	if( !pPlayer )
		return false;

	CSteamID steamID;
	pPlayer->GetSteamID( &steamID );
	KeyValues *manifest;
	KeyValues *sub;
	bool pResult = false;
	manifest = new KeyValues("AdminsList");
	manifest->LoadFromFile(filesystem, "admins.txt", "GAME");
	for( sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
	{
		if( sub->GetUint64("steamID64") == steamID.ConvertToUint64() ) {
			pResult = true;
			break;
		}
	}
	manifest->deleteThis();
	return pResult;
}