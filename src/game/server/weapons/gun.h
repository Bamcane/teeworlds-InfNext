#ifndef GAME_SERVER_WEAPONS_GUN_H
#define GAME_SERVER_WEAPONS_GUN_H

#include <game/server/weapon.h>

class CWeaponGun : public CWeapon
{
public:
    CWeaponGun(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif