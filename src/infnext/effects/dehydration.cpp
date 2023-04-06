#include <game/server/gamecontext.h>
#include <engine/server.h>

#include "dehydration.h"

CDehydration::CDehydration(class CCharacter *pOwner, int StartTick, int Time, int Damage, int From, int Weapon) :
    CEffect(pOwner, StartTick, Time, EFFECTTYPE_DEHYDRATION)
{
    m_Damage = Damage;
    m_From = From;
    m_Weapon = Weapon;
}

void CDehydration::OnPlayerTick()
{
    int Time = (m_StartTick + m_Time - Server()->Tick())/Server()->TickSpeed()+1;
    GameServer()->SendBroadcast_Localization(Player()->GetCID(), _("You are dehydrated: {sec:Time}"), 2, BROADCAST_DEHYDRATED, "Time", &Time);


    if((Server()->Tick() - m_StartTick) % (m_Time/m_Damage) == 0)
        Character()->TakeDamage(vec2(0.f, 1.f), 1, m_From, m_Weapon);

    CEffect::OnPlayerTick();
}

void CDehydration::OnCharacterSnap(CNetObj_Character *pCharacter)
{
    pCharacter->m_Emote = EMOTE_PAIN;
    CEffect::OnCharacterSnap(pCharacter);
}

void CDehydration::OnCharacterSnap(protocol7::CNetObj_Character *pCharacter)
{
    pCharacter->m_Emote = protocol7::EMOTE_PAIN;
    CEffect::OnCharacterSnap(pCharacter);
}