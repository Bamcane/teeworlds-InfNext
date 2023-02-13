#ifndef GAME_SERVER_WEAPONS_HAMMER_H
#define GAME_SERVER_WEAPONS_HAMMER_H

#include <game/server/weapon.h>

class CWeaponHammer : public CWeapon
{
public:
    CWeaponHammer(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif