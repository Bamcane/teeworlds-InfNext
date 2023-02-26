#ifndef INFNEXT_CLASS_BOOMER_H
#define INFNEXT_CLASS_BOOMER_H

#include <infnext/class.h>

class CClassBoomer : public CClass
{
public:
    CClassBoomer(CGameContext *pGameServer);

    void OnPlayerDeath(int ClientID, int KillerID, vec2 Pos) override;
};

#endif