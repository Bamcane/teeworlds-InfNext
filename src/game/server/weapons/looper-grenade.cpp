
#include <game/server/gamecontext.h>
#include <game/server/weapon-entities/projectile.h>
#include "looper-grenade.h"

CWeaponLooperGrenade::CWeaponLooperGrenade(CGameContext *pGameServer)
    : CWeaponGrenade(pGameServer)
{
    m_AmmoRegen = 5000;
    m_InitAmmo = 5;
}