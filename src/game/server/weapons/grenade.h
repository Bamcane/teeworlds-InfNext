#ifndef GAME_SERVER_WEAPONS_GRENADE_H
#define GAME_SERVER_WEAPONS_GRENADE_H

#include <game/server/weapon.h>

class CWeaponGrenade : public CWeapon
{
public:
    CWeaponGrenade(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif