#include <game/server/gamecontext.h>
#include "medic-hammer.h"

CWeaponMedicHammer::CWeaponMedicHammer(CGameContext *pGameServer)
    : CWeaponHammer(pGameServer)
{
}

void CWeaponMedicHammer::Fire(vec2 Pos, vec2 Direction, int Owner)
{
    CCharacter *pOwnerChr = GameServer()->GetPlayerChar(Owner);
    if(!pOwnerChr)
        return;

	GameServer()->CreateSound(Pos, SOUND_HAMMER_FIRE);

    CCharacter *apEnts[MAX_CLIENTS];
    int Hits = 0;
    int Num = GameServer()->m_World.FindEntities(Pos, pOwnerChr->m_ProximityRadius*0.5f, (CEntity**)apEnts,
                                                MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

    for (int i = 0; i < Num; ++i)
    {
        CCharacter *pTarget = apEnts[i];

        if ((pTarget == pOwnerChr) || GameServer()->Collision()->IntersectLine(Pos, pTarget->m_Pos, NULL, NULL))
            continue;

        // set his velocity to fast upward (for now)
        if(length(pTarget->m_Pos-Pos) > 0.0f)
            GameServer()->CreateHammerHit(pTarget->m_Pos-normalize(pTarget->m_Pos-Pos)*pOwnerChr->m_ProximityRadius*0.5f);
        else
            GameServer()->CreateHammerHit(Pos);

        vec2 Dir;
        if (length(pTarget->m_Pos - Pos) > 0.0f)
            Dir = normalize(pTarget->m_Pos - Pos);
        else
            Dir = vec2(0.f, -1.f);

        if(pTarget->GetPlayer()->IsInfect())
            pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, 20,
                pOwnerChr->GetPlayer()->GetCID(), WEAPON_HAMMER, DAMAGEMODE_DMG);
        else
        {
            if(pTarget->IncreaseHealth(2))
                pTarget->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
            else if(pTarget->IncreaseArmor(2))
                pTarget->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
        }
        Hits++;
    }

    // if we Hit anything, we have to wait for the reload
    if(Hits)
        pOwnerChr->SetReloadTimer(GameServer()->Server()->TickSpeed()/3);

    
    return;
}