#ifndef INFNEXT_WEAPONS_HAMMER_H
#define INFNEXT_WEAPONS_HAMMER_H

#include <infnext/weapon.h>

class CWeaponHammer : public CWeapon
{
public:
    CWeaponHammer(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif