#pragma once
#include "Core/Input/InputSystem.h"
#include "Core/Physics/PhysicsModule.h"
#include <Jolt/Physics/Character/Character.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <ThirdParty/flecs.h>
//TODO a programmer should comment this so Finn can guess what is happening

struct InputComponent;
struct TransformComponent;
struct Grabbable;
struct AnchorPoint;

struct PlayerStatComponent
{
	int health = 3;
	int numLives = 3;
	bool diedFirst = false;
	float damageTaken = 0.f;
	float damageDealt = 0.f;
	float timeAlive = 0.f;
	int shotsFired = 0;
	int shotsHit = 0;
	int arrowsParried = 0;
	int harpoonsParried = 0;
	int shieldBashes = 0;
	int timesShieldBashed = 0;
	int ammoPickedUp = 0;
	int recycledAmmoPickedUp = 0;
	int shotsAttemptedNoAmmo = 0;
	int ammoHoarded = 0;
	float timeSpentAnchored = 0.f;
	float poiseDamageTaken = 0.f;
};

struct CharacterComponent
{
	float maxSpeed = 6.0f;
	float jumpPower = 8.0f;
	float acceleration = 50.0f;
	float airControl = 0.05f;
	float friction = 25.0f;
	float restitution = 0.5f;
	float halfHeight = 0.4f;
	float radius = 0.4f;
	float mass = 40.0f;
	float maxSlopeAngle = 60.0f;
	float characterFriction = 0.0f;
	float gravityFactor = 2.35f;
	float motorStrength = 300.f;//300000.0f;
	float motorDamping = 1.f;//15000.0f;
	float verticalMotorStrength = 300.f;//300000.0f;
	bool enableMotors = false;
	float groundCheckDistance = 1.6f;

	int characterID = 0;

	bool enableSuspension = true;
	float maxSuspensionTravel = 1.6f;
	float springRate = 50.0f;
	float damperRate = 50.0f;

	// Runtime suspension data
	bool groundContact = false;
	glm::vec3 contactPoint = glm::vec3(0.0f);
	glm::vec3 surfaceNormal = glm::vec3(0, 1, 0);
	glm::vec3 smoothedSurfaceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
	float normalSmoothingFactor = 0.1f;
	float suspensionCompression = 0.0f;

	float maxLinearVelocity = 75.f;
	glm::vec3 velocity = glm::vec3(0.0f);
	glm::vec3 inputDirection = glm::vec3(0.0f);
	glm::vec3 groundVelocity = glm::vec3(0.0f);
	glm::vec3 groundAngularVelocity = glm::vec3(0.0f);
	glm::vec3 jumpStartPosition = glm::vec3(0.0f);
	bool isGrounded = false;
	bool isJumping = false;
	float jumpTime = 0.0f;

	float maxStamina = 100.f;
	float currentStamina = 100.f;

	flecs::entity_t lastHitEntity = 0;
	float sprintMultiplier = 2.0f;
	float staminaDrainRate = 1.0f;

	bool inSafeZone = false;
};


// Custom Event for when Transforms are updated by result of Character System.
struct OnCharacterUpdateTransform {};

class CharacterModule
{
public:
	CharacterModule(flecs::world& world);
	~CharacterModule();

	CharacterModule(const CharacterModule&) = delete;
	CharacterModule& operator=(const CharacterModule&) = delete;
	CharacterModule(CharacterModule&&) = delete;
	CharacterModule& operator=(CharacterModule&&) = delete;

	//void Update(float deltaTime);
	void EnableCharacters(bool enable = true);
	void DisableCharacters() { EnableCharacters(false); }

	void OnImGuiRender();

	void ResolveUnboundEntities();

	JPH::Character* GetCharacter(flecs::entity_t entityId);
	bool HasCharacter(flecs::entity entity) const;
	bool IsGrounded(flecs::entity entity) const;
	glm::vec3 GetPosition(flecs::entity entity) const;
	glm::vec3 GetVelocity(flecs::entity entity) const;
	void SetPosition(flecs::entity entity, const glm::vec3& position);
	JPH::BodyID GetBodyID(flecs::entity entity) const;
	void SetGravityFactor(flecs::entity entity, float factor);
	flecs::entity_t BodyIDtoEntityID(JPH::BodyID bodyID);

	void SetLinearVelocity(flecs::entity entity, const glm::vec3& velocity);
	glm::vec3 GetLinearVelocity(flecs::entity entity) const;
	void AddLinearVelocity(flecs::entity entity, const glm::vec3& velocity);
	void SetLinearAndAngularVelocity(flecs::entity entity, const glm::vec3& linearVel, const glm::vec3& angularVel);

	void AddImpulse(flecs::entity entity, const glm::vec3& impulse);

	void GetPositionAndRotation(flecs::entity entity, glm::vec3& outPosition, glm::quat& outRotation) const;
	void SetPositionAndRotation(flecs::entity entity, const glm::vec3& position, const glm::quat& rotation);
	glm::vec3 GetCenterOfMassPosition(flecs::entity entity) const;
	glm::mat4 GetWorldTransform(flecs::entity entity) const;

	void RespawnPlayerAtPosition(flecs::entity player, TransformComponent& transform, glm::vec3 pos);

	void Activate(flecs::entity entity);

	JPH::TransformedShape GetTransformedShape(flecs::entity entity) const;

	JPH::ObjectLayer GetLayer(flecs::entity entity) const;
	void SetLayer(flecs::entity entity, JPH::ObjectLayer layer);

	JPH::Character::EGroundState GetGroundState(flecs::entity entity) const;
	bool IsSupported(flecs::entity entity) const;
	glm::vec3 GetGroundPosition(flecs::entity entity) const;
	glm::vec3 GetGroundNormal(flecs::entity entity) const;
	glm::vec3 GetGroundVelocity(flecs::entity entity) const;
	const JPH::PhysicsMaterial* GetGroundMaterial(flecs::entity entity) const;
	JPH::BodyID GetGroundBodyID(flecs::entity entity) const;
	uint64_t GetGroundUserData(flecs::entity entity) const;

private:
	struct CharacterData
	{
		JPH::Character* character = nullptr;
		JPH::BodyID anchorBodyId = JPH::BodyID();
		JPH::Constraint* movementConstraint = nullptr;
	};

	bool m_showCharacterWindow = true;

	JPH::ShapeRefC m_characterShape;

	flecs::world& m_world;
	PhysicsModule* m_physicsModule = nullptr;
	class EditorModule* m_editorModule = nullptr;
	class CameraModule* m_cameraModule = nullptr;

	flecs::system m_updateSystem;
	flecs::system m_resolveUnboundEntitiesSystem;
	//flecs::query<TransformComponent, CharacterComponent> m_updateQuery;
	std::unordered_map<flecs::entity_t, CharacterData> m_characters;

private:
	void SetupComponents();
	void SetupSystems();
	void SetupObservers();
	void SetupQueries();

	void RegisterWithEditor();

	void UpdateInternalPositionAndRotation(flecs::entity entity, const TransformComponent& transform);
	CharacterData CreateCharacterInternal(const CharacterComponent& component, const glm::vec3& position, uint64_t userData);
	void UpdateCharacterInternal(flecs::entity entity, const CharacterComponent& component);
	void DestroyCharacterInternal(flecs::entity_t entityId);
	void CreateMotorSystem(CharacterData& data, JPH::BodyID characterBodyId, const CharacterComponent& component);
	JPH::Constraint* CreateMovementConstraint(JPH::BodyID anchorBody, JPH::BodyID characterBody, const CharacterComponent& component);

	JPH::CharacterSettings CreateCharacterSettings(const CharacterComponent& component);
	void UpdateCharacterPhysics(flecs::entity entity, CharacterComponent& character,
		TransformComponent& transform, float deltaTime);
	void UpdateSuspension(flecs::entity entity, CharacterComponent& character, const TransformComponent& transform);
	void UpdateMovementConstraint(flecs::entity entity, CharacterComponent& character, JPH::BodyID bodyId, float deltaTime);
	void ApplySuspensionForces(flecs::entity entity, CharacterComponent& character, JPH::BodyID bodyId, float deltaTime);

	void OnCharacterAdded(flecs::entity entity, CharacterComponent& component);
	void OnCharacterRemoved(flecs::entity entity, CharacterComponent& component);

	void DrawPlayerStatsComponent(flecs::entity entity, PlayerStatComponent& component);
	void DrawInputComponent(flecs::entity entity, InputComponent& component);
	void DrawAnchorPointInspector(flecs::entity entity, AnchorPoint& component);
	void DrawCharacterInspector(flecs::entity entity, CharacterComponent& component);
	ImVec4 GetCharacterEntityColour(flecs::entity entity);
};
