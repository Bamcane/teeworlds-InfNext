#ifndef INFNEXT_CLASS_BOOMER_H
#define INFNEXT_CLASS_BOOMER_H

#include <infnext/class.h>

class CClassBoomer : public CClass
{
public:
    CClassBoomer(CGameContext *pGameServer, CPlayer *pOwner);

    void OnPlayerDeath(int KillerID, vec2 Pos) override;
    
    CClass *CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) override;
};

#endif