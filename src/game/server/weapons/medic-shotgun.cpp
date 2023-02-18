#include <game/server/gamecontext.h>
#include <game/server/weapon-entities/projectile.h>
#include "medic-shotgun.h"

CWeaponMedicShotgun::CWeaponMedicShotgun(CGameContext *pGameServer)
    : CWeaponShotgun(pGameServer)
{
    m_AmmoRegen = 750;
    m_FireDelay = 250;
}