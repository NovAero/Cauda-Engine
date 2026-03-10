#pragma once
#include "Core/Character/CharacterModule.h"
#include "Core/Input/InputSystem.h"
#include "InputComponent.h"

class SensorModule;
class PhysicsModule;
class RopeModule;
class ConstraintModule;

class CharacterController : public IGamepadListener, IKeyListener
{
public:
	CharacterController(flecs::world& world, SDL_JoystickID controllerID = 0);
	~CharacterController();

	void Update(float deltaTime);

	bool IsPossessing() const;

	void PossessFirstAvailable();
	void PossessWithID(int characterID);
	void DetachFromPawn();

	void DisableInput();
	void EnableInput();

	virtual void OnButtonDown(Uint8 button) override;
	virtual void OnButtonHeld(Uint8 button, int heldTicks) override;
	virtual void OnButtonUp(Uint8 button) override;

	virtual void OnLeftAxisInput(float xValue, float yValue) override;
	virtual void OnRightAxisInput(float xValue, float yValue) override;

	virtual void OnTriggerDown(bool left) override;
	virtual void OnTriggerUp(bool left) override;

	virtual void OnKeyPress(PKey key) override;
	virtual void OnKeyRelease(PKey key) override;

	flecs::entity GetPosessedPawn() { return m_possessedPawn; }

	int GetGamepadSocket() { return m_gamepadSocket; }

	bool TryAddAmmo(int type);

	void CachePlayerIDForReload();
	void TryReposessCachedPlayerID();
	void ResetCachedID();
	void ResetReadyState();

	bool IsPlayerReadied() { return m_readied; }
private:

	void Grab();
	void Grab(flecs::entity target);
	void Release();
	void CleanupConstraints();

	flecs::world& m_world;

	flecs::entity m_possessedPawn;
	flecs::entity m_colliderChild; //Replace with shape casts down the line
	flecs::entity m_ropeChild;

	std::queue<JPH::Ref<JPH::Constraint>> m_cachedGrabConstraints;

	flecs::entity m_heldEntity;

	flecs::entity m_harpoonLauncher = flecs::entity::null();
	flecs::entity m_projectileLauncher = flecs::entity::null();
	int m_currentProjectileType = 0;

	CharacterModule* m_characterModule;
	PhysicsModule* m_physicsModule;
	SensorModule* m_sensorModule;
	RopeModule* m_ropeModule;
	ConstraintModule* m_constraintModule;

	flecs::entity ropeEntity = flecs::entity::null();

	int m_cachedPlayerID = -1;

	int m_gamepadSocket = -1;

	bool m_readied = false;
};