#ifndef GAME_SERVER_CLASS_BOOMER_H
#define GAME_SERVER_CLASS_BOOMER_H

#include <game/server/class.h>

class CClassBoomer : public CClass
{
public:
    CClassBoomer(CGameContext *pGameServer);

    void OnPlayerDeath(int ClientID, int KillerID, vec2 Pos) override;
};

#endif