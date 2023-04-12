#include <game/server/gamecontext.h>
#include <infnext/entities/projectile.h>
#include "medic-shotgun.h"

CWeaponMedicShotgun::CWeaponMedicShotgun(CGameContext *pGameServer)
    : CWeaponShotgun(pGameServer)
{
    m_AmmoRegen = 750;
    m_FireDelay = 250;
}

void CWeaponMedicShotgun::Fire(vec2 Pos, vec2 Dir, int Owner)
{
    int ShotSpread = 2;

    for(int i = -ShotSpread; i <= ShotSpread; ++i)
    {
        float Spreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
        float a = GetAngle(Dir);
        a += Spreading[i+2];
        float v = 1-(absolute(i)/(float)ShotSpread);
        float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
        new CProjectile(GameWorld(), WEAPON_SHOTGUN,
            Owner,
            Pos,
            vec2(cosf(a), sinf(a))*Speed,
            (int)(GameServer()->Server()->TickSpeed()*GameServer()->Tuning()->m_ShotgunLifetime),
            1, 0, 10, -1, WEAPON_SHOTGUN);
    }

    GameServer()->CreateSound(Pos, SOUND_SHOTGUN_FIRE);
    
    return;
}