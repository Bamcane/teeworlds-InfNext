#ifndef INFNEXT_WEAPONS_MEDIC_HAMMER_H
#define INFNEXT_WEAPONS_MEDIC_HAMMER_H

#include <infnext/weapons/hammer.h>

class CWeaponMedicHammer : public CWeaponHammer
{
public:
    CWeaponMedicHammer(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif