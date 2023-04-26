
#include <game/server/gamecontext.h>
#include <infnext/entities/projectile.h>
#include "grenade.h"

CWeaponGrenade::CWeaponGrenade(CGameContext *pGameServer)
    : CWeapon(pGameServer)
{
    m_MaxAmmo = g_pData->m_Weapons.m_Grenade.m_pBase->m_Maxammo;
    m_InitAmmo = m_MaxAmmo;
    m_AmmoRegen = g_pData->m_Weapons.m_Grenade.m_pBase->m_Ammoregentime;
    m_FireDelay = g_pData->m_Weapons.m_Grenade.m_pBase->m_Firedelay;
    m_FullAuto = true;
}

void CWeaponGrenade::Fire(vec2 Pos, vec2 Dir, int Owner)
{
   new CProjectile(GameWorld(), WEAPON_GRENADE,
        Owner,
        Pos,
        Dir,
        (int)(GameServer()->Server()->TickSpeed()*GameServer()->Tuning()->m_GrenadeLifetime),
        1, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);

    GameServer()->CreateSound(Pos, SOUND_GRENADE_FIRE);
    
    return;
}