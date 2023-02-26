#ifndef INFNEXT_CLASS_SMOKER_H
#define INFNEXT_CLASS_SMOKER_H

#include <infnext/class.h>

class CClassSmoker : public CClass
{
public:
    CClassSmoker(CGameContext *pGameServer);
    int m_HookDamage;

    void OnTick(class CCharacter *pOwner) override;
};

#endif