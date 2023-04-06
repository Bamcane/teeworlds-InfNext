#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>

#include <infnext/effects/dehydration.h>
#include "sea-water.h"

CSeaWater::CSeaWater(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir, int Span, int Weapon)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_SEAWATER)
{
	m_Pos = Pos;
	m_ActualPos = Pos;
	m_Direction = Dir;
	m_LifeSpan = Span*Server()->TickSpeed();
	m_Owner = Owner;

	m_Weapon = Weapon;
	m_StartTick = Server()->Tick();

	GameWorld()->InsertEntity(this);
}

void CSeaWater::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

vec2 CSeaWater::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	Curvature = 6.0f;
	Speed = 1000.0f;

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}

void CSeaWater::Tick()
{
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
    
	m_ActualPos = CurPos;
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, &CurPos, 0);
	
	m_LifeSpan--;

    if(Collide || m_LifeSpan < 0)
	    GameServer()->m_World.DestroyEntity(this);

    for(int i = 0;i < MAX_CLIENTS;i ++)
    {
        CCharacter *pChr = GameServer()->GetPlayerChar(i);
        if(!pChr)
            continue;
        if(pChr->IsHuman())
            continue;
        if(distance(pChr->m_Pos, CurPos) < 96.0f)
		{
            pChr->GetCore()->m_Vel += normalize(CurPos - PrevPos);
			pChr->AddEffect(new CDehydration(pChr, Server()->Tick(), 200, 10, m_Owner, m_Weapon));
		}
	}

    if(random_prob(0.3f))
	    GameServer()->CreateDeath(m_ActualPos, m_Owner);
}

void CSeaWater::TickPaused()
{
	++m_StartTick;
}