#ifndef GAME_SERVER_WEAPONS_BOOMER_HAMMER_H
#define GAME_SERVER_WEAPONS_BOOMER_HAMMER_H

#include "hammer.h"

class CWeaponBoomerHammer : public CWeaponHammer
{
public:
    CWeaponBoomerHammer(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif