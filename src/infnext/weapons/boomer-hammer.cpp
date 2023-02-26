#include <game/server/gamecontext.h>
#include "boomer-hammer.h"

CWeaponBoomerHammer::CWeaponBoomerHammer(CGameContext *pGameServer)
    : CWeaponHammer(pGameServer)
{
}

void CWeaponBoomerHammer::Fire(vec2 Pos, vec2 Direction, int Owner)
{
    CCharacter *pOwnerChr = GameServer()->GetPlayerChar(Owner);
    if(!pOwnerChr)
        return;

    pOwnerChr->Die(Owner, WEAPON_SELF);

    return;
}