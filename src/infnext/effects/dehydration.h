#ifndef INFNEXT_EFFECTS_DEHYDRATION_H
#define INFNEXT_EFFECTS_DEHYDRATION_H

#include <infnext/effect.h>

class CDehydration : public CEffect
{
    int m_From;
    int m_Weapon;
    int m_Damage;
public:
    CDehydration(class CCharacter *pOwner, int StartTick, int Time, int Damage, int From, int Weapon);

    void OnCharacterSnap(CNetObj_Character *pCharacter) override;
    void OnCharacterSnap(protocol7::CNetObj_Character *pCharacter) override;
    void OnPlayerTick() override;
};

#endif