#include <game/server/gamecontext.h>
#include <infnext/weapon-entities/laser.h>
#include "looper-rifle.h"

CWeaponLooperRifle::CWeaponLooperRifle(CGameContext *pGameServer)
    : CWeaponRifle(pGameServer)
{
    m_AmmoRegen = 750;
    m_FireDelay = 250;
}

void CWeaponLooperRifle::Fire(vec2 Pos, vec2 Dir, int Owner)
{
    CLaser *pLaser = new CLaser(GameWorld(),
        Pos,
        Dir,
        GameServer()->Tuning()->m_LaserReach*0.7f,
        Owner,
        GameServer()->Tuning()->m_LaserDamage);

    GameServer()->CreateSound(Pos, SOUND_RIFLE_FIRE);
    
    return;
}