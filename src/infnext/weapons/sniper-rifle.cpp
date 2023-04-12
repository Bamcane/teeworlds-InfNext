#include <game/server/gamecontext.h>
#include <infnext/entities/laser.h>
#include "sniper-rifle.h"

CWeaponSniperRifle::CWeaponSniperRifle(CGameContext *pGameServer)
    : CWeaponRifle(pGameServer)
{
    m_AmmoRegen = 3750;
}

void CWeaponSniperRifle::Fire(vec2 Pos, vec2 Dir, int Owner)
{
    new CLaser(GameWorld(),
        Pos,
        Dir,
        GameServer()->Tuning()->m_LaserReach,
        Owner,
        random_int(10, 13));

    GameServer()->CreateSound(Pos, SOUND_RIFLE_FIRE);
    
    return;
}