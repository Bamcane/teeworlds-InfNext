#ifndef INFNEXT_WEAPONS_GUN_H
#define INFNEXT_WEAPONS_GUN_H

#include <infnext/weapon.h>

class CWeaponGun : public CWeapon
{
public:
    CWeaponGun(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif