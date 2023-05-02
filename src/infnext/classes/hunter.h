#ifndef INFNEXT_CLASS_HUNTER_H
#define INFNEXT_CLASS_HUNTER_H

#include <infnext/class.h>

class CClassHunter : public CClass
{
public:
    CClassHunter(CGameContext *pGameServer, CPlayer *pOwner);
    
    CClass *CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) override;
};

#endif