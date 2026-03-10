#include "cepch.h"
#include "CharacterModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Math/TransformModule.h"
#include "Core/Scene/CameraModule.h"
#include "Core/Physics/ConstraintModule.h"
#include "CharacterController.h"
#include "Core/Components/GrabComponent.h"
#include "Core/Physics/RopeModule.h"
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <iostream>
#include <imgui.h>
#include "Platform/Win32/Application.h"

namespace ObjectLayers
{
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer NO_COLLISION = 2;
	static constexpr JPH::ObjectLayer KINEMATIC_SENSOR(3);
	static constexpr JPH::ObjectLayer ROPE(4);
	static constexpr JPH::ObjectLayer ROPE_EVEN(5);
	static constexpr JPH::ObjectLayer CHARACTER = 6;
	static constexpr JPH::ObjectLayer PROJECTILE = 7;
	static constexpr JPH::ObjectLayer PROJECTILE_ONLY = 8;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 9;
};

CharacterModule::CharacterModule(flecs::world& world) : m_world(world)
{
	m_editorModule = m_world.try_get_mut<EditorModule>();
	m_cameraModule = m_world.try_get_mut<CameraModule>();
	m_physicsModule = m_world.try_get_mut<PhysicsModule>();

	SetupComponents();
	SetupSystems();
	SetupObservers();
	SetupQueries();

	RegisterWithEditor();

	JPH::CapsuleShapeSettings capsuleSettings(0.8f, 0.4f);
	auto shapeResult = capsuleSettings.Create();
	if (shapeResult.IsValid())
		m_characterShape = shapeResult.Get();


	EditorConsole::Inst()->AddConsoleMessage("CharacterModule initialised successfully");
}

CharacterModule::~CharacterModule()
{
	Logger::PrintLog("CharacterModule shutdown starting...");

	if (m_resolveUnboundEntitiesSystem && m_resolveUnboundEntitiesSystem.is_alive())
		m_resolveUnboundEntitiesSystem.destruct();

	m_characters.clear();

	Logger::PrintLog("CharacterModule shutdown completed.");
}


void CharacterModule::EnableCharacters(bool enable)
{
	if (enable)
		m_updateSystem.enable();
	else
		m_updateSystem.disable();
}


void CharacterModule::OnImGuiRender()
{
	if (m_showCharacterWindow)
	{
		ImGui::Begin("Character Module", &m_showCharacterWindow);
		ImGui::End();
	}
}

void CharacterModule::ResolveUnboundEntities()
{
	m_resolveUnboundEntitiesSystem.run();
}

void CharacterModule::SetupComponents()
{
	m_world.component<InputComponent>()
		.member<bool>("useRawInput")
		.member<float>("rotationSpeed")
		.member<float>("disconnectHoldTime")
		.add(flecs::Exclusive);

	m_world.component<PlayerStatComponent>()
		.member<int>("numLives");

	m_world.component<CharacterComponent>()
		.member<float>("maxSpeed")
		.member<float>("jumpHeight")
		.member<float>("acceleration")
		.member<float>("airControl")
		.member<float>("friction")
		.member<float>("restitution")
		.member<float>("halfHeight")
		.member<float>("radius")
		.member<float>("mass")
		.member<float>("maxSlopeAngle")
		.member<float>("characterFriction")
		.member<float>("gravityFactor")
		.member<float>("motorStrength")
		.member<float>("motorDamping")
		.member<float>("verticalMotorStrength")
		.member<bool>("enableMotors")
		.member<float>("groundCheckDistance")
		.member<int>("characterID")
		.member<bool>("enableSuspension")
		.member<float>("maxSuspensionsTravel")
		.member<float>("springRate")
		.member<float>("damperRate")
		.add(flecs::With, m_world.component<InputComponent>());


	m_world.component<Grabbable>();

	m_world.component<AnchorPoint>()
		.member<int>("maxAttachments");
}

void CharacterModule::SetupSystems()
{
	flecs::entity GameTick = Cauda::Application::GetGameTickSource();
	//flecs::entity PhysicsPhase = Cauda::Application::GetPhysicsPhase();
	//flecs::entity ResolvePhysicsPhase = Cauda::Application::ResolvePhysicsPhase();

	m_resolveUnboundEntitiesSystem = m_world.system<TransformComponent, CharacterComponent>("ResolveUnboundCharacters")
		.kind(flecs::PreUpdate)
		.tick_source(GameTick)
		.each([this](flecs::entity e, TransformComponent& transform, CharacterComponent& character)
			{
				if (!e.is_alive() || m_characters.contains(e.id()))
					return;

				OnCharacterAdded(e, character);
			});

	m_updateSystem = m_world.system<TransformComponent, CharacterComponent>("CharacterUpdate")
		.kind(flecs::PostUpdate)
		.tick_source(GameTick)
		.each([this](flecs::iter& it, size_t row, TransformComponent& transform, CharacterComponent& character)
			{
				flecs::entity entity = it.entity(row);
				if (!entity.is_alive() || !m_characters.contains(entity.id()))
					return;

				float deltaTime = it.delta_system_time();
				JPH::Character* charPtr = GetCharacter(entity.id());

				UpdateCharacterPhysics(entity, character, transform, deltaTime);
				//if (transform.position.y <= WORLD_KILL_Y)
				//{
				//	auto query = m_world.query_builder<RopeComponent>()
				//		.with(flecs::ChildOf, entity)
				//		.build();

				//	charPtr->SetLinearVelocity({ 0,0,0 });
				//	charPtr->SetPosition({ 0.0, 10, 0.0 });

				//	auto module = m_world.try_get_mut<RopeModule>();
				//	query.each([this, &module, entity](flecs::entity child, RopeComponent& rope)
				//		{
				//			rope.entityBName = "None";
				//			rope.entityB = flecs::entity::null();
				//			module->ConnectRope(child, entity, true);
				//		});
				//}

				//std::cout << "Updated character on PostUpdate\n";

				glm::vec3 euler = Math::GetEulerAngles(transform.rotation);

				transform.cachedEuler = euler;

				charPtr->PostSimulation(1.6f);

				m_world.event<OnCharacterUpdateTransform>()
					.id<TransformComponent>()
					.id<CharacterComponent>()
					.entity(entity)
					.emit();
			});

	

}

void CharacterModule::SetupObservers()
{
	m_world.observer<CharacterComponent>()
		.event(flecs::OnSet)
		.each([this](flecs::entity e, CharacterComponent& character)
			{
				UpdateCharacterInternal(e, character);
				//UpdateMovementConstraint(e, character, );
			});

	m_world.observer<CharacterComponent>()
		.event(flecs::OnRemove)
		.each([this](flecs::entity e, CharacterComponent& character)
			{
				OnCharacterRemoved(e, character);
			});

	m_world.observer<CharacterComponent>()
		.event(flecs::OnDelete)
		.each([this](flecs::entity e, CharacterComponent& character)
			{
				DestroyCharacterInternal(e.id());
			});

	m_world.observer<const TransformComponent>()
		.with<CharacterComponent>().filter()
		.event(flecs::OnSet)
		.each([this](flecs::entity entity, const TransformComponent& transform)
			{
				UpdateInternalPositionAndRotation(entity, transform);
			});

	m_world.observer<CharacterComponent, const TransformComponent>()
		.event<OnCharacterUpdateTransform>().up()
		.each([this](flecs::entity entity, CharacterComponent& character, const TransformComponent& transform)
			{
				m_physicsModule->UpdateJoltBody(entity);
			});
}

void CharacterModule::SetupQueries()
{
	//m_updateQuery = m_world.query_builder<TransformComponent, CharacterComponent>().build();
}

