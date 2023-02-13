#ifndef GAME_SERVER_WEAPONS_LOOPER_GRENADE_H
#define GAME_SERVER_WEAPONS_LOOPER_GRENADE_H

#include <game/server/weapons/grenade.h>

class CWeaponLooperGrenade : public CWeaponGrenade
{
public:
    CWeaponLooperGrenade(CGameContext *pGameServer);
};

#endif