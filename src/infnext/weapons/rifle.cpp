#include <game/server/gamecontext.h>
#include <infnext/weapon-entities/laser.h>
#include "rifle.h"

CWeaponRifle::CWeaponRifle(CGameContext *pGameServer)
    : CWeapon(pGameServer)
{
    m_MaxAmmo = g_pData->m_Weapons.m_Rifle.m_pBase->m_Maxammo;
    m_InitAmmo = m_MaxAmmo;
    m_AmmoRegen = g_pData->m_Weapons.m_Rifle.m_pBase->m_Ammoregentime;
    m_FireDelay = g_pData->m_Weapons.m_Rifle.m_pBase->m_Firedelay;
    m_FullAuto = true;
}

void CWeaponRifle::Fire(vec2 Pos, vec2 Dir, int Owner)
{
    CLaser *pLaser = new CLaser(GameWorld(),
        Pos,
        Dir,
        GameServer()->Tuning()->m_LaserReach,
        Owner,
        GameServer()->Tuning()->m_LaserDamage);

    GameServer()->CreateSound(Pos, SOUND_RIFLE_FIRE);
    
    return;
}