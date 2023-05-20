#ifndef INFNEXT_CLASS_LOOPER_H
#define INFNEXT_CLASS_LOOPER_H

#include <infnext/class.h>

class CClassLooper : public CClass
{
public:
    CClassLooper(CGameContext *pGameServer, CPlayer *pOwner);

    int OnPlayerDeath(int KillerID, vec2 Pos) override;

    CClass *CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) override;
};

#endif