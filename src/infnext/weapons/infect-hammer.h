#ifndef INFNEXT_WEAPONS_INFECT_HAMMER_H
#define INFNEXT_WEAPONS_INFECT_HAMMER_H

#include <infnext/weapons/hammer.h>

class CWeaponInfectHammer : public CWeaponHammer
{
public:
    CWeaponInfectHammer(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif