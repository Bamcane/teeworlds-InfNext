#ifndef GAME_SERVER_WEAPONS_MEDIC_SHOTGUN_H
#define GAME_SERVER_WEAPONS_MEDIC_SHOTGUN_H

#include <game/server/weapons/shotgun.h>

class CWeaponMedicShotgun : public CWeaponShotgun
{
public:
    CWeaponMedicShotgun(CGameContext *pGameServer);
};

#endif