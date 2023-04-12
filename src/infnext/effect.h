#ifndef INFNEXT_EFFECT_H
#define INFNEXT_EFFECT_H

#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>

class CEffect
{
    class CCharacter *m_pCharacter;

protected:
    class CCharacter *Character() { return m_pCharacter; }
    class CPlayer *Player();
    class IServer *Server();
    class CGameContext *GameServer();
    class IGameController *GameController();
    int m_StartTick;
    int m_Time;// in tick
public:
    CEffect *m_pNextEffect;
    int m_Type;

    CEffect(class CCharacter *pOwner, int StartTick, int Time, int Type);
    virtual ~CEffect() {};

    virtual void OnPlayerTick();
    virtual void OnCharacterSnap(CNetObj_Character *pCharacter);
    virtual void OnCharacterSnap(protocol7::CNetObj_Character *pCharacter);
    virtual void OnPlayerTuning(class CTuningParams *pTuning);
};

#endif