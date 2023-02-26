#ifndef INFNEXT_WEAPONS_SHOTGUN_H
#define INFNEXT_WEAPONS_SHOTGUN_H

#include <infnext/weapon.h>

class CWeaponShotgun : public CWeapon
{
public:
    CWeaponShotgun(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Dir, int Owner) override;
};

#endif