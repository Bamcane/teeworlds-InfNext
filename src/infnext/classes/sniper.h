#ifndef INFNEXT_CLASS_SNIPER_H
#define INFNEXT_CLASS_SNIPER_H

#include <infnext/class.h>

class CClassSniper : public CClass
{
public:
    CClassSniper(CGameContext *pGameServer, CPlayer *pOwner);

    int OnPlayerDeath(int KillerID, vec2 Pos) override;

    CClass *CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) override;
};

#endif