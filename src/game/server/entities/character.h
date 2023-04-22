/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H

#include <game/server/entity.h>
#include <game/generated/server_data.h>
#include <game/generated/protocol.h>

#include <game/gamecore.h>

enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

class CEffect;

class CCharacter : public CEntity
{
	MACRO_ALLOC_POOL_ID()

public:
	//character's size
	static const int ms_PhysSize = 28;

	CCharacter(CGameWorld *pWorld);
	~CCharacter();

	void Reset() override;
	void Destroy() override;
	void Tick() override;
	void TickDefered() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	bool IsGrounded();

	void SetWeapon(int W);
	void HandleWeaponSwitch();
	void DoWeaponSwitch();

	void HandleWeapons();
	void HandleNinja();
	void HandleEvents();
	void HandleInput();
	void HandleClass();
	void HandleMenu();
	void HandleEffects();

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ResetInput();
	void FireWeapon();

	void Die(int Killer, int Weapon);
	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon, int Mode = 0);

	bool Spawn(class CPlayer *pPlayer, vec2 Pos);
	void InitState();

	bool Remove();

	bool IncreaseHealth(int Amount);
	bool IncreaseArmor(int Amount);

	bool GiveWeapon(int Weapon, int Ammo);
	void GiveNinja();

	void SetEmote(int Emote, int Tick);

	bool IsAlive() const { return m_Alive; }
	class CPlayer *GetPlayer() { return m_pPlayer; }

private:
	// player controlling this character
	class CPlayer *m_pPlayer;

	bool m_Alive;

	// weapon info
	CEntity *m_apHitObjects[10];
	int m_NumObjectsHit;

	struct WeaponStat
	{
		int m_AmmoRegenStart;
		int m_Ammo;
		bool m_Got;

	} m_aWeapons[NUM_WEAPONS];

	int m_ActiveWeapon;
	int m_LastWeapon;
	int m_QueuedWeapon;

	int m_ReloadTimer;
	int m_AttackTick;

	int m_DamageTaken;

	int m_EmoteType;
	int m_EmoteStop;

	// last tick that the player took any action ie some input
	int m_LastAction;
	int m_LastNoAmmoSound;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_PrevInput;
	CNetObj_PlayerInput m_Input;
	int m_NumInputs;
	int m_Jumped;

	int m_DamageTakenTick;

	int m_Health;
	int m_Armor;
	int m_MaxHealth;

	// ninja
	struct NinjaInfo
	{
		vec2 m_ActivationDir;
		int m_ActivationTick;
		int m_CurrentMoveTime;
		int m_OldVelAmount;
	} m_Ninja;

	// the player core for the physics
	CCharacterCore m_Core;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

private:
	bool IsCollisionTile(int Zone, int Flags);
	void UpdateTuning();
	int m_FreezeStartTick;
	int m_FreezeEndTick;

	CEffect* m_pFirstEffect;

public:
	CCollision *Collision();
	CCharacterCore *GetCore() {return &m_Core;}
	WeaponStat *GetWeaponStat() {return m_aWeapons;}
	CNetObj_PlayerInput *GetInput() {return &m_Input;}
	CNetObj_PlayerInput *GetPrevInput() {return &m_PrevInput;}
	CNetObj_PlayerInput *GetLatestInput() {return &m_LatestInput;}
	int GetActiveWeapon() {return m_ActiveWeapon; }
	int GetHealth() const {return m_Health; }
	int GetMaxHealth() const {return m_MaxHealth; }
	int GetCID() const;

	CClass *GetClass() const;
	bool IsHuman() const;
	bool IsInfect() const;

	void AddEffect(CEffect *pEffect);
	void RemoveEffect(CEffect *pEffect);
	void EffectsSnap();

/** Weapon Public for weapon system*/
	NinjaInfo *GetNinjaInfo() {return &m_Ninja;}
	void SetReloadTimer(int ReloadTimer) { m_ReloadTimer = ReloadTimer;}

	void Freeze(float Seconds);
	void SaturateVelocity(vec2 Force, float MaxSpeed);
	void InitClassWeapon();
	bool m_DeepFreeze;

	int m_LastHookDmgTick;
	int m_TimeShiftTick;
};

#endif