void CharacterModule::RegisterWithEditor()
{
	if (!m_editorModule) return;
	
	m_editorModule->RegisterComponent<CharacterComponent>(
		"Character", "Character",
		[this](flecs::entity entity, CharacterComponent& component)
		{
			DrawCharacterInspector(entity, component);
		}
	);

	m_editorModule->RegisterComponent<InputComponent>("Pawn Input", "Character",
		[this](flecs::entity entity, InputComponent& component)
		{
			DrawInputComponent(entity, component);
		},
		[](flecs::entity entity) -> bool { return false; }
	);

	m_editorModule->RegisterComponent<Grabbable>("Grabbable", "Tags",
		nullptr,
		[](flecs::entity entity) -> bool { return true; }
	);

	m_editorModule->RegisterComponent<AnchorPoint>("Anchor Point", "Constraints",
		[&](flecs::entity entity, AnchorPoint& anchor)
		{
			DrawAnchorPointInspector(entity, anchor);
		},
		[](flecs::entity entity) -> bool { return true; });

	m_editorModule->RegisterComponent<PlayerStatComponent>("Player Stats", "Character",
		[&](flecs::entity entity, PlayerStatComponent& stats)
		{
			DrawPlayerStatsComponent(entity, stats);
		},
		[](flecs::entity entity) -> bool { return true; }
	);
}

void CharacterModule::OnCharacterAdded(flecs::entity entity, CharacterComponent& component)
{
	DestroyCharacterInternal(entity.id());

	entity.add<InputComponent>();

	EditorConsole::Inst()->AddConsoleMessage("OnCharacterAdded called");
	if (entity.try_get<InputComponent>()) {
		EditorConsole::Inst()->AddConsoleMessage("Added pawn component to entity");
	}

	glm::vec3 position = glm::vec3(0, 5, 0);
	if (auto* transform = entity.try_get<TransformComponent>())
	{
		position = transform->position;
	}

	auto characterData = CreateCharacterInternal(component, position, entity.id());
	if (characterData.character)
	{
		m_characters[entity.id()] = characterData;

		component.velocity = glm::vec3(0.0f);
		component.isGrounded = false;
		component.isJumping = false;
		component.jumpTime = 0.0f;

		std::string consoleMessage = "Character created for entity: ";
		consoleMessage.append(entity.name());

		EditorConsole::Inst()->AddConsoleMessage(consoleMessage, false);
	}
}

void CharacterModule::OnCharacterRemoved(flecs::entity entity, CharacterComponent& component)
{
	if (entity.has<InputComponent>())
	{
		entity.remove<InputComponent>();
	}
	DestroyCharacterInternal(entity.id());

	std::string consoleMessage = "Character removed for entity: ";
	consoleMessage.append(entity.name());

	EditorConsole::Inst()->AddConsoleMessage(consoleMessage, false);
}

void CharacterModule::UpdateInternalPositionAndRotation(flecs::entity entity, const TransformComponent& transform)
{
	if (!HasCharacter(entity)) return;

	auto character = GetCharacter(entity.id());
	character->SetPositionAndRotation(PhysicsModule::ToJolt(transform.position), PhysicsModule::ToJoltQuat(transform.rotation), JPH::EActivation::Activate);
}

CharacterModule::CharacterData CharacterModule::CreateCharacterInternal(const CharacterComponent& component, const glm::vec3& position, uint64_t userData)
{
	if (!m_physicsModule) return {};

	JPH::CharacterSettings settings = CreateCharacterSettings(component);
	JPH::Character* character = new JPH::Character(
		&settings,
		PhysicsModule::ToJolt(position),
		JPH::Quat::sIdentity(),
		userData,
		m_physicsModule->GetPhysicsSystem()
	);

	character->AddToPhysicsSystem(JPH::EActivation::Activate);

	CharacterData data;
	data.character = character;

	if (component.enableMotors)
	{
		CreateMotorSystem(data, character->GetBodyID(), component);
	}

	return data;
}

void CharacterModule::UpdateCharacterInternal(flecs::entity entity, const CharacterComponent& component)
{
	if (!HasCharacter(entity)) return;

	auto data = m_characters[entity];
	auto character = data.character;

	JPH::CapsuleShapeSettings capsuleSettings(component.halfHeight, component.radius);
	auto shapeResult = capsuleSettings.Create();
	auto shape = shapeResult.IsValid() ? shapeResult.Get() : m_characterShape;
	character->SetShape(shape, FLT_MAX);

	character->SetMaxSlopeAngle(JPH::DegreesToRadians(component.maxSlopeAngle));
	character->SetLayer(ObjectLayers::CHARACTER);

	auto bodyID = character->GetBodyID();
	m_physicsModule->GetBodyInterface().SetFriction(bodyID, 0.0f);
	m_physicsModule->GetBodyInterface().SetGravityFactor(bodyID, component.gravityFactor);

	// Need to do this to update mass.
	JPH::BodyLockWrite lock(m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface(), bodyID);
	if (lock.Succeeded()) {
		JPH::MotionProperties* motionProperties = lock.GetBody().GetMotionProperties();
		JPH::MassProperties massProperties = lock.GetBody().GetShape()->GetMassProperties();

		// ScaleToMass also scales inertia tensor.
		massProperties.ScaleToMass(component.mass);
		motionProperties->SetMassProperties(JPH::EAllowedDOFs::All, massProperties);
	}

	if (data.anchorBodyId.IsInvalid())
	{
		CreateMotorSystem(data, bodyID, component); // This method should automatically create a movement constraint.
		return;
	}

	if (data.movementConstraint) return;

	data.movementConstraint = CreateMovementConstraint(data.anchorBodyId, bodyID, component);
	if (data.movementConstraint)
	{
		m_physicsModule->GetPhysicsSystem()->AddConstraint(data.movementConstraint);
	}
}

void CharacterModule::CreateMotorSystem(CharacterData& data, JPH::BodyID characterBodyId, const CharacterComponent& component)
{
	if (!m_physicsModule) return;

	JPH::Vec3 position = m_physicsModule->GetBodyInterface().GetPosition(characterBodyId);

	JPH::BoxShapeSettings anchorSettings(JPH::Vec3(0.1f, 0.1f, 0.1f));
	JPH::ShapeSettings::ShapeResult anchorShapeResult = anchorSettings.Create();
	if (!anchorShapeResult.IsValid()) return;

	JPH::BodyCreationSettings anchorBodySettings(
		anchorShapeResult.Get(),
		position,
		JPH::Quat::sIdentity(),
		JPH::EMotionType::Kinematic,
		ObjectLayers::KINEMATIC_SENSOR
	);

	JPH::Body* anchorBody = m_physicsModule->GetBodyInterface().CreateBody(anchorBodySettings);
	if (!anchorBody) return;

	m_physicsModule->GetBodyInterface().AddBody(anchorBody->GetID(), JPH::EActivation::Activate);
	data.anchorBodyId = anchorBody->GetID();

	data.movementConstraint = CreateMovementConstraint(data.anchorBodyId, characterBodyId, component);
	if (data.movementConstraint)
	{
		m_physicsModule->GetPhysicsSystem()->AddConstraint(data.movementConstraint);
	}
	else
	{
		if (!data.anchorBodyId.IsInvalid())
		{
			m_physicsModule->GetBodyInterface().RemoveBody(data.anchorBodyId);
			m_physicsModule->GetBodyInterface().DestroyBody(data.anchorBodyId);
			data.anchorBodyId = JPH::BodyID();
		}
	}
}

