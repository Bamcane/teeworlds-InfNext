/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <engine/server/mapconverter.h>

#include <game/server/gamecontext.h>
#include <game/mapitems.h>

#include <infnext/classes.h>
#include <infnext/infdefine.h>
#include <infnext/weapons/hammer.h>

#include <infnext/effect.h>

#include "character.h"

//input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}


MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld)
: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER)
{
	m_ProximityRadius = ms_PhysSize;
	m_MaxHealth = 10;
	m_Health = 0;
	m_Armor = 0;
}

CCharacter::~CCharacter()
{
	CEffect *p = m_pFirstEffect;
	while(p)
	{
		RemoveEffect(p);

		p = m_pFirstEffect;
	}
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_LastWeapon = WEAPON_HAMMER;
	
	m_QueuedWeapon = -1;

	m_DeepFreeze = 0;
	m_FreezeStartTick = 0;
	m_FreezeEndTick = 0;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	m_Core.Reset();
	m_Core.Init(&GameServer()->m_World.m_Core, GameServer()->Collision());
	m_Core.m_Pos = m_Pos;
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameServer()->m_World.InsertEntity(this);
	m_Alive = true;
	
	if(pPlayer->GetClass())
	{
		m_Core.m_Infect = pPlayer->GetClass()->m_Infect;
	}

	// infNext init
	InitState();

	GameServer()->m_pController->OnCharacterSpawn(this);

	return true;
}

void CCharacter::InitState()
{
	m_pFirstEffect = 0;

	m_TimeShiftTick = -1;
}

void CCharacter::Destroy()
{
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
}

void CCharacter::SetWeapon(int W)
{
	if(W == m_ActiveWeapon)
		return;

	m_LastWeapon = m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_ActiveWeapon = W;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

	if(m_ActiveWeapon < 0 || m_ActiveWeapon >= NUM_WEAPONS)
		m_ActiveWeapon = 0;
}

bool CCharacter::IsGrounded()
{
	if(GameServer()->Collision()->CheckPoint(m_Pos.x+m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x-m_ProximityRadius/2, m_Pos.y+m_ProximityRadius/2+5))
		return true;
	return false;
}


