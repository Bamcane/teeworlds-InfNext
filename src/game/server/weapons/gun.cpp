
#include <game/server/gamecontext.h>
#include <game/server/weapon-entities/projectile.h>
#include "gun.h"

CWeaponGun::CWeaponGun(CGameContext *pGameServer)
    : CWeapon(pGameServer)
{
    m_MaxAmmo = g_pData->m_Weapons.m_Gun.m_pBase->m_Maxammo;
    m_InitAmmo = m_MaxAmmo;
    m_AmmoRegen = g_pData->m_Weapons.m_Gun.m_pBase->m_Ammoregentime;
    m_FireDelay = g_pData->m_Weapons.m_Gun.m_pBase->m_Firedelay;
    m_FullAuto = false;
}

void CWeaponGun::Fire(vec2 Pos, vec2 Dir, int Owner)
{
    CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_GUN,
    Owner,
    Pos,
    Dir,
    (int)(GameServer()->Server()->TickSpeed()*GameServer()->Tuning()->m_GunLifetime),
    1, 0, 0, -1, WEAPON_GUN);

    GameServer()->CreateSound(Pos, SOUND_GUN_FIRE);
    
    return;
}