#ifndef INFNEXT_WEAPONS_MEDIC_SHOTGUN_H
#define INFNEXT_WEAPONS_MEDIC_SHOTGUN_H

#include <infnext/weapons/shotgun.h>

class CWeaponMedicShotgun : public CWeaponShotgun
{
public:
    CWeaponMedicShotgun(CGameContext *pGameServer);
    void Fire(vec2 Pos, vec2 Dir, int Owner) override; 
};

#endif