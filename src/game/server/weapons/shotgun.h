#ifndef GAME_SERVER_WEAPONS_SHOTGUN_H
#define GAME_SERVER_WEAPONS_SHOTGUN_H

#include <game/server/weapon.h>

class CWeaponShotgun : public CWeapon
{
public:
    CWeaponShotgun(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Dir, int Owner) override;
};

#endif