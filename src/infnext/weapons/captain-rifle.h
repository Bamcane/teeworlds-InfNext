#ifndef INFNEXT_WEAPONS_CAPTAIN_RIFLE_H
#define INFNEXT_WEAPONS_CAPTAIN_RIFLE_H

#include <infnext/weapons/rifle.h>

class CWeaponCaptainRifle : public CWeaponRifle
{
public:
    CWeaponCaptainRifle(CGameContext *pGameServer);

    void Fire(vec2 Pos, vec2 Dir, int Owner) override;
};

#endif