
#ifndef INFNEXT_ENTITIES_SEA_WATER_H
#define INFNEXT_ENTITIES_SEA_WATER_H

class CSeaWater : public CEntity
{
public:
	CSeaWater(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir, int Span, int Weapon);

	vec2 GetPos(float Time);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();

private:
	vec2 m_Direction;
	vec2 m_ActualPos;
	int m_LifeSpan;
	int m_Owner;
	int m_Weapon;
	int m_StartTick;
};

#endif