JPH::Constraint* CharacterModule::CreateMovementConstraint(JPH::BodyID anchorBody, JPH::BodyID characterBody, const CharacterComponent& component)
{
	JPH::Body* anchorBodyPtr = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterfaceNoLock().TryGetBody(anchorBody);
	JPH::Body* characterBodyPtr = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterfaceNoLock().TryGetBody(characterBody);

	if (!anchorBodyPtr || !characterBodyPtr) return nullptr;

	JPH::SixDOFConstraintSettings settings;
	settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
	settings.mPosition1 = JPH::Vec3::sZero();
	settings.mPosition2 = JPH::Vec3::sZero();
	settings.mAxisX1 = JPH::Vec3::sAxisX();
	settings.mAxisY1 = JPH::Vec3::sAxisY();
	settings.mAxisX2 = JPH::Vec3::sAxisX();
	settings.mAxisY2 = JPH::Vec3::sAxisY();

	// Horizontal movement X and Z
	settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationX);
	settings.mMotorSettings[JPH::SixDOFConstraintSettings::TranslationX] =
		JPH::MotorSettings(10.0f, component.damperRate / component.motorStrength, component.motorStrength, component.motorStrength);

	settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationZ);
	settings.mMotorSettings[JPH::SixDOFConstraintSettings::TranslationZ] =
		JPH::MotorSettings(10.0f, component.damperRate / component.motorStrength, component.motorStrength, component.motorStrength);

	// Vertical movement Y
	settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::TranslationY);
	settings.mMotorSettings[JPH::SixDOFConstraintSettings::TranslationY] =
		JPH::MotorSettings(10.0f, component.damperRate / component.springRate, component.springRate, component.springRate);

	// Rotation constraints
	settings.MakeFixedAxis(JPH::SixDOFConstraintSettings::RotationX);
	settings.MakeFixedAxis(JPH::SixDOFConstraintSettings::RotationZ);
	settings.MakeFreeAxis(JPH::SixDOFConstraintSettings::RotationY);

	JPH::SixDOFConstraint* constraint = static_cast<JPH::SixDOFConstraint*>(settings.Create(*anchorBodyPtr, *characterBodyPtr));
	if (!constraint) return nullptr;

	constraint->SetMotorState(JPH::SixDOFConstraint::EAxis::TranslationX, JPH::EMotorState::Position);
	constraint->SetMotorState(JPH::SixDOFConstraint::EAxis::TranslationZ, JPH::EMotorState::Position);
	constraint->SetMotorState(JPH::SixDOFConstraint::EAxis::TranslationY, JPH::EMotorState::Position);
	constraint->SetMotorState(JPH::SixDOFConstraint::EAxis::RotationY, JPH::EMotorState::Off);

	constraint->SetNumPositionStepsOverride(15);
	constraint->SetNumVelocityStepsOverride(15);

	return constraint;
}

void CharacterModule::DestroyCharacterInternal(flecs::entity_t entityId)
{
	if (m_characters.empty()) return;
	if (!m_physicsModule) return;
	auto it = m_characters.find(entityId);
	if (it != m_characters.end())
	{
		CharacterData& data = it->second;

		if (data.movementConstraint)
		{
			m_physicsModule->GetPhysicsSystem()->RemoveConstraint(data.movementConstraint);
			data.movementConstraint = nullptr;
		}

		if (!data.anchorBodyId.IsInvalid())
		{
			if (m_physicsModule->GetBodyInterface().IsAdded(data.anchorBodyId))
			{
				m_physicsModule->GetBodyInterface().RemoveBody(data.anchorBodyId);
				m_physicsModule->GetBodyInterface().DestroyBody(data.anchorBodyId);
			}
			data.anchorBodyId = JPH::BodyID();
		}

		if (data.character)
		{
			data.character->RemoveFromPhysicsSystem();
			delete data.character;
		}
		m_characters.erase(it);
	}
}

JPH::Character* CharacterModule::GetCharacter(flecs::entity_t entityId)
{
	auto it = m_characters.find(entityId);
	return (it != m_characters.end()) ? it->second.character : nullptr;
}

JPH::CharacterSettings CharacterModule::CreateCharacterSettings(const CharacterComponent& component)
{
	JPH::CapsuleShapeSettings capsuleSettings(component.halfHeight, component.radius);
	auto shapeResult = capsuleSettings.Create();

	JPH::CharacterSettings settings;
	settings.mMaxSlopeAngle = JPH::DegreesToRadians(component.maxSlopeAngle);
	settings.mLayer = ObjectLayers::CHARACTER;
	settings.mShape = shapeResult.IsValid() ? shapeResult.Get() : m_characterShape;
	settings.mFriction = component.characterFriction;
	settings.mMass = component.mass;
	settings.mGravityFactor = component.gravityFactor;

	return settings;
}

//void CharacterModule::Update(float deltaTime)
//{
//	m_updateQuery.each([this, deltaTime](flecs::entity entity, TransformComponent& transform, CharacterComponent& character)
//		{
//			JPH::Character* charPtr = GetCharacter(entity.id());
//			if (!charPtr) return;
//
//			UpdateCharacterPhysics(entity, character, transform, deltaTime);
//			/*if (transform.position.y <= WORLD_KILL_Y)
//			{
//				auto query = m_world.query_builder<RopeComponent>()
//					.with(flecs::ChildOf, entity)
//					.build();
//
//				charPtr->SetLinearVelocity({ 0,0,0 });
//				charPtr->SetPosition({ 0.0, 10, 0.0 });
//
//				auto module = m_world.try_get_mut<RopeModule>();
//				query.each([this, &module, entity](flecs::entity child, RopeComponent& rope)
//					{
//						rope.entityBName = "None";
//						rope.entityB = flecs::entity::null();
//						module->ConnectRope(child, entity, true);
//					});
//			}*/
//
//			charPtr->PostSimulation(1.6f);
//
//			m_world.event<OnCharacterUpdateTransform>()
//				.id<TransformComponent>()
//				.id<CharacterComponent>()
//				.entity(entity)
//				.emit();
//		});
//}

