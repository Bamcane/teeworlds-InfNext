#ifndef INFNEXT_CLASS_QUEEN_H
#define INFNEXT_CLASS_QUEEN_H

#include <infnext/class.h>

class CClassQueen : public CClass
{
    int m_HookDamage;
    int m_HookDmgTick;
public:
    CClassQueen(CGameContext *pGameServer, CPlayer *pOwner);

    void OnTick() override;
    int OnPlayerDeath(int KillerID, vec2 Pos) override;
    bool OnPlayerSpawn(bool IsInfected, vec2* Pos) override;

    CClass *CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) override;
};

#endif