void CCharacter::HandleNinja()
{
	if(m_ActiveWeapon != WEAPON_NINJA)
		return;

	m_Ninja.m_CurrentMoveTime--;

	if (m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir*m_Ninja.m_OldVelAmount;
	}

	if (m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = m_Pos;
		GameServer()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(m_ProximityRadius, m_ProximityRadius), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = m_Pos - OldPos;
			float Radius = m_ProximityRadius * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameServer()->m_World.FindEntities(Center, Radius, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for (int i = 0; i < Num; ++i)
			{
				if (aEnts[i] == this)
					continue;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for (int j = 0; j < m_NumObjectsHit; j++)
				{
					if (m_apHitObjects[j] == aEnts[i])
						bAlreadyHit = true;
				}
				if (bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if (distance(aEnts[i]->m_Pos, m_Pos) > (m_ProximityRadius * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				GameServer()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT);
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = aEnts[i];

				aEnts[i]->TakeDamage(vec2(0, -10.0f), g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}

		return;
	}

	return;
}


void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	if(!m_pPlayer)
		return;

	int WantedWeapon = m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_ActiveWeapon && m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
	
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	if(m_FreezeEndTick > Server()->Tick())
		return;

	DoWeaponSwitch();

	if(!m_pPlayer->GetClass())
		return;
	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	CWeapon *pWeapon = m_pPlayer->GetClass()->m_pWeapons[m_ActiveWeapon];

	if(!pWeapon)
		return;

	bool FullAuto = pWeapon->IsFullAuto();

	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;

	if(FullAuto && (m_LatestInput.m_Fire&1) && m_aWeapons[m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	// check for ammo
	if(!m_aWeapons[m_ActiveWeapon].m_Ammo)
	{
		// 125ms is a magical limit of how fast a human can click
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		if(m_LastNoAmmoSound+Server()->TickSpeed() <= Server()->Tick())
		{
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
			m_LastNoAmmoSound = Server()->Tick();
		}
		return;
	}

	vec2 ProjStartPos = m_Pos+Direction*m_ProximityRadius*0.75f;
	
	pWeapon->Fire(m_ActiveWeapon ? ProjStartPos : m_Pos, Direction, GetCID());

	m_AttackTick = Server()->Tick();

	if(m_aWeapons[m_ActiveWeapon].m_Ammo > 0) // -1 == unlimited
		m_aWeapons[m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
		m_ReloadTimer = pWeapon->GetFireDelay() * Server()->TickSpeed() / 1000;
}

void CCharacter::HandleWeapons()
{
	//ninja
	HandleNinja();

	// check reload timer
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	// fire Weapon, if wanted
	FireWeapon();

	if(!m_pPlayer->GetClass())
		return;

	// ammo regen
	for(int i=WEAPON_GUN; i<=WEAPON_RIFLE; i++)
	{
		if(!m_aWeapons[i].m_Got)
			continue;
		int AmmoRegenTime = m_pPlayer->GetClass()->m_pWeapons[i]->GetAmmoRegen();
		int MaxAmmo = m_pPlayer->GetClass()->m_pWeapons[i]->GetMaxAmmo();
		
		if(AmmoRegenTime)
		{
			if(m_ReloadTimer <= 0)
			{
				if (m_aWeapons[i].m_AmmoRegenStart < 0)
					m_aWeapons[i].m_AmmoRegenStart = Server()->Tick();

				if ((Server()->Tick() - m_aWeapons[i].m_AmmoRegenStart) >= AmmoRegenTime * Server()->TickSpeed() / 1000)
				{
					// Add some ammo
					m_aWeapons[i].m_Ammo = min(m_aWeapons[i].m_Ammo + 1, MaxAmmo);
					m_aWeapons[i].m_AmmoRegenStart = -1;
				}
			}
		}
	}

	return;
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	if(m_aWeapons[Weapon].m_Ammo < g_pData->m_Weapons.m_aId[Weapon].m_Maxammo || !m_aWeapons[Weapon].m_Got)
	{
		m_aWeapons[Weapon].m_Got = true;
		m_aWeapons[Weapon].m_Ammo = min(g_pData->m_Weapons.m_aId[Weapon].m_Maxammo, Ammo);
		return true;
	}
	return false;
}

void CCharacter::GiveNinja()
{
	m_Ninja.m_ActivationTick = Server()->Tick();
	m_aWeapons[WEAPON_NINJA].m_Got = true;
	m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
	if (m_ActiveWeapon != WEAPON_NINJA)
		m_LastWeapon = m_ActiveWeapon;
	m_ActiveWeapon = WEAPON_NINJA;

	GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA);
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire&1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::HandleEvents()
{
	HandleEffects();

	int Index = Collision()->GetMapIndex(m_Pos);

	// handle speedup tiles
	if(Collision()->IsSpeedup(Index))
	{
		vec2 Direction, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if(Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed / 5);
		}
		else
		{
			if(MaxSpeed > 0 && MaxSpeed < 5)
				MaxSpeed = 5;
			if(MaxSpeed > 0)
			{
				if(Direction.x > 0.0000001f)
					SpeederAngle = -std::atan(Direction.y / Direction.x);
				else if(Direction.x < 0.0000001f)
					SpeederAngle = std::atan(Direction.y / Direction.x) + 2.0f * std::asin(1.0f);
				else if(Direction.y > 0.0000001f)
					SpeederAngle = std::asin(1.0f);
				else
					SpeederAngle = std::asin(-1.0f);

				if(SpeederAngle < 0)
					SpeederAngle = 4.0f * std::asin(1.0f) + SpeederAngle;

				if(TempVel.x > 0.0000001f)
					TeeAngle = -std::atan(TempVel.y / TempVel.x);
				else if(TempVel.x < 0.0000001f)
					TeeAngle = std::atan(TempVel.y / TempVel.x) + 2.0f * std::asin(1.0f);
				else if(TempVel.y > 0.0000001f)
					TeeAngle = std::asin(1.0f);
				else
					TeeAngle = std::asin(-1.0f);

				if(TeeAngle < 0)
					TeeAngle = 4.0f * std::asin(1.0f) + TeeAngle;

				TeeSpeed = std::sqrt(std::pow(TempVel.x, 2) + std::pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - std::cos(DiffAngle) * TeeSpeed;
				if(absolute((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if(absolute((int)SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;
		}
	}

	if(m_Alive && !GameServer()->m_pController->IsGameOver())
	{
		// handle infect-tiles
		if(IsCollisionTile(TILE_TELEINWEAPON) && m_pPlayer->IsHuman())
		{
			m_pPlayer->Infect();
		}

		// handle dead tiles
		if(IsCollisionTile(TILE_DEATH) || GameLayerClipped(m_Pos))
		{
			Die(GetCID(), WEAPON_GAME);
		}
	}

	if(!m_pPlayer)
		return;

	if(m_DeepFreeze)
	{
		Freeze(30.0f);
	}

	// set emote
	if (m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}

	UpdateTuning();
}

void CCharacter::HandleInput()
{
	// handle Weapons
	HandleWeapons();

	if(m_FreezeEndTick >= Server()->Tick())
	{
		m_Input.m_Jump = 0;
		m_Input.m_Direction = 0;
		m_Input.m_Hook = 0;
	}
}

void CCharacter::HandleClass()
{
	m_Core.m_MaxJumps = 2;

	if(!m_pPlayer->GetClass())
		return;

	m_Core.m_MaxJumps = m_pPlayer->GetClass()->m_MaxJumpNum;
	
	m_pPlayer->GetClass()->OnTick();
}

void CCharacter::HandleMenu()
{
	m_TimeShiftTick = -1;
	if(m_pPlayer->GetClass())
		return;
	vec2 CursorPos = vec2(m_Input.m_TargetX, m_Input.m_TargetY);

	m_pPlayer->m_MapMenuItem = -1;
	
	if(length(CursorPos) > 80.0f)
	{
		float Angle = 2.0f*pi+atan2(CursorPos.x, -CursorPos.y);
		float AngleStep = 2.0f*pi/static_cast<float>(Server()->Classes()->m_HumanClasses.size());
		m_pPlayer->m_MapMenuItem = ((int)((Angle+AngleStep/2.0f)/AngleStep))%Server()->Classes()->m_HumanClasses.size();
		
		GameServer()->m_pController->OnPlayerSelectClass(m_pPlayer);
	}

}

void CCharacter::HandleEffects()
{
	if(m_pFirstEffect)
		m_pFirstEffect->OnPlayerTick();
}

void CCharacter::Tick()
{
	HandleInput();

	HandleClass();

	HandleMenu();

	// handle events
	HandleEvents();

	if(!m_pPlayer)
		return;

	m_Core.m_Input = m_Input;
	m_Core.Tick(true, m_pPlayer->GetNextTuningParams());

	// Previnput
	m_PrevInput = m_Input;
	return;
}

void CCharacter::TickDefered()
{
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision());
		m_ReckoningCore.Tick(false, &TempWorld.m_Tuning);
		m_ReckoningCore.Move(&TempWorld.m_Tuning);
		m_ReckoningCore.Quantize();
	}

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.Move(m_pPlayer->GetNextTuningParams());
	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		}StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	int Events = m_Core.m_TriggeredEvents;
	int Mask = CmaskAllExceptOne(m_pPlayer->GetCID());

	if(Events&COREEVENT_GROUND_JUMP) GameServer()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, Mask);

	if(Events&COREEVENT_HOOK_ATTACH_PLAYER) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, CmaskAll());
	if(Events&COREEVENT_HOOK_ATTACH_GROUND) GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, Mask);
	if(Events&COREEVENT_HOOK_HIT_NOHOOK) GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, Mask);


	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_ReckoningTick+Server()->TickSpeed()*3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}


void CCharacter::SaturateVelocity(vec2 Force, float MaxSpeed)
{
	if(length(Force) < 0.00001)
		return;
	
	float Speed = length(m_Core.m_Vel);
	vec2 VelDir = normalize(m_Core.m_Vel);
	if(Speed < 0.00001)
	{
		VelDir = normalize(Force);
	}
	vec2 OrthoVelDir = vec2(-VelDir.y, VelDir.x);
	float VelDirFactor = dot(Force, VelDir);
	float OrthoVelDirFactor = dot(Force, OrthoVelDir);
	
	vec2 NewVel = m_Core.m_Vel;
	if(Speed < MaxSpeed || VelDirFactor < 0.0f)
	{
		NewVel += VelDir*VelDirFactor;
		float NewSpeed = length(NewVel);
		if(NewSpeed > MaxSpeed)
		{
			if(VelDirFactor > 0.f)
				NewVel = VelDir*MaxSpeed;
			else
				NewVel = -VelDir*MaxSpeed;
		}
	}
	
	NewVel += OrthoVelDir * OrthoVelDirFactor;
	
	m_Core.m_Vel = NewVel;
}

void CCharacter::TickPaused()
{
	++m_AttackTick;
	++m_DamageTakenTick;
	++m_Ninja.m_ActivationTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_EmoteStop > -1)
		++m_EmoteStop;
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 10)
		return false;
	m_Health = clamp(m_Health+Amount, 0, 10);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor+Amount, 0, 10);
	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	// we got to wait 0.5 secs before respawning
	m_pPlayer->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = ModeSpecial;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	// a nice sound
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	// infection
	if(m_pPlayer->IsHuman() && GameServer()->m_pController->IsInfectionStarted())
		m_pPlayer->Infect();

	// this is for auto respawn after 3 secs
	m_pPlayer->m_DieTick = Server()->Tick();
	
	DestroyChildEntites();

	m_Alive = false;
	GameServer()->m_World.RemoveEntity(this);
	GameServer()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID());
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon, int Mode)
{
	if(Mode != DAMAGEMODE_DMGALL && GameServer()->m_pController->IsFriendlyFire(m_pPlayer->GetCID(), From))
		return false;

	m_Core.m_Vel += Force;

	if(Mode == DAMAGEMODE_INFECTION && From >= 0 && From != m_pPlayer->GetCID())
	{
		m_pPlayer->Infect();
		GameServer()->m_apPlayers[From]->m_Score += 3;
		GameServer()->SendChatTarget_Localization(From, _("You infected '{str:Player}'"), "Player", Server()->ClientName(GetCID()), NULL);
		GameServer()->SendChatTarget_Localization(GetCID(), _("You're infected by '{str:Player}'"), "Player", Server()->ClientName(From), NULL);
		
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "kill killer='%s' victim='%s' weapon=%d",
			Server()->ClientName(From),
			Server()->ClientName(m_pPlayer->GetCID()), Weapon);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
		
		CNetMsg_Sv_KillMsg Msg;
		Msg.m_Killer = From;
		Msg.m_Victim = m_pPlayer->GetCID();
		Msg.m_Weapon = Weapon;
		Msg.m_ModeSpecial = 0;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

		if(GameServer()->m_apPlayers[From])
			GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_CTF_GRAB_PL, CmaskOne(From));
		return false;
	}

	// m_pPlayer only inflicts half damage on self
	if(From == m_pPlayer->GetCID())
	{
		if(Mode == DAMAGEMODE_DMGSELF || Mode == DAMAGEMODE_DMGALL)
			Dmg = max(1, Dmg/2);
		else return false;
	}

	m_DamageTaken++;

	// create healthmod indicator
	if(Server()->Tick() < m_DamageTakenTick+25)
	{
		// make sure that the damage indicators doesn't group together
		GameServer()->CreateDamageInd(m_Pos, m_DamageTaken*0.25f, Dmg);
	}
	else
	{
		m_DamageTaken = 0;
		GameServer()->CreateDamageInd(m_Pos, 0, Dmg);
	}

	if(Dmg)
	{
		if(m_Armor)
		{
			if(Dmg > 1)
			{
				m_Health--;
				Dmg--;
			}

			if(Dmg > m_Armor)
			{
				Dmg -= m_Armor;
				m_Armor = 0;
			}
			else
			{
				m_Armor -= Dmg;
				Dmg = 0;
			}
		}

		m_Health -= Dmg;
	}

	m_DamageTakenTick = Server()->Tick();

	// do damage Hit sound
	if(From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
	{
		int64 Mask = CmaskOne(From);
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_SpectatorID == From)
				Mask |= CmaskOne(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
	}

	// check for death
	if(m_Health <= 0)
	{
		// set attacker's face to happy (taunt!)
		if (From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
		{
			CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
			if (pChr)
			{
				pChr->m_EmoteType = EMOTE_HAPPY;
				pChr->m_EmoteStop = Server()->Tick() + Server()->TickSpeed();
			}
		}

		Die(From, Weapon);

		return false;
	}

	if (Dmg > 2)
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
	else
		GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);

	m_EmoteType = EMOTE_PAIN;
	m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;

	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	int Id = m_pPlayer->GetCID();

	if (!Server()->Translate(Id, SnappingClient))
		return;
	CCharacterCore *pCore;
	int Tick, Emote = m_EmoteType, Weapon = m_ActiveWeapon, AmmoCount = 0,
		  Health = 0, Armor = 0;

	if(!m_ReckoningTick || GameServer()->m_World.m_Paused)
	{
		Tick = 0;
		pCore = &m_Core;
	}
	else
	{
		Tick = m_ReckoningTick;
		pCore = &m_SendCore;
	}

	Health = m_Health;
	Armor = m_Armor;
	AmmoCount = m_aWeapons[Weapon].m_Ammo;

	CNetObj_DDNetCharacter *pDDNetCharacter = static_cast<CNetObj_DDNetCharacter *>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, Id, sizeof(CNetObj_DDNetCharacter)));
	if(!pDDNetCharacter)
		return;

	pDDNetCharacter->m_Flags = 0;

	if(m_pPlayer->IsInfect())
		Emote = EMOTE_ANGRY;

	if(m_aWeapons[WEAPON_HAMMER].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_HAMMER;
	if(m_aWeapons[WEAPON_GUN].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_GUN;
	if(m_aWeapons[WEAPON_SHOTGUN].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_SHOTGUN;
	if(m_aWeapons[WEAPON_GRENADE].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_GRENADE;
	if(m_aWeapons[WEAPON_RIFLE].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_RIFLE;
	if(m_aWeapons[WEAPON_NINJA].m_Got)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_NINJA;

	pDDNetCharacter->m_NinjaActivationTick = Server()->Tick();

	pDDNetCharacter->m_FreezeStart = 0;
	pDDNetCharacter->m_FreezeEnd =	0;

	pDDNetCharacter->m_Jumps = m_Core.m_MaxJumps;
	pDDNetCharacter->m_JumpedTotal = m_Core.m_JumpCounter;
	pDDNetCharacter->m_TeleCheckpoint = 0;
	pDDNetCharacter->m_StrongWeakID = 0; // ???

	if(m_FreezeEndTick >= Server()->Tick())
	{
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_IN_FREEZE;
		pDDNetCharacter->m_FreezeStart = m_FreezeStartTick;
		pDDNetCharacter->m_FreezeEnd =	m_DeepFreeze ? -1 : m_FreezeEndTick;
		pDDNetCharacter->m_Jumps = 0;
	}

	pDDNetCharacter->m_TargetX = m_Core.m_Input.m_TargetX;
	pDDNetCharacter->m_TargetY = m_Core.m_Input.m_TargetY;
		
	if(GetClass())
		GetClass()->OnDDNetCharacterSnap(pDDNetCharacter);

	if(!Server()->IsSixup(SnappingClient))
	{
		CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, Id, sizeof(CNetObj_Character)));
		if(!pCharacter)
			return;

		pCore->Write(pCharacter);

		pCharacter->m_Tick = Tick;
		pCharacter->m_Emote = Emote;

		if(pCharacter->m_HookedPlayer != -1)
		{
			if(!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
				pCharacter->m_HookedPlayer = -1;
		}

		pCharacter->m_AttackTick = m_AttackTick;
		pCharacter->m_Direction = m_Input.m_Direction;
		pCharacter->m_Weapon = Weapon;
		pCharacter->m_AmmoCount = AmmoCount;
		pCharacter->m_Health = Health;
		pCharacter->m_Armor = Armor;
		pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;

		if(GetClass())
			GetClass()->OnCharacterSnap(pCharacter);
			
		if(m_pFirstEffect)
			m_pFirstEffect->OnCharacterSnap(pCharacter);
	}
	else
	{
		protocol7::CNetObj_Character *pCharacter = static_cast<protocol7::CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, Id, sizeof(protocol7::CNetObj_Character)));
		if(!pCharacter)
			return;

		pCore->Write(reinterpret_cast<CNetObj_CharacterCore *>(static_cast<protocol7::CNetObj_CharacterCore *>(pCharacter)));
		if(pCharacter->m_Angle > (int)(pi * 256.0f))
		{
			pCharacter->m_Angle -= (int)(2.0f * pi * 256.0f);
		}

		pCharacter->m_Tick = Tick;
		pCharacter->m_Emote = Emote;
		pCharacter->m_AttackTick = m_AttackTick;
		pCharacter->m_Direction = m_Input.m_Direction;
		pCharacter->m_Weapon = Weapon;
		pCharacter->m_AmmoCount = AmmoCount;

		if(Weapon == WEAPON_NINJA)
			pCharacter->m_AmmoCount = m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;

		pCharacter->m_Health = Health;
		pCharacter->m_Armor = Armor;
		pCharacter->m_TriggeredEvents = 0;

		if(GetClass())
			GetClass()->OnCharacterSnap(pCharacter);

		if(m_pFirstEffect)
			m_pFirstEffect->OnCharacterSnap(pCharacter);
	}
}

int CCharacter::GetCID() const
{
	return m_pPlayer->GetCID();
}

CClass *CCharacter::GetClass() const
{
	return m_pPlayer->GetClass();
}

bool CCharacter::IsHuman() const
{
	return m_pPlayer->IsHuman();
}

bool CCharacter::IsInfect() const
{
	return m_pPlayer->IsInfect();
}

void CCharacter::Freeze(float Seconds)
{
	if(!m_Alive || m_FreezeEndTick > Server()->Tick())
		return;
	m_FreezeStartTick = Server()->Tick();
	m_FreezeEndTick = m_FreezeStartTick + Server()->TickSpeed() * Seconds;
}

void CCharacter::AddEffect(CEffect *pEffect)
{
	for(CEffect *p = m_pFirstEffect; p; p = p->m_pNextEffect)
	{
		if(pEffect->m_Type == p->m_Type)
		{
			delete pEffect;
			return;
		}
		if(!p->m_pNextEffect)
		{
			p->m_pNextEffect = pEffect;
			return;
		}
	}

	m_pFirstEffect = pEffect;
}

void CCharacter::RemoveEffect(CEffect *pEffect)
{
	if(pEffect == m_pFirstEffect)
	{
		m_pFirstEffect = pEffect->m_pNextEffect;
		return;
	}

	CEffect *pLastEffect = 0;
	for(CEffect *p = m_pFirstEffect; p; p = p->m_pNextEffect)
	{
		if(p->m_pNextEffect == pEffect)
		{
			pLastEffect = p;
			break;
		}
	}

	bool test = pLastEffect;
	dbg_assert(test, "Effect is invalid");

	if(!pEffect->m_pNextEffect)
		pLastEffect->m_pNextEffect = 0;
	else pLastEffect->m_pNextEffect = pEffect->m_pNextEffect;

	delete pEffect;
}

void CCharacter::DestroyChildEntites()
{
	for(int i = 0; i < CGameWorld::NUM_ENTTYPES; i++)
		for(CEntity *pEnt = GameWorld()->FindFirst(i); pEnt; pEnt = pEnt->TypeNext())
		{
			if(pEnt->GetOwner() == GetCID())
				GameWorld()->DestroyEntity(pEnt);
		}
}

bool CCharacter::IsCollisionTile(int Flags)
{
	return 
		Collision()->IsCollisionTile(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f, Flags) ||
		Collision()->IsCollisionTile(m_Pos.x+m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f, Flags) ||
		Collision()->IsCollisionTile(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y-m_ProximityRadius/3.f, Flags) ||
		Collision()->IsCollisionTile(m_Pos.x-m_ProximityRadius/3.f, m_Pos.y+m_ProximityRadius/3.f, Flags);
}

void CCharacter::UpdateTuning()
{
	m_pPlayer->GetNextTuningParams();
}

CCollision *CCharacter::Collision()
{
	return GameServer()->Collision();
}

void CCharacter::InitClassWeapon()
{
	m_ActiveWeapon = WEAPON_HAMMER;
	if(!m_pPlayer->GetClass())
	{
		m_aWeapons[WEAPON_HAMMER].m_Ammo = -1;
		m_aWeapons[WEAPON_HAMMER].m_Got = true;
		return;
	}

	for(int i = 0;i < NUM_WEAPONS;i ++)
	{
		m_aWeapons[i].m_Ammo = 0;
		m_aWeapons[i].m_Got = false;

		if(!m_pPlayer->GetClass()->m_pWeapons[i])
			continue;
		
		m_aWeapons[i].m_Ammo = m_pPlayer->GetClass()->m_pWeapons[i]->GetInitAmmo();
		m_aWeapons[i].m_Got = true;
	}
}