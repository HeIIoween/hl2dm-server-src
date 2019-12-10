#include "hl2mp_player.h"
#include "filesystem.h"
#include "viewport_panel_names.h"
#include "Sprite.h"

class CHL2MP_Player_fix : public CHL2MP_Player
{
public:
	DECLARE_CLASS(CHL2MP_Player_fix, CHL2MP_Player);
	//DECLARE_DATADESC();
	virtual void Spawn(void);
	virtual void PostThink(void);
	virtual void PickupObject(CBaseEntity *pObject, bool bLimitMassAndSize);
	virtual CBaseEntity * EntSelectSpawnPoint( void );
	virtual void ChangeTeam(int iTeam);
	virtual int OnTakeDamage( const CTakeDamageInfo &inputInfo );
	virtual void	DeathNotice ( CBaseEntity *pVictim );

	float m_flNextChargeTime;
	int m_iLoadFirmware;
	float m_fLoadFirmware;
	EHANDLE m_hHealerTarget;
	bool m_bWalkBT;
	float m_fHoldR;
	float m_fStuck;
	float m_fNextHealth;
	EHANDLE m_hBot;
	EHANDLE m_hNPC;
	CHandle<CSprite> m_hHealSprite;
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
		manifest->LoadFromFile(filesystem, "custom/hl2mp.ru/entityinfo.txt", "GAME");
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