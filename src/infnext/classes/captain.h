#ifndef INFNEXT_CLASS_CAPTAIN_H
#define INFNEXT_CLASS_CAPTAIN_H

#include <infnext/class.h>

class CClassCaptain : public CClass
{
public:
    CClassCaptain(CGameContext *pGameServer);
    void OnCharacterSnap(class CNetObj_Character *pCharacter) override;
    void OnCharacterSnap(class protocol7::CNetObj_Character *pCharacter) override;
};

#endif