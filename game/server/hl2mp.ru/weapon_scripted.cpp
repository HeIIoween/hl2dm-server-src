#include "cbase.h"
#include "soundent.h"

class WeaponScripted : public CBaseEntity
{
public:
	DECLARE_CLASS( WeaponScripted, CBaseEntity );
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( weapon_scripted, WeaponScripted );

static const char *RandomWeapon[] = 
{
	"weapon_ak47",
	"weapon_m4a1",
	"weapon_m5navy",
	"weapon_mac10",
	"weapon_xm1014",
	"weapon_scout",
	"weapon_m3",
};

void WeaponScripted::Spawn( void )
{
	CBaseEntity *entity = CreateEntityByName( RandomWeapon[rand() % sizeof(RandomWeapon)/sizeof(RandomWeapon[0])] );
	if( entity ) {
		entity->SetAbsOrigin( GetAbsOrigin() );
		entity->SetAbsAngles( GetAbsAngles() );
		DispatchSpawn( entity );
	}
	Remove();
}