void CharacterModule::UpdateCharacterPhysics(flecs::entity entity, CharacterComponent& character,
	TransformComponent& transform, float deltaTime)
{
	auto pawnInput = entity.try_get_mut<InputComponent>();
	
	if (pawnInput && !pawnInput->isPhysicsOverridden)
	{

		JPH::Character* charPtr = GetCharacter(entity.id());
		if (!charPtr) return;


		auto mainCamera = m_cameraModule->GetMainCamera();
		character.inputDirection = glm::vec3(0.0f);

		if (mainCamera.is_alive() && pawnInput) {
			glm::vec2 moveInput = pawnInput->movementInput;
			if (glm::length(moveInput) > 1.0f) moveInput = glm::normalize(moveInput);
			if (moveInput.x != 0.0f || moveInput.y != 0.0f) {
				auto cameraForward = m_cameraModule->GetForwardVector(mainCamera);
				auto cameraRight = m_cameraModule->GetRightVector(mainCamera);
				glm::vec3 forward = glm::normalize(glm::vec3(-cameraForward.x, 0, -cameraForward.z));
				glm::vec3 right = glm::normalize(glm::vec3(-cameraRight.x, 0, -cameraRight.z));
				character.inputDirection = moveInput.y * forward + -moveInput.x * right;
			}
		}

		bool jumpPressed = pawnInput ? pawnInput->jumpPressed : false;

		UpdateSuspension(entity, character, transform);
		character.isGrounded = character.groundContact;

		if (character.isGrounded && character.isJumping)
		{
			character.isJumping = false;
		}

		ApplySuspensionForces(entity, character, charPtr->GetBodyID(), deltaTime);

		glm::vec3 currentVelocity = PhysicsModule::ToGLM(charPtr->GetLinearVelocity());

		if (character.isGrounded && character.suspensionCompression > 0.01f)
		{

			glm::vec3 groundNormal = character.smoothedSurfaceNormal;

			float slopeDot = glm::dot(groundNormal, glm::vec3(0, 1, 0));
			float currentSlopeAngle = glm::acos(glm::clamp(slopeDot, -1.0f, 1.0f));
			float maxSlopeRad = glm::radians(character.maxSlopeAngle);

			bool slopeTooSteep = currentSlopeAngle > maxSlopeRad;

			if (slopeTooSteep)
			{
				glm::vec3 gravityDir = glm::vec3(0, -1, 0);
				glm::vec3 slideDirection = gravityDir - groundNormal * glm::dot(gravityDir, groundNormal);
				if (glm::length(slideDirection) > 1.0e-6f)
				{
					slideDirection = glm::normalize(slideDirection);
				}

				float slideSpeed = glm::length(glm::vec3(currentVelocity.x, 0, currentVelocity.z));
				float gravity = 9.81f * character.gravityFactor;
				slideSpeed += gravity * glm::sin(currentSlopeAngle) * deltaTime;

				float slideFriction = character.friction * 0.001f;
				slideSpeed = glm::max(0.0f, slideSpeed - slideFriction * deltaTime) * 2.0f;

				slideSpeed = glm::clamp(slideSpeed, 0.0f, character.maxLinearVelocity);

				glm::vec3 targetSlideVel = slideDirection * slideSpeed;
				glm::vec3 currentHorizontalVel = glm::vec3(currentVelocity.x, 0, currentVelocity.z);
				glm::vec3 velocityChange = targetSlideVel - currentHorizontalVel;

				if (glm::length(velocityChange) > 0.001f)
				{
					float maxSlideAccel = character.acceleration * 2.0f;
					if (glm::length(velocityChange) > maxSlideAccel * deltaTime) {
						if (glm::length(velocityChange) > 1.0e-6f)
						{
							velocityChange = glm::normalize(velocityChange) * maxSlideAccel * deltaTime;
						}
					}
					charPtr->AddLinearVelocity(PhysicsModule::ToJolt(velocityChange));
				}

				if (jumpPressed && !character.isJumping)
				{
					float jumpMultiplier = 0.7f;
					charPtr->AddLinearVelocity(JPH::Vec3(0, character.jumpPower * jumpMultiplier, 0));
					character.isJumping = true;
					if (pawnInput) pawnInput->jumpPressed = false;
				}
			}
			else
			{
				glm::vec3 currentVelOnPlane = currentVelocity - groundNormal * glm::dot(currentVelocity, groundNormal);
				glm::vec3 velocityChange(0.0f);

				if (glm::length(character.inputDirection) > 0.1f)
				{
					glm::vec3 right = glm::cross(character.inputDirection, groundNormal);
					if (glm::length(right) > 1.0e-6f)
					{
						right = glm::normalize(right);
					}
					glm::vec3 moveDirection = glm::cross(groundNormal, right);
					if (glm::length(moveDirection) > 1.0e-6f)
					{
						moveDirection = glm::normalize(moveDirection);
					}


					float targetSpeed = character.maxSpeed;

					float slopeSpeedReduction = 1.0f;
					if (currentSlopeAngle > maxSlopeRad * 0.7f)
					{
						float slopeRatio = (currentSlopeAngle - maxSlopeRad * 0.7f) / (maxSlopeRad * 0.3f);
						slopeSpeedReduction = 1.0f - (slopeRatio * 0.5f);
					}
					targetSpeed *= slopeSpeedReduction;

					glm::vec3 targetVelocity = moveDirection * targetSpeed;
					targetVelocity += character.groundVelocity;


					velocityChange = targetVelocity - currentVelOnPlane;

					float accel = character.acceleration;// * slopeSpeedReduction;

					if (glm::length(velocityChange) > accel * deltaTime)
					{
						if (glm::length(velocityChange) > 1.0e-6f)
						{
							velocityChange = glm::normalize(velocityChange) * accel * deltaTime;
						}
					}
				}
				else
				{

					glm::vec3 targetVelocity = character.groundVelocity;

					glm::vec3 targetVelOnPlane = targetVelocity - groundNormal * glm::dot(targetVelocity, groundNormal);

					velocityChange = targetVelOnPlane - currentVelOnPlane;

					float friction = character.friction;

					float slopeSpeedReduction = 1.0f;
					if (currentSlopeAngle > maxSlopeRad * 0.7f)
					{
						float slopeRatio = (currentSlopeAngle - maxSlopeRad * 0.7f) / (maxSlopeRad * 0.3f);
						slopeSpeedReduction = 1.0f - (slopeRatio * 1.0f);
						friction *= slopeSpeedReduction;
					}

					if (glm::length(velocityChange) > friction * deltaTime)
					{
						if (glm::length(velocityChange) > 1.0e-6f)
						{
							velocityChange = glm::normalize(velocityChange) * friction * deltaTime;
						}
					}
				}

				if (glm::length(velocityChange) > 0.001f)
				{
					charPtr->AddLinearVelocity(PhysicsModule::ToJolt(velocityChange));
				}

				if (jumpPressed && !character.isJumping)
				{
					charPtr->AddLinearVelocity(JPH::Vec3(0, character.jumpPower, 0));
					character.isJumping = true;
					if (pawnInput) pawnInput->jumpPressed = false;
				}
			}
		}
		else
		{
			bool isSlidingOnWall = false;
			glm::vec3 wallNormal(0.0f);
			if (glm::length(character.inputDirection) > 0.01f)
			{
				ShapecastHit hit = m_physicsModule->SphereCastWithLayerFilter(
					character.radius, transform.position, glm::normalize(character.inputDirection), 0.15f, {}
				);
				if (hit.hasHit && abs(hit.penetrationAxis.y) < 0.3f && glm::dot(character.inputDirection, hit.penetrationAxis) < 0.0f)
				{
					isSlidingOnWall = true;
					wallNormal = hit.penetrationAxis;
				}
			}

			if (isSlidingOnWall)
			{
				glm::vec3 slideDirection = character.inputDirection - wallNormal * glm::dot(character.inputDirection, wallNormal);
				if (glm::length(slideDirection) > 1.0e-6f)
				{
					slideDirection = glm::normalize(slideDirection);
				}

				glm::vec3 targetHorizontalVel = slideDirection * character.maxSpeed;
				JPH::Vec3 newVelocity = JPH::Vec3(targetHorizontalVel.x, charPtr->GetLinearVelocity().GetY(), targetHorizontalVel.z);
				charPtr->SetLinearVelocity(newVelocity);
			}
			else
			{
				glm::vec3 targetHorizontalVel = character.inputDirection * character.maxSpeed;
				glm::vec3 currentHorizontalVel = glm::vec3(currentVelocity.x, 0, currentVelocity.z);

				float currentSpeed = glm::length(currentHorizontalVel);
				if (currentSpeed > character.maxSpeed)
				{
					if (glm::length(targetHorizontalVel) > 1.0e-6f)
					{
						targetHorizontalVel = glm::normalize(targetHorizontalVel) * character.maxSpeed;
					}
				}

				glm::vec3 velocityChange = (targetHorizontalVel - currentHorizontalVel) * character.airControl;

				charPtr->AddLinearVelocity(JPH::Vec3(velocityChange.x, 0.0f, velocityChange.z));
			}
		}


		JPH::Vec3 finalVelocity = charPtr->GetLinearVelocity();
		float finalSpeed = finalVelocity.Length();

		if (finalSpeed > character.maxLinearVelocity)
		{
			if (finalSpeed > 1.0e-6f)
			{
				charPtr->SetLinearVelocity(finalVelocity.Normalized() * character.maxLinearVelocity);
			}
		}


		transform.position = PhysicsModule::ToGLM(charPtr->GetPosition());
		character.velocity = PhysicsModule::ToGLM(charPtr->GetLinearVelocity());

	}

	if (pawnInput)
	{

		auto mainCamera = m_cameraModule->GetMainCamera();
		glm::vec2 rotationInput = pawnInput->rotationInput;

		if (glm::length(rotationInput) > 0.1f)
		{
			auto cameraForward = m_cameraModule->GetForwardVector(mainCamera);
			auto cameraRight = m_cameraModule->GetRightVector(mainCamera);
			glm::vec3 forward = glm::normalize(glm::vec3(-cameraForward.x, 0, -cameraForward.z));
			glm::vec3 right = glm::normalize(glm::vec3(-cameraRight.x, 0, -cameraRight.z));
			glm::vec3 worldRotationDir = -rotationInput.y * forward + rotationInput.x * right;
			if (glm::length(worldRotationDir) > 1.0e-6f)
			{
				glm::quat targetRotation = glm::quatLookAt(glm::normalize(-worldRotationDir), glm::vec3(0, 1, 0));
				transform.rotation = glm::slerp(transform.rotation, targetRotation, pawnInput->rotationSpeed * deltaTime);
			}
		}
		else
		{
			if (glm::length(character.inputDirection) > 1.0e-6f)
			{
				glm::quat targetRotation = glm::quatLookAt(
					glm::normalize(character.inputDirection),
					glm::vec3(0, 1, 0)
				);
				transform.rotation = glm::slerp(
					transform.rotation,
					targetRotation,
					pawnInput->rotationSpeed * deltaTime
				);
			}
		}
	}
}

