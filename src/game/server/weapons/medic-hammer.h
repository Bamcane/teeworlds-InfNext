#ifndef GAME_SERVER_WEAPONS_MEDIC_HAMMER_H
#define GAME_SERVER_WEAPONS_MEDIC_HAMMER_H

#include <game/server/weapons/hammer.h>

class CWeaponMedicHammer : public CWeaponHammer
{
public:
    CWeaponMedicHammer(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Direction, int Owner) override;
};

#endif