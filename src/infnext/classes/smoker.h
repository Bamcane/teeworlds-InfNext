#ifndef INFNEXT_CLASS_SMOKER_H
#define INFNEXT_CLASS_SMOKER_H

#include <infnext/class.h>

class CClassSmoker : public CClass
{
    int m_HookDamage;
    int m_HookDmgTick;
public:
    CClassSmoker(CGameContext *pGameServer, CPlayer *pOwner);

    void OnTick() override;

    CClass *CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) override;
};

#endif