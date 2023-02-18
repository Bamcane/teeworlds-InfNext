#ifndef GAME_SERVER_CLASS_SMOKER_H
#define GAME_SERVER_CLASS_SMOKER_H

#include <game/server/class.h>

class CClassSmoker : public CClass
{
public:
    CClassSmoker(CGameContext *pGameServer);
    int m_HookDamage;

    void OnTick(class CCharacter *pOwner) override;
};

#endif