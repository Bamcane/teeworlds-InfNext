#include <game/server/gamecontext.h>
#include <infnext/entities/sea-water.h>
#include "captain-rifle.h"

CWeaponCaptainRifle::CWeaponCaptainRifle(CGameContext *pGameServer)
    : CWeaponRifle(pGameServer)
{
    m_AmmoRegen = 500;
    m_MaxAmmo = 5;
    m_InitAmmo = 5;
    m_FireDelay = 250;
}

void CWeaponCaptainRifle::Fire(vec2 Pos, vec2 Dir, int Owner)
{
    new CSeaWater(GameWorld(), Owner, Pos, Dir, 3.0f, WEAPON_RIFLE);

    GameServer()->CreateSound(Pos, SOUND_RIFLE_BOUNCE);
    
    return;
}