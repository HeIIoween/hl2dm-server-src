#include "cbase.h"
#include "vehicle_base.h"

class vehicleSpawn : public CBaseEntity
{
public:
	DECLARE_CLASS( vehicleSpawn, CBaseEntity );
	DECLARE_DATADESC();

	void Spawn( void );
	void TimerThink( void );
	void CreateVehicle( void );
	const char *m_vehiclescript;
	const char *m_model;
	int m_vehicletype;
	bool p_EnableGun;
	EHANDLE m_hLastCar;
};

LINK_ENTITY_TO_CLASS( info_vehicle_spawn, vehicleSpawn );

BEGIN_DATADESC( vehicleSpawn )

	DEFINE_THINKFUNC( TimerThink ),
	DEFINE_KEYFIELD( m_vehiclescript, FIELD_STRING, "vehiclescript" ),
	DEFINE_KEYFIELD( m_model, FIELD_STRING, "model" ),
	DEFINE_KEYFIELD( m_vehicletype, FIELD_INTEGER, "vehicletype" ),
	DEFINE_KEYFIELD( p_EnableGun, FIELD_BOOLEAN, "EnableGun" ),

END_DATADESC()

void vehicleSpawn::Spawn( void )
{
	SetThink( &vehicleSpawn::TimerThink );
	SetNextThink( gpGlobals->curtime + 0.5 );
	CreateVehicle();
}

const char *gpVehicleList[] = {
	"prop_vehicle_jeep",
	"prop_vehicle_airboat",
	"prop_vehicle_jalopy",
};

void vehicleSpawn::TimerThink( void )
{
	SetNextThink( gpGlobals->curtime + 5.0 );

	if( m_hLastCar && m_hLastCar->GetPlayerMP() )
		m_hLastCar = NULL;

	if( !m_hLastCar ) 
		CreateVehicle();
}

void vehicleSpawn::CreateVehicle( void )
{
	Vector vecOrigin = GetAbsOrigin();
	QAngle vecAngles = GetAbsAngles();
	CBaseEntity *pJeep = CreateEntityByName(  gpVehicleList[m_vehicletype] );
	if( pJeep ) {
		pJeep->SetAbsOrigin( vecOrigin );
		pJeep->SetAbsAngles( vecAngles );
		pJeep->KeyValue( "model", m_model );
		pJeep->KeyValue( "solid", "6" );
		pJeep->KeyValue( "vehiclescript", m_vehiclescript );
		pJeep->KeyValue("EnableGun",p_EnableGun);
		DispatchSpawn( pJeep );
		pJeep->Activate();
		pJeep->SetOwnerEntity( this );
		m_hLastCar = pJeep;
	}
}