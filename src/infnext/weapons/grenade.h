#ifndef INFNEXT_WEAPONS_GRENADE_H
#define INFNEXT_WEAPONS_GRENADE_H

#include <infnext/weapon.h>

class CWeaponGrenade : public CWeapon
{
public:
    CWeaponGrenade(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif