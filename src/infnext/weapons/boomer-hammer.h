#ifndef INFNEXT_WEAPONS_BOOMER_HAMMER_H
#define INFNEXT_WEAPONS_BOOMER_HAMMER_H

#include <infnext/weapons/hammer.h>

class CWeaponBoomerHammer : public CWeaponHammer
{
public:
    CWeaponBoomerHammer(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif