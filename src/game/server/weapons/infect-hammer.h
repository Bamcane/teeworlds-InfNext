#ifndef GAME_SERVER_WEAPONS_INFECT_HAMMER_H
#define GAME_SERVER_WEAPONS_INFECT_HAMMER_H

#include <game/server/weapons/hammer.h>

class CWeaponInfectHammer : public CWeaponHammer
{
public:
    CWeaponInfectHammer(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif