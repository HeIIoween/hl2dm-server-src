#include "cbase.h"

class CHLSpawnPoint : public CPointEntity
{
public:
	DECLARE_CLASS(CHLSpawnPoint, CPointEntity);
	DECLARE_DATADESC();

	int	m_iDisabled;
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
};