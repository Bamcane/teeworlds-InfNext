#ifndef INFNEXT_CLASS_MEDIC_H
#define INFNEXT_CLASS_MEDIC_H

#include <infnext/class.h>

class CClassMedic : public CClass
{
public:
    CClassMedic(CGameContext *pGameServer, CPlayer *pOwner);

    int OnPlayerDeath(int KillerID, vec2 Pos) override;

    CClass *CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) override;
};

#endif