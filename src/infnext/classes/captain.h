#ifndef INFNEXT_CLASS_CAPTAIN_H
#define INFNEXT_CLASS_CAPTAIN_H

#include <infnext/class.h>

class CClassCaptain : public CClass
{
public:
    CClassCaptain(CGameContext *pGameServer, CPlayer *pOwner);

    void OnCharacterSnap(class CNetObj_Character *pCharacter) override;
    void OnCharacterSnap(class protocol7::CNetObj_Character *pCharacter) override;

    CClass *CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) override;
};

#endif