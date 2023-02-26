#ifndef INFNEXT_WEAPONS_LOOPER_RIFLE_H
#define INFNEXT_WEAPONS_LOOPER_RIFLE_H

#include <infnext/weapons/rifle.h>

class CWeaponLooperRifle : public CWeaponRifle
{
public:
    CWeaponLooperRifle(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Dir, int Owner) override;
};

#endif