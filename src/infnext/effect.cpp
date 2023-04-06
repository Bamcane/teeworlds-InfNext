#include <game/server/gamecontext.h>
#include <engine/server.h>
#include "effect.h"


CEffect::CEffect(class CCharacter *pOwner, int StartTick, int Time, int Type)
{
    m_StartTick = StartTick;
    m_Time = Time;
    m_Type = Type;
    m_pCharacter = pOwner;
    m_pNextEffect = 0;
}

void CEffect::OnPlayerTick()
{
    if(Server()->Tick() > (m_StartTick + m_Time))
        m_pCharacter->RemoveEffect(this);

    if(m_pNextEffect)
        m_pNextEffect->OnPlayerTick();
}

void CEffect::OnCharacterSnap(CNetObj_Character *pCharacter)
{
    if(m_pNextEffect)
        m_pNextEffect->OnCharacterSnap(pCharacter);
}

void CEffect::OnCharacterSnap(protocol7::CNetObj_Character *pCharacter)
{
    if(m_pNextEffect)
        m_pNextEffect->OnCharacterSnap(pCharacter);
}

void CEffect::OnPlayerTuning(class CTuningParams *pTuning)
{
    if(m_pNextEffect)
        m_pNextEffect->OnPlayerTuning(pTuning);
}

CPlayer *CEffect::Player()
{
    return m_pCharacter->GetPlayer();
}

IServer *CEffect::Server()
{
    return m_pCharacter->Server();
}

CGameContext *CEffect::GameServer()
{
    return m_pCharacter->GameServer();
}

IGameController *CEffect::GameController()
{
    return GameServer()->m_pController;
}