void CharacterModule::UpdateSuspension(flecs::entity entity, CharacterComponent& character, const TransformComponent& transform)
{
	character.groundVelocity = glm::vec3(0.0f);
	character.groundAngularVelocity = glm::vec3(0.0f);
	if (!character.enableSuspension || !m_physicsModule) return;

	glm::vec3 castOrigin = transform.position;
	castOrigin.y -= (character.halfHeight + 0.1f);
	glm::vec3 castDirection = glm::vec3(0, -1, 0);

	std::vector<JPH::ObjectLayer> ignoreLayers = {
		ObjectLayers::ROPE,
		ObjectLayers::ROPE_EVEN,
		ObjectLayers::CHARACTER,
		ObjectLayers::KINEMATIC_SENSOR,
		ObjectLayers::NO_COLLISION,
		ObjectLayers::PROJECTILE,
		ObjectLayers::PROJECTILE_ONLY
	};

	ShapecastHit hit = m_physicsModule->SphereCastWithLayerFilter(
		character.radius - 0.01f, castOrigin, castDirection,
		character.maxSuspensionTravel, ignoreLayers
	);

	if (hit.hasHit && hit.hitEntity != entity.id())
	{
		character.groundContact = true;
		character.contactPoint = hit.contactPoint2;
		character.lastHitEntity = hit.hitEntity;

		glm::vec3 sphereCastNormal = -hit.penetrationAxis;
		glm::vec3 joltGroundNormal = GetGroundNormal(entity);

		glm::vec3 targetNormal = sphereCastNormal;

		if (glm::length(character.smoothedSurfaceNormal) < 0.5f)
		{
			character.smoothedSurfaceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
		}

		character.smoothedSurfaceNormal = glm::mix(
			character.smoothedSurfaceNormal,
			targetNormal,
			character.normalSmoothingFactor
		);

		if (glm::length(character.smoothedSurfaceNormal) > 0.01f)
		{
			character.smoothedSurfaceNormal = glm::normalize(character.smoothedSurfaceNormal);
		}
		else
		{
			character.smoothedSurfaceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
		}

		JPH::BodyID groundBodyID = m_physicsModule->GetBodyID(hit.hitEntity);
		if (!groundBodyID.IsInvalid())
		{
			JPH::BodyInterface& bodyInterface = m_physicsModule->GetBodyInterface();
			JPH::EMotionType motionType = bodyInterface.GetMotionType(groundBodyID);
			if (motionType == JPH::EMotionType::Kinematic || motionType == JPH::EMotionType::Dynamic)
			{
				character.groundVelocity = PhysicsModule::ToGLM(bodyInterface.GetLinearVelocity(groundBodyID));

				character.groundAngularVelocity = PhysicsModule::ToGLM(bodyInterface.GetAngularVelocity(groundBodyID));

				if (glm::length(character.groundAngularVelocity) > 0.001f)
				{
					JPH::Vec3 groundCOM = bodyInterface.GetCenterOfMassPosition(groundBodyID);
					glm::vec3 groundCenter = PhysicsModule::ToGLM(groundCOM);
					glm::vec3 radiusVector = character.contactPoint - groundCenter;
					glm::vec3 tangentialVelocity = glm::cross(character.groundAngularVelocity, radiusVector);

					character.groundVelocity += tangentialVelocity;
				}
			}
		}

		float verticalDistance = transform.position.y - hit.contactPoint2.y;
		character.suspensionCompression = 1.0f - glm::clamp(
			(verticalDistance - character.halfHeight) /
			(character.maxSuspensionTravel - character.halfHeight),
			0.0f, 1.0f
		);
	}
	else
	{
		character.groundContact = false;
		character.suspensionCompression = 0.0f;
		character.contactPoint = glm::vec3(0.0f);
		character.lastHitEntity = 0;

		character.smoothedSurfaceNormal = glm::mix(
			character.smoothedSurfaceNormal,
			glm::vec3(0.0f, 1.0f, 0.0f),
			character.normalSmoothingFactor
		);

		if (glm::length(character.smoothedSurfaceNormal) > 0.01f)
		{
			character.smoothedSurfaceNormal = glm::normalize(character.smoothedSurfaceNormal);
		}
		else
		{
			character.smoothedSurfaceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}
}

void CharacterModule::ApplySuspensionForces(flecs::entity entity, CharacterComponent& character,
	JPH::BodyID bodyId, float deltaTime)
{
	if (!character.groundContact || !character.enableSuspension) return;

	auto* transform = entity.try_get<TransformComponent>();
	if (!transform) return;

	float distance = transform->position.y - character.contactPoint.y;

	float springForce = 0.0f;
	if (distance <= character.halfHeight)
	{
		springForce = character.springRate;
	}
	else if (distance < character.maxSuspensionTravel)
	{
		float t = (distance - character.halfHeight) / (character.maxSuspensionTravel - character.halfHeight);
		springForce = character.springRate * (1.0f - t);
	}

	JPH::Vec3 currentVel = m_physicsModule->GetBodyInterface().GetLinearVelocity(bodyId);
	float verticalVelocity = currentVel.GetY();

	float dampingForce = 0.0f;
	if (verticalVelocity < 0.0f)
	{
		float distanceFactor = 1.0f;
		if (distance > character.halfHeight && distance < character.maxSuspensionTravel)
		{
			distanceFactor = 1.0f - ((distance - character.halfHeight) / (character.maxSuspensionTravel - character.halfHeight));
		}
		else if (distance >= character.maxSuspensionTravel)
		{
			distanceFactor = 0.0f;
		}

		dampingForce = -character.damperRate * verticalVelocity * distanceFactor;
	}

	float totalForce = springForce + dampingForce;

	if (abs(totalForce) > 0.01f)
	{
		JPH::Vec3 upwardImpulse(0, totalForce * deltaTime, 0);

		JPH::Character* charPtr = GetCharacter(entity.id());
		if (charPtr)
		{
			charPtr->AddLinearVelocity(upwardImpulse);
		}
	}
}

void CharacterModule::UpdateMovementConstraint(flecs::entity entity, CharacterComponent& character, JPH::BodyID bodyId, float deltaTime)
{
	auto it = m_characters.find(entity.id());
	if (it == m_characters.end() || it->second.anchorBodyId.IsInvalid()) return;

	JPH::SixDOFConstraint* sixDOFConstraint = static_cast<JPH::SixDOFConstraint*>(it->second.movementConstraint);
	if (!sixDOFConstraint) return;

	JPH::Vec3 characterPos = m_physicsModule->GetBodyInterface().GetPosition(bodyId);

	JPH::Vec3 horizontalTargetPos = characterPos;
	if (glm::length(character.inputDirection) > 0.001f)
	{
		glm::vec3 desiredVelocity = character.inputDirection * character.maxSpeed;
		if (!character.isGrounded)
		{
			desiredVelocity *= character.airControl;
		}
		horizontalTargetPos = characterPos + PhysicsModule::ToJolt(desiredVelocity) * deltaTime;
	}

	float targetY = characterPos.GetY();
	bool activateVerticalMotor = false;

	if (character.isJumping)
	{
		float gravity = m_physicsModule->GetPhysicsSystem()->GetGravity().GetY();
		float timeToApex = -character.jumpPower / gravity;

		if (character.jumpTime < timeToApex)
		{
			activateVerticalMotor = true;
		}

		character.jumpTime += deltaTime;
		targetY = character.jumpStartPosition.y +
			(character.jumpPower * character.jumpTime) +
			(0.5f * gravity * character.jumpTime * character.jumpTime);
	}

	JPH::Vec3 finalAnchorPos(horizontalTargetPos.GetX(), targetY, horizontalTargetPos.GetZ());
	m_physicsModule->GetBodyInterface().MoveKinematic(it->second.anchorBodyId, finalAnchorPos, JPH::Quat::sIdentity(), deltaTime);

	// sixDOFConstraint->SetTargetPositionCS(JPH::Vec3::sZero());
	sixDOFConstraint->SetMotorState(JPH::SixDOFConstraint::EAxis::TranslationX, JPH::EMotorState::Position);
	sixDOFConstraint->SetMotorState(JPH::SixDOFConstraint::EAxis::TranslationZ, JPH::EMotorState::Position);

	if (activateVerticalMotor)
	{
		sixDOFConstraint->SetMotorState(JPH::SixDOFConstraint::EAxis::TranslationY, JPH::EMotorState::Position);
	}
	else
	{
		sixDOFConstraint->SetMotorState(JPH::SixDOFConstraint::EAxis::TranslationY, JPH::EMotorState::Off);
	}
}

bool CharacterModule::HasCharacter(flecs::entity entity) const
{
	return m_characters.find(entity.id()) != m_characters.end();
}

bool CharacterModule::IsGrounded(flecs::entity entity) const
{
	auto* character = entity.try_get<CharacterComponent>();
	return character ? character->isGrounded : false;
}

glm::vec3 CharacterModule::GetPosition(flecs::entity entity) const
{
	JPH::Character* character = const_cast<CharacterModule*>(this)->GetCharacter(entity.id());
	if (character)
	{
		return PhysicsModule::ToGLM(character->GetPosition());
	}

	auto* transform = entity.try_get<TransformComponent>();
	return transform ? transform->position : glm::vec3(0.0f);
}

glm::vec3 CharacterModule::GetVelocity(flecs::entity entity) const
{
	auto* character = entity.try_get<CharacterComponent>();
	return character ? character->velocity : glm::vec3(0.0f);
}

void CharacterModule::SetPosition(flecs::entity entity, const glm::vec3& position)
{
	JPH::Character* charPtr = GetCharacter(entity.id());
	if (charPtr)
	{
		charPtr->SetPosition(PhysicsModule::ToJolt(position));

		auto* transform = entity.try_get_mut<TransformComponent>();
		if (transform)
		{
			transform->position = position;
		}

		m_physicsModule->UpdateJoltBody(entity);
	}
}

JPH::BodyID CharacterModule::GetBodyID(flecs::entity entity) const
{
	JPH::Character* character = const_cast<CharacterModule*>(this)->GetCharacter(entity.id());
	return character ? character->GetBodyID() : JPH::BodyID();
}

void CharacterModule::DrawPlayerStatsComponent(flecs::entity entity, PlayerStatComponent& component)
{
	ImGui::SliderInt("Lives", &component.numLives, 1, 99);
}

void CharacterModule::DrawInputComponent(flecs::entity entity, InputComponent& component)
{
	ImGui::BeginTable("##InputComponent", 2, ImGuiTableFlags_Resizable);

	ImGui::TableNextColumn();
	ImGui::Text("Enabled");

	ImGui::TableNextColumn();
	ImGui::Checkbox("##input_enabled", &component.inputEnabled);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("Gamepad Socket");

	ImGui::TableNextColumn();
	std::string socketIndex = std::to_string(component.socketNumber);
	ImGui::Text(socketIndex.c_str());

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("Raw Input");

	ImGui::TableNextColumn();
	ImGui::Checkbox("##Raw Input", &component.useRawInput);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("Rotation Speed");
	ImGui::TableNextColumn();
	ImGui::DragFloat("##RotationSpeed", &component.rotationSpeed, 0.1f, 0.0f, 100.0f);


	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("Iterate Rope");
	ImGui::TableNextColumn();
	if (ImGui::Checkbox("##iterate_rope", &component.iterateRope))
	{
		component.segmentHeld = nullptr;
		component.currentRopeSegment = 0;
		component.ropeIterateAccum = 0.f;
	}

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("Disconnect Hold Time");
	ImGui::TableNextColumn();
	ImGui::DragFloat("##hold_to_disconnect_time", &component.disconnectHoldTime , 0.01f, 0.0f, 10.f);
	ImGui::EndTable();
}

void CharacterModule::DrawAnchorPointInspector(flecs::entity entity, AnchorPoint& component)
{
	ImGui::SliderInt("Max Attachments", &component.maxAttachments, 1, 10);
}


void CharacterModule::DrawCharacterInspector(flecs::entity entity, CharacterComponent& component)
{
	bool changed = false;
	JPH::Character* charPtr = GetCharacter(entity.id());

	if (ImGui::BeginTable("##character_props", 2, ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// --- GENERAL STATUS ---
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Separator();
		ImGui::Text("CHARACTER STATUS");
		ImGui::TableNextColumn();
		ImGui::Separator();

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Character ID");
		ImGui::TableNextColumn();
		ImGui::Text("%i", component.characterID);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Active");
		ImGui::TableNextColumn();
		bool hasCharacter = (charPtr != nullptr);
		ImGui::Text(hasCharacter ? "Yes" : "No");

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Grounded");
		ImGui::TableNextColumn();
		ImGui::Text(component.isGrounded ? "Yes" : "No");

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Velocity");
		ImGui::TableNextColumn();
		ImGui::Text("%.2f, %.2f, %.2f", component.velocity.x, component.velocity.y, component.velocity.z);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Speed");
		ImGui::TableNextColumn();
		ImGui::Text("%.2f", glm::length(component.velocity));


		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Separator();
		ImGui::Text("SLOPE DEBUG");
		ImGui::TableNextColumn();
		ImGui::Separator();

		if (charPtr)
		{
			glm::vec3 joltGroundNormal = GetGroundNormal(entity);
			float joltNormalLength = glm::length(joltGroundNormal);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Jolt Ground Normal");
			ImGui::TableNextColumn();
			if (joltNormalLength > 0.01f) {
				ImGui::Text("%.3f, %.3f, %.3f", joltGroundNormal.x, joltGroundNormal.y, joltGroundNormal.z);
			}
			else {
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Invalid/Zero");
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Smoothed Normal");
			ImGui::TableNextColumn();
			ImGui::Text("%.3f, %.3f, %.3f",
				component.smoothedSurfaceNormal.x,
				component.smoothedSurfaceNormal.y,
				component.smoothedSurfaceNormal.z);

			glm::vec3 upVector = glm::vec3(0, 1, 0);
			float joltSlopeDot = glm::dot(joltGroundNormal, upVector);
			float joltSlopeAngle = glm::degrees(glm::acos(glm::clamp(joltSlopeDot, -1.0f, 1.0f)));

			float smoothedSlopeDot = glm::dot(component.smoothedSurfaceNormal, upVector);
			float smoothedSlopeAngle = glm::degrees(glm::acos(glm::clamp(smoothedSlopeDot, -1.0f, 1.0f)));

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Jolt Slope Angle");
			ImGui::TableNextColumn();
			ImGui::Text("%.1f�", joltSlopeAngle);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Smoothed Slope Angle");
			ImGui::TableNextColumn();
			ImVec4 angleColour = smoothedSlopeAngle > component.maxSlopeAngle ?
				ImVec4(1.0f, 0.3f, 0.3f, 1.0f) :
				(smoothedSlopeAngle > component.maxSlopeAngle * 0.9f ?
					ImVec4(1.0f, 1.0f, 0.3f, 1.0f) :
					ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
			ImGui::TextColored(angleColour, "%.1f�", smoothedSlopeAngle);
		

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Max Slope Angle");
			ImGui::TableNextColumn();
			ImGui::Text("%.1f�", component.maxSlopeAngle);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Slope Status");
			ImGui::TableNextColumn();
			if (smoothedSlopeAngle > component.maxSlopeAngle) 
			{
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "TOO STEEP - SLIDING");
			}
			else if (smoothedSlopeAngle > component.maxSlopeAngle * 0.9f)
			{
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "APPROACHING LIMIT");
			}
			else {
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "WALKABLE");
			}
		}

		// --- GROUND INFO ---
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Separator();
		ImGui::Text("GROUND INFO");
		ImGui::TableNextColumn();
		ImGui::Separator();

		if (charPtr)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Ground State");
			ImGui::TableNextColumn();
			JPH::Character::EGroundState groundState = GetGroundState(entity);
			const char* stateStr = "In Air";
			if (groundState == JPH::Character::EGroundState::OnGround) stateStr = "On Ground";
			if (groundState == JPH::Character::EGroundState::OnSteepGround) stateStr = "On Steep Ground";
			if (groundState == JPH::Character::EGroundState::NotSupported) stateStr = "Not Supported";
			ImGui::Text("%s", stateStr);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Is Supported");
			ImGui::TableNextColumn();
			ImGui::Text(IsSupported(entity) ? "True" : "False");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Ground Body ID");
			ImGui::TableNextColumn();
			ImGui::Text("%llu", GetGroundBodyID(entity).GetIndexAndSequenceNumber());

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Ground User Data");
			ImGui::TableNextColumn();
			ImGui::Text("%llu", GetGroundUserData(entity));

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Ground Position");
			ImGui::TableNextColumn();
			glm::vec3 groundPos = GetGroundPosition(entity);
			ImGui::Text("%.2f, %.2f, %.2f", groundPos.x, groundPos.y, groundPos.z);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Ground Normal");
			ImGui::TableNextColumn();
			glm::vec3 groundNormal = GetGroundNormal(entity);
			ImGui::Text("%.2f, %.2f, %.2f", groundNormal.x, groundNormal.y, groundNormal.z);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Ground Velocity");
			ImGui::TableNextColumn();
			glm::vec3 groundVelocity = GetGroundVelocity(entity);
			ImGui::Text("%.2f, %.2f, %.2f", groundVelocity.x, groundVelocity.y, groundVelocity.z);
		}
		else
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Jolt Info");
			ImGui::TableNextColumn();
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not Available");
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Character ID");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputInt("##CharacterID", &component.characterID, 1, 1, 8);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		ImGui::Separator();

		// --- MOVEMENT PROPERTIES ---
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Separator();
		ImGui::Text("MOVEMENT");
		ImGui::TableNextColumn();
		ImGui::Separator();

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Max Speed");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##MaxSpeed", &component.maxSpeed, 0.1f, 0.1f, 500.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Max Linear Velocity");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##MaxLinearVelocity", &component.maxLinearVelocity, 1.0f, 1.0f, 500.0f);


		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Acceleration");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##Acceleration", &component.acceleration, 0.1f, 0.1f, 500.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Friction");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##Friction", &component.friction, 0.1f, 0.1f, 500.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Restitution");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##Restitution", &component.restitution, 0.1f, 0.1f, 1.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Jump Power");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##JumpHeight", &component.jumpPower, 1.0f, 1.0f, 100.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Air Control");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##AirControl", &component.airControl, 0.01f, 0.0f, 1.0f);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Gravity Factor");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##GravityFactor", &component.gravityFactor, 0.1f, 0.0f, 10.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Mass");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##Mass", &component.mass, 1.0f, 1.0f, 1000.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Max Slope Angle");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##MaxSlopeAngle", &component.maxSlopeAngle, 1.0f, 0.0f, 89.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// --- CAPSULE PROPERTIES ---
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Separator();
		ImGui::Text("CAPSULE");
		ImGui::TableNextColumn();
		ImGui::Separator();

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Capsule Height");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##CapsuleHeight", &component.halfHeight, 0.1f, 0.1f, 5.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Capsule Radius");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##CapsuleRadius", &component.radius, 0.1f, 0.1f, 2.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Motor Strength");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##MotorStrength", &component.motorStrength, 100.0f, 100.0f, 50000.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Motor Damping");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##MotorDamping", &component.motorDamping, 1.0f, 1.0f, 1000.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Vertical Strength");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##VerticalMotorStrength", &component.verticalMotorStrength, 100.0f, 100.0f, 50000.0f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Enable Motors");
		ImGui::TableNextColumn();
		ImGui::Checkbox("##EnableMotors", &component.enableMotors);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Enable Suspension");
		ImGui::TableNextColumn();
		ImGui::Checkbox("##Suspension", &component.enableSuspension);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Spring Rate");
		ImGui::TableNextColumn();
		ImGui::DragFloat("##SpringRate", &component.springRate, 10.0f, 10.0f, 1000.0f);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Damper Rate");
		ImGui::TableNextColumn();
		ImGui::DragFloat("##DamperRate", &component.damperRate, 1.0f, 1.0f, 100.0f);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Max Suspension Travel");
		ImGui::TableNextColumn();
		ImGui::DragFloat("##MaxSuspensionTravel", &component.maxSuspensionTravel, 0.01f, 0.01f, 10.0f);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Compression");
		ImGui::TableNextColumn();
		ImGui::Text("%.1f%%", component.suspensionCompression * 100.0f);

		ImGui::EndTable();
	}

	if (changed)
	{
		glm::vec3 currentPos = GetPosition(entity);
		DestroyCharacterInternal(entity.id());

		auto characterData = CreateCharacterInternal(component, currentPos, entity.id());
		if (characterData.character)
		{
			m_characters[entity.id()] = characterData;
		}
	}
}

ImVec4 CharacterModule::GetCharacterEntityColour(flecs::entity entity)
{
	if (entity.has<CharacterComponent>())
	{
		bool hasCharacter = GetCharacter(entity.id()) != nullptr;
		return hasCharacter ? ImVec4(0.65f, 0.85f, 0.95f, 1.0f) : ImVec4(0.5f, 0.5f, 0.7f, 1.0f);
	}
	return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}

//void CharacterModule::OnImGuiRender()
//{
//	if (m_showCharacterWindow)
//	{
//		ImGui::Begin("Character Module", &m_showCharacterWindow);
//		ImGui::End();
//	}
//}

//void CharacterModule::ResolveUnboundEntities()
//{
//	m_resolveUnboundEntitiesSystem.run();
//}

void CharacterModule::SetLinearVelocity(flecs::entity entity, const glm::vec3& velocity)
{
	JPH::Character* character = GetCharacter(entity.id());
	if (character)
	{
		character->SetLinearVelocity(PhysicsModule::ToJolt(velocity));
	}
}

glm::vec3 CharacterModule::GetLinearVelocity(flecs::entity entity) const
{
	JPH::Character* character = const_cast<CharacterModule*>(this)->GetCharacter(entity.id());
	if (character)
	{
		return PhysicsModule::ToGLM(character->GetLinearVelocity());
	}
	return glm::vec3(0.0f);
}

void CharacterModule::AddLinearVelocity(flecs::entity entity, const glm::vec3& velocity)
{
	JPH::Character* character = GetCharacter(entity.id());
	if (character)
	{
		character->AddLinearVelocity(PhysicsModule::ToJolt(velocity));
	}
}

void CharacterModule::SetLinearAndAngularVelocity(flecs::entity entity, const glm::vec3& linearVel, const glm::vec3& angularVel)
{
	JPH::Character* character = GetCharacter(entity.id());
	if (character)
	{
		character->SetLinearAndAngularVelocity(PhysicsModule::ToJolt(linearVel), PhysicsModule::ToJolt(angularVel));
	}
}

void CharacterModule::AddImpulse(flecs::entity entity, const glm::vec3& impulse)
{
	JPH::Character* character = GetCharacter(entity.id());
	if (character)
	{
		character->AddImpulse(PhysicsModule::ToJolt(impulse));
	}
}

void CharacterModule::GetPositionAndRotation(flecs::entity entity, glm::vec3& outPosition, glm::quat& outRotation) const
{
	JPH::Character* character = const_cast<CharacterModule*>(this)->GetCharacter(entity.id());
	if (character)
	{
		JPH::RVec3 pos;
		JPH::Quat rot;
		character->GetPositionAndRotation(pos, rot);
		outPosition = PhysicsModule::ToGLM(pos);
		outRotation = PhysicsModule::ToGLMQuat(rot);
	}
	else
	{
		auto* transform = entity.try_get<TransformComponent>();
		if (transform)
		{
			outPosition = transform->position;
			outRotation = transform->rotation;
		}
		else
		{
			outPosition = glm::vec3(0.0f);
			outRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}

void CharacterModule::SetPositionAndRotation(flecs::entity entity, const glm::vec3& position, const glm::quat& rotation)
{
	JPH::Character* character = GetCharacter(entity.id());
	if (character)
	{
		character->SetPositionAndRotation(PhysicsModule::ToJolt(position), PhysicsModule::ToJoltQuat(rotation), JPH::EActivation::Activate);

		auto* transform = entity.try_get_mut<TransformComponent>();
		if (transform)
		{
			transform->position = position;
			transform->rotation = rotation;
		}
	}
}

glm::vec3 CharacterModule::GetCenterOfMassPosition(flecs::entity entity) const
{
	JPH::Character* character = const_cast<CharacterModule*>(this)->GetCharacter(entity.id());
	if (character)
	{
		return PhysicsModule::ToGLM(character->GetCenterOfMassPosition());
	}
	return GetPosition(entity);
}

glm::mat4 CharacterModule::GetWorldTransform(flecs::entity entity) const
{
	JPH::Character* character = const_cast<CharacterModule*>(this)->GetCharacter(entity.id());
	if (character)
	{
		JPH::RMat44 joltTransform = character->GetWorldTransform();

		glm::mat4 transform(1.0f);

		for (int col = 0; col < 3; ++col)
		{
			JPH::Vec3 column = joltTransform.GetColumn3(col);
			transform[col][0] = column.GetX();
			transform[col][1] = column.GetY();
			transform[col][2] = column.GetZ();
		}

		JPH::RVec3 translation = joltTransform.GetTranslation();
		transform[3][0] = translation.GetX();
		transform[3][1] = translation.GetY();
		transform[3][2] = translation.GetZ();

		return transform;
	}

	auto* transform = entity.try_get<TransformComponent>();
	if (transform)
	{
		glm::mat4 result(1.0f);
		result = glm::translate(result, transform->position);
		result = result * glm::mat4_cast(transform->rotation);
		result = glm::scale(result, transform->scale);
		return result;
	}

	return glm::mat4(1.0f);
}

void CharacterModule::RespawnPlayerAtPosition(flecs::entity player, TransformComponent& transform, glm::vec3 pos)
{
	transform.position = pos;
	//player.modified<TransformComponent>();

	SetPosition(player, pos);
	
	SetLinearAndAngularVelocity(player, glm::vec3(0), glm::vec3(0));

	//player.modified<CharacterComponent>();

	flecs::entity ropeChild;
	player.children([&ropeChild](flecs::entity child)
		{
			if (child.has<RopeComponent>())
			{
				ropeChild = child;
			}
		});
	auto ropeModule = player.world().try_get_mut<RopeModule>();

	if (ropeModule && ropeChild.is_alive())
	{
		std::cout << "Reconnected rope\n";
		auto comp = ropeChild.try_get_mut<RopeComponent>();
		comp->entityB = flecs::entity::null();
		comp->entityBName = "";
		comp->entityBOffset = glm::vec3(0);
		ropeModule->ConnectRope(ropeChild, player, true);
	}
}

void CharacterModule::Activate(flecs::entity entity)
{
	JPH::Character* character = GetCharacter(entity.id());
	if (character)
	{
		character->Activate();
	}
}

JPH::TransformedShape CharacterModule::GetTransformedShape(flecs::entity entity) const
{
	JPH::Character* character = const_cast<CharacterModule*>(this)->GetCharacter(entity.id());
	if (character)
	{
		return character->GetTransformedShape();
	}

	return JPH::TransformedShape();
}

JPH::ObjectLayer CharacterModule::GetLayer(flecs::entity entity) const
{
	JPH::Character* character = const_cast<CharacterModule*>(this)->GetCharacter(entity.id());
	if (character)
	{
		return character->GetLayer();
	}
	return 0;
}

void CharacterModule::SetLayer(flecs::entity entity, JPH::ObjectLayer layer)
{
	JPH::Character* character = GetCharacter(entity.id());
	if (character)
	{
		character->SetLayer(layer);
	}
}

JPH::Character::EGroundState CharacterModule::GetGroundState(flecs::entity entity) const
{
	auto it = m_characters.find(entity.id());
	if (it != m_characters.end())
	{
		return it->second.character->GetGroundState();
	}
	return JPH::Character::EGroundState::InAir;
}

bool CharacterModule::IsSupported(flecs::entity entity) const
{
	auto it = m_characters.find(entity.id());
	if (it != m_characters.end())
	{
		return it->second.character->IsSupported();
	}
	return false;
}

glm::vec3 CharacterModule::GetGroundPosition(flecs::entity entity) const
{
	auto it = m_characters.find(entity.id());
	if (it != m_characters.end())
	{
		return PhysicsModule::ToGLM(it->second.character->GetGroundPosition());
	}
	return glm::vec3(0.0f);
}

glm::vec3 CharacterModule::GetGroundNormal(flecs::entity entity) const
{
	auto it = m_characters.find(entity.id());
	if (it != m_characters.end())
	{
		return PhysicsModule::ToGLM(it->second.character->GetGroundNormal());
	}
	return glm::vec3(0.0f);
}

glm::vec3 CharacterModule::GetGroundVelocity(flecs::entity entity) const
{
	auto it = m_characters.find(entity.id());
	if (it != m_characters.end())
	{
		return PhysicsModule::ToGLM(it->second.character->GetGroundVelocity());
	}
	return glm::vec3(0.0f);
}

const JPH::PhysicsMaterial* CharacterModule::GetGroundMaterial(flecs::entity entity) const
{
	auto it = m_characters.find(entity.id());
	if (it != m_characters.end())
	{
		return it->second.character->GetGroundMaterial();
	}
	return nullptr;
}

JPH::BodyID CharacterModule::GetGroundBodyID(flecs::entity entity) const
{
	auto it = m_characters.find(entity.id());
	if (it != m_characters.end())
	{
		return it->second.character->GetGroundBodyID();
	}
	return JPH::BodyID();
}

uint64_t CharacterModule::GetGroundUserData(flecs::entity entity) const
{
	auto it = m_characters.find(entity.id());
	if (it != m_characters.end())
	{
		return it->second.character->GetGroundUserData();
	}
	return 0;
}

void CharacterModule::SetGravityFactor(flecs::entity entity, float factor)
{
	JPH::Character* charPtr = GetCharacter(entity.id());
	if (charPtr)
	{
		m_physicsModule->GetBodyInterface().SetGravityFactor(charPtr->GetBodyID(), factor);
	}
}

flecs::entity_t CharacterModule::BodyIDtoEntityID(JPH::BodyID bodyID)
{
	for (auto [entity, info] : m_characters)
	{
		if (info.character->GetBodyID() == bodyID)
		{
			return entity;
		}
	}
	return 0;
}
