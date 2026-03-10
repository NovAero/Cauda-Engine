#include "cepch.h"
#include "CharacterController.h"
#include "Core/Character/CharacterModule.h"
#include "Core/Components/GrabComponent.h"
#include "Core/Editor/EditorCommands.h"
#include "Core/Physics/RopeModule.h"
#include "Core/Physics/SensorModule.h"
#include "Core/Physics/ConstraintModule.h"
#include "Core/Audio/AudioModule.h"
#include "Core/Utilities/Timer.h"
#include "Content/Grimoires/AimAssist.h"
#include "Content/Grimoires/Dash.h"
#include "Core/Physics/PhysicsCasting.h"
#include "Content/Grimoires/ProjectileLauncher.h"

#include <mutex>

std::mutex g_ControllerMutex;

CharacterController::CharacterController(flecs::world& world, SDL_JoystickID controllerID) :
	m_world(world), m_possessedPawn(flecs::entity::null()) 
{
	m_characterModule = world.try_get_mut<CharacterModule>();
	m_physicsModule = world.try_get_mut<PhysicsModule>();
	m_sensorModule = world.try_get_mut<SensorModule>();
	m_ropeModule = world.try_get_mut<RopeModule>();
	m_constraintModule = world.try_get_mut<ConstraintModule>();

	InputSystem::Inst()->AddGamepadListener(this, controllerID);
	InputSystem::Inst()->AddKeyListener(this);
}

CharacterController::~CharacterController()
{
	InputSystem::Inst()->RemoveGamepadListener(this);
	InputSystem::Inst()->RemoveKeyListener(this);

	CleanupConstraints();
}

void CharacterController::Update(float deltaTime)
{
	if (IsPossessing())
	{
		auto* input = m_possessedPawn.try_get_mut<InputComponent>();
		if (input)
		{
			if (!input->isLoadingHarpoon && !input->isHarpoonLoaded)
			{
				input->iterateRope = true;
				input->currentRopeSegment = 0;
				input->isLoadingHarpoon = true;
			}

			if (input->respawnGracePeriod > 0.f)
			{
				input->respawnGracePeriod -= deltaTime;
				input->hasIFrames = true;

				flecs::entity lightChild;
				PointLightComponent* light = nullptr;

				m_possessedPawn.children([&](flecs::entity child)
					{
						if (child.has<PointLightComponent>())
						{
							lightChild = child;
						}
					});


				if (lightChild)
				{
					light = lightChild.try_get_mut<PointLightComponent>();
				}

				if (light)
				{
					//light->intensity += glm::sin(50.f * deltaTime);
					light->intensity = 0;// glm::clamp<float>(light->intensity, 0, 50);
				}

				if (input->respawnGracePeriod <= 0.f)
				{
					if (light)
					{
						light->intensity = 10.f;
					}
					input->respawnGracePeriod = 0.f;
					input->hasIFrames = false;
				}
			}


			if (input->shouldReleaseItem && input->isHoldingItem)
			{
				Release();
			}

			if (input->iterateRope && input->inputEnabled)
			{
				if (!m_ropeChild.is_alive())
				{
					m_possessedPawn.children([this](flecs::entity child)
						{
							if (child.has<RopeComponent>())
							{
								m_ropeChild = child;
							}
						});
				}

				if (m_ropeChild.is_alive())
				{
					auto* ropeComp = m_ropeChild.try_get_mut<RopeComponent>();
					bool hasConnection = ropeComp && ropeComp->entityB.is_alive();

					if (hasConnection)
					{
						input->iterateRope = false;
						input->holdingRopeEnd = true;
					}
					else
					{
						input->ropeIterateAccum += deltaTime;
						if (input->ropeIterateAccum >= input->slipTime)
						{
							input->ropeIterateAccum = 0.f;

							if (!m_ropeModule)
							{
								input->iterateRope = false;
								input->isHarpoonLoaded = true;
								input->currentRopeSegment = 0;
								input->ropeIterateAccum = 0.f;

								if (auto* launcher = m_harpoonLauncher.try_get_mut<ProjectileLauncher>())
								{
									launcher->TryAddAmmo(m_harpoonLauncher, ProjectileType::Harpoon, 10.f);
									auto audioModule = m_world.try_get_mut<AudioModule>();
									audioModule->TryPlaySound2D("SW_HarpoonRetracting", AudioChannel::EFFECTS);
									
								}
							}
							else
							{
								auto ropeSegs = m_ropeModule->GetSegmentsForRope(m_ropeChild);

								if (ropeSegs.size() > 0)
								{
									if (input->currentRopeSegment >= ropeSegs.size() - 1)
									{
										input->holdingRopeEnd = true;
										input->iterateRope = false;
										input->isHarpoonLoaded = true;
										input->currentRopeSegment = 0;
										input->ropeIterateAccum = 0.f;

										if (auto* launcher = m_harpoonLauncher.try_get_mut<ProjectileLauncher>())
										{
											auto audioModule = m_world.try_get_mut<AudioModule>();
											audioModule->TryPlaySound2D("SW_HarpoonRetracting", AudioChannel::EFFECTS);
											launcher->TryAddAmmo(m_harpoonLauncher, ProjectileType::Harpoon, 10.f);
										}
									}
									else
									{
										Release();
										Grab(ropeSegs[input->currentRopeSegment + 1]);
									}
								}
								else
								{
									input->iterateRope = false;
									input->isHarpoonLoaded = true;
									input->currentRopeSegment = 0;
									input->ropeIterateAccum = 0.f;
								}
							}
						}
					}
				}
				else
				{
					std::cout << "m_ropeChild is invalid!\n";
					input->iterateRope = false;
					input->isHarpoonLoaded = true;
					input->currentRopeSegment = 0;
					input->ropeIterateAccum = 0.f;
				}
			}
			else if (!input->inputEnabled)
			{
				Release();
			}

		    if (input->isPullingRope && m_ropeChild.is_alive() && input->inputEnabled)
			{
				auto* ropeComp = m_ropeChild.try_get_mut<RopeComponent>();
				if (ropeComp && ropeComp->entityB.is_alive())
				{
					auto* charTransform = m_possessedPawn.try_get_mut<TransformComponent>();
					if (charTransform)
					{
						glm::vec3 targetPos = Math::GetWorldPosition(ropeComp->entityB);

						if (ropeComp->entityB.has<TransformComponent>())
						{
							auto targetRot = Math::GetWorldRotation(ropeComp->entityB);
							targetPos += targetRot * ropeComp->entityBOffset;
						}


						glm::vec3 charPos = charTransform->position;
						
						float elevationDiff = charPos.y - targetPos.y;
						glm::vec3 toTarget = targetPos - charPos;
						
						toTarget.y += abs(elevationDiff);
						
						float distance = glm::length(toTarget);

						if (distance > 2.0f)
						{
							glm::vec3 pullDirection = glm::normalize(toTarget);
							float pullStrength = 5000.0f;
							float distanceFactor = glm::clamp(distance / 10.0f, 0.2f, 1.0f);
							glm::vec3 pullForce = pullDirection * pullStrength * distanceFactor;

							if (m_characterModule)
							{
								m_characterModule->AddImpulse(m_possessedPawn, pullForce * deltaTime);
							}
						}
					}
				}
			}

			if (input->isHoldingDisconnect)
			{
				input->disconnectHoldAccum += deltaTime;

				if (input->disconnectHoldAccum >= input->disconnectHoldTime)
				{
					if (m_ropeChild.is_alive())
					{
						auto* ropeComp = m_ropeChild.try_get_mut<RopeComponent>();
						if (ropeComp && ropeComp->entityB.is_alive())
						{

							if (m_ropeModule)
							{
								m_ropeModule->DisconnectRope(m_ropeChild, false);
								auto audioModule = m_world.try_get_mut<AudioModule>();
								audioModule->TryPlaySound2D("SW_GrabTail", AudioChannel::EFFECTS);
							}

							Release();

							input->iterateRope = true;
							input->currentRopeSegment = 0;
							input->ropeIterateAccum = 0.0f;
							input->holdingRopeEnd = false;

						}
					}

					input->isHoldingDisconnect = false;
					input->disconnectHoldAccum = 0.0f;
				}
			}

			auto* aimAssist = m_possessedPawn.try_get_mut<AimAssist>();
			if (aimAssist && aimAssist->enabled)
			{
				glm::quat pawnWorldRotation = Math::GetWorldRotation(m_possessedPawn);
				glm::vec3 rawForwardDirection = pawnWorldRotation * glm::vec3(0, 0, 1);

				if (m_projectileLauncher.is_alive())
				{
					glm::vec3 assistedDir = aimAssist->ApplyAimAssist(
						m_possessedPawn, rawForwardDirection, 1.0f, false
					);

					if (glm::length(assistedDir) > 0.01f)
					{
						glm::quat assistedRot = glm::quatLookAt(
							glm::normalize(assistedDir), glm::vec3(0, 1, 0)
						);
						Math::SetWorldRotation(m_projectileLauncher, assistedRot);
					}
				}

				if (m_harpoonLauncher.is_alive())
				{
					glm::vec3 assistedDir = aimAssist->ApplyAimAssist(
						m_possessedPawn, rawForwardDirection, 1.0f, true
					);

					if (glm::length(assistedDir) > 0.01f)
					{
						glm::quat assistedRot = glm::quatLookAt(
							glm::normalize(assistedDir), glm::vec3(0, 1, 0)
						);
						Math::SetWorldRotation(m_harpoonLauncher, assistedRot);
					}
				}
			}

			if (input->firePressed)
			{
				input->respawnGracePeriod = 0.f;

				m_possessedPawn.children([&](flecs::entity child)
					{
						if (child.has<PointLightComponent>())
						{
							auto light = child.try_get_mut<PointLightComponent>();
							light->intensity = 10.f;
						}
					});


				if (m_projectileLauncher.is_alive())
				{
					if (auto* launcher = m_projectileLauncher.try_get_mut<ProjectileLauncher>())
					{
						if (launcher->TryFire(m_projectileLauncher))
						{
							input->respawnGracePeriod = 0.f;

							m_possessedPawn.children([&](flecs::entity child)
								{
									if (child.has<PointLightComponent>())
									{
										auto light = child.try_get_mut<PointLightComponent>();
										light->intensity = 10.f;
									}
								});
						}
					}
				}
			}
		}
	}
}

bool CharacterController::IsPossessing() const
{
	return m_possessedPawn && m_possessedPawn.is_alive();
}

void CharacterController::PossessFirstAvailable()
{
	if (IsPossessing()) return;

	if (Cauda::Application::GetGameLayer()->IsInMainMenu() || 
		Cauda::Application::GetGameLayer()->IsStartingRound() ||
		!Cauda::Application::GetGameLayer()->IsInLobby()) return;

	flecs::entity bestCandidate = flecs::entity::null();
	int lowestID = INT_MAX;

	auto query = m_world.query_builder<InputComponent, CharacterComponent>().build();

	query.each([&](flecs::entity entity, InputComponent& input, CharacterComponent& character)
		{
			if (input.socketNumber == -1 && character.characterID < lowestID)
			{
				bestCandidate = entity;
				lowestID = character.characterID;
			}
		});

	if (bestCandidate.is_alive())
	{
		m_possessedPawn = bestCandidate;

		Timer::AddScriptTimerComponent(bestCandidate);

		Cauda::Application::GetGameLayer()->OnPlayerPossessed(m_possessedPawn, *this);

		if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
		{
			InputSystem::Inst()->BindFirstAvailableController(this);
			m_gamepadSocket = InputSystem::Inst()->RequestSocketForPossess();
			input->socketNumber = m_gamepadSocket;
			input->inputEnabled = true;

			m_harpoonLauncher = flecs::entity::null();
			m_projectileLauncher = flecs::entity::null();

			auto find_launchers = [this](flecs::entity entity)
			{
				if (auto* launcher = entity.try_get_mut<ProjectileLauncher>())
				{
					if (launcher->isHarpoonGun)
					{
						if (!m_harpoonLauncher.is_alive()) m_harpoonLauncher = entity;
					}
					else
					{
						if (!m_projectileLauncher.is_alive()) m_projectileLauncher = entity;
					}
				}
			};

			find_launchers(m_possessedPawn); 

			m_possessedPawn.children([&](flecs::entity child)
			{
				if (child.has<ColliderComponent>())
				{
					m_colliderChild = child;
				}
				find_launchers(child); 
			});
		}
		else
		{
			DetachFromPawn();
		}
	}
}

void CharacterController::PossessWithID(int characterID)
{
	if (IsPossessing()) return;

	if (Cauda::Application::GetGameLayer()->IsInMainMenu()) return;

	auto query = m_world.query_builder<InputComponent, CharacterComponent>().build();

	query.each([this, characterID](flecs::entity entity, InputComponent& input, CharacterComponent& character)
		{
			if (character.characterID == characterID && input.socketNumber == -1)
			{
				m_possessedPawn = entity;

				Cauda::Application::GetGameLayer()->OnPlayerPossessed(m_possessedPawn, *this);

				Timer::AddScriptTimerComponent(entity);

				if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
				{
					m_gamepadSocket = InputSystem::Inst()->RequestSocketForPossess();
					input->socketNumber = m_gamepadSocket;
					input->inputEnabled = true;

					m_harpoonLauncher = flecs::entity::null();
					m_projectileLauncher = flecs::entity::null();

					auto find_launchers = [this](flecs::entity entity)
					{
						if (auto* launcher = entity.try_get_mut<ProjectileLauncher>())
						{
							if (launcher->isHarpoonGun)
							{

								if (!m_harpoonLauncher.is_alive()) m_harpoonLauncher = entity;
							}
							else 
							{
								if (!m_projectileLauncher.is_alive()) m_projectileLauncher = entity;
							}
						}
					};

					find_launchers(m_possessedPawn); 

					m_possessedPawn.children([&](flecs::entity child)
					{
						if (child.has<ColliderComponent>())
						{
							m_colliderChild = child;
						}
						find_launchers(child); 
					});
				}
				else
				{
					DetachFromPawn();
				}
			}
		});
}

void CharacterController::DetachFromPawn()
{
	if (m_gamepadSocket > -1)
	{
		InputSystem::Inst()->ReleaseSocket(m_gamepadSocket);
		m_gamepadSocket = -1;
	}

	if (IsPossessing())
	{
		if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
		{
			input->socketNumber = -1;
			input->movementInput = glm::vec2(0);
			input->inputEnabled = false;
			input->ResetValues();
		}

		Cauda::Application::GetGameLayer()->OnPlayerDetach(m_possessedPawn, *this);
	}

	CleanupConstraints();

	m_possessedPawn = flecs::entity::null();
	m_ropeChild = flecs::entity::null();
	m_harpoonLauncher = flecs::entity::null();
	m_projectileLauncher = flecs::entity::null();
	m_currentProjectileType = ProjectileType::Kinetic;
}

void CharacterController::DisableInput()
{
	if (!IsPossessing()) return;
	auto input = m_possessedPawn.try_get_mut<InputComponent>();
	input->inputEnabled = false;
	input->movementInput = glm::vec2(0);
	input->rotationInput = glm::vec2(0);
}

void CharacterController::EnableInput()
{
	if (!IsPossessing()) return;
	auto input = m_possessedPawn.try_get_mut<InputComponent>();
	input->inputEnabled = true;
}

void CharacterController::OnButtonDown(Uint8 button)
{
	if (!IsPossessing())
	{
		PossessFirstAvailable();
		if (!Cauda::Application::GetGameLayer()->IsInMainMenu())
		{
			if (Cauda::Application::Get().IsRumbleEnabled())
			{
				if (Bindings().size() >= 1)
				{
					SDL_Gamepad* gamepad = SDL_GetGamepadFromID(Bindings().front());
					if (gamepad)
					{
						SDL_RumbleGamepad(gamepad, 0x4000, 0x8000, 500);
					}
				}
			}
		}
	}
	else
	{
		if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
		{
			if (!input->inputEnabled) return;

			switch (button)
			{
			case SDL_GAMEPAD_BUTTON_START:
			{
				SDL_PushEvent(&GameEvents::g_ButtonEvent_AppSettings);

			} break;
			case SDL_GAMEPAD_BUTTON_SOUTH:
			{
				if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
				{
					input->jumpPressed = true;
				}
			} break;
			case SDL_GAMEPAD_BUTTON_EAST:
			{
				input->iterateRope = true;
				input->currentRopeSegment = 0;
				input->ropeIterateAccum = 0.f;
				input->holdingRopeEnd = false;
				input->shouldReleaseItem = false;

				if (Cauda::Application::GetGameLayer()->IsInLobby() && 
					!Cauda::Application::GetGameLayer()->IsStartingRound())
				{
					if (m_readied)
					{
						m_readied = false;
						SDL_PushEvent(&GameEvents::g_PlayerUnreadiedEvent);
					}
					else
					{
						m_readied = true;
						SDL_PushEvent(&GameEvents::g_PlayerReadiedEvent);
					}
				}

			} break;
			case SDL_GAMEPAD_BUTTON_BACK:
			{
				if (IsPossessing() && Cauda::Application::GetGameLayer()->IsInLobby() 
					&& !Cauda::Application::GetGameLayer()->IsStartingRound())
				{
					m_readied = false;
					SDL_PushEvent(&GameEvents::g_PlayerUnreadiedEvent);
					if (Cauda::Application::Get().IsRumbleEnabled())
					{
						if (Bindings().size() >= 1)
						{
							SDL_Gamepad* gamepad = SDL_GetGamepadFromID(Bindings().front());
							if (gamepad)
							{
								SDL_RumbleGamepad(gamepad, 0x4000, 0x4000, 500);
							}
						}
					}
					DetachFromPawn();
				}
			} break;
			case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
			{
				if (!input->inputEnabled) return;
				if (input->isHarpoonLoaded)
				{
					input->isHarpoonLoaded = false;
					Release();

					if (m_harpoonLauncher.is_alive())
					{
						if (auto* launcher = m_harpoonLauncher.try_get_mut<ProjectileLauncher>())
						{
							launcher->TryFire(m_harpoonLauncher);

							Timer::SetDelay(m_possessedPawn, 1.0f, [this, pawn = m_possessedPawn]()
								{
									if (!pawn.is_alive()) return;

									auto* input = pawn.try_get_mut<InputComponent>();
									if (!input) return;

									flecs::entity ropeChild;
									pawn.children([&ropeChild](flecs::entity child)
										{
											if (child.has<RopeComponent>())
											{
												ropeChild = child;
											}
										});

									bool hasConnection = false;
									if (ropeChild.is_alive())
									{
										auto* ropeComp = ropeChild.try_get_mut<RopeComponent>();
										hasConnection = ropeComp && ropeComp->entityB.is_alive();
									}

									if (!hasConnection)
									{
										input->iterateRope = true;
										input->currentRopeSegment = 0;
										input->ropeIterateAccum = 0.0f;
										input->isHoldingItem = false;
										input->holdingRopeEnd = false;
										input->isHarpoonLoaded = false;
									}
								});
						}
					}
					else if (m_ropeChild.try_get_mut<RopeComponent>()->entityB != flecs::entity::null())
					{
						input->isHoldingDisconnect = true;
					}
				}
				else
				{
					if (m_ropeChild.is_alive())
					{
						auto* ropeComp = m_ropeChild.try_get_mut<RopeComponent>();
						if (ropeComp && ropeComp->entityB.is_alive())
						{
							input->isHoldingDisconnect = true;
							input->disconnectHoldAccum = 0.0f;
						}
					}
				}
			} break;
			case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
			{
				input->isPullingRope = true;

			} break;

			}
		}
	}
}

void CharacterController::OnButtonHeld(Uint8 button, int heldTicks)
{
	if (!IsPossessing()) return;

	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		if (!input->inputEnabled) return;

		switch (button)
		{
		case SDL_GAMEPAD_BUTTON_WEST:
		{
			
		} break;
		}
	}
}
void CharacterController::OnButtonUp(Uint8 button)
{
	if (!IsPossessing()) return;

	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		if (!input->inputEnabled) return;

		switch (button)
		{
		case SDL_GAMEPAD_BUTTON_SOUTH:
		{
			input->jumpPressed = false;
		} break;
		case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		{
			input->isHoldingDisconnect = false;
		} break;
		case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		{
			input->isPullingRope = false;
		} break;
		}
	}
}
void CharacterController::OnLeftAxisInput(float xValue, float yValue)
{
	if (!IsPossessing()) return;

	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		if (!input->inputEnabled) return;
		input->movementInput = glm::vec2{ xValue, yValue };
	}
}

void CharacterController::OnRightAxisInput(float xValue, float yValue)
{
	if (!IsPossessing()) return;
	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		if (!input->inputEnabled) return;
		input->rotationInput = glm::vec2{ xValue, yValue };
	}
}

void CharacterController::OnTriggerDown(bool left)
{
	if (!IsPossessing()) return;
	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		if (!input->inputEnabled) return;

		if (!left)
		{
			if (m_projectileLauncher.is_alive())
				{
					if (auto* launcher = m_projectileLauncher.try_get_mut<ProjectileLauncher>())
					{
						input->firePressed = true;
						launcher->projectileType = static_cast<ProjectileType>(m_currentProjectileType);
					}
				}
			else
			{
				std::cout << "CharacterController Error: Projectile launcher not found!" << std::endl;
			}
		}
		else  
		{
			input->dashPressed = true;
		}
	}
}

void CharacterController::OnTriggerUp(bool left)
{
	if (!IsPossessing()) return;

	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		if (!left) 
		{
			input->firePressed = false;
		}
		else  
		{
			input->dashPressed = false;
		}
	}
}

void CharacterController::OnKeyPress(PKey key)
{
	if (!IsPossessing()) return;

	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		if (!input->inputEnabled) return;
		
		switch (key)
		{
		case PKey::W:
		{
			input->movementInput.y = -1.0f;
		} break;
		case PKey::S:
		{
			input->movementInput.y = 1.0f;
		} break;
		case PKey::A:
		{
			input->movementInput.x = -1.0f;
		} break;
		case PKey::D:
		{
			input->movementInput.x = 1.0f;
		} break;
		case PKey::F:
		{
			input->firePressed = true;
		} break;
		case PKey::Space:
		{
			input->jumpPressed = true;
		} break;
		case PKey::LShift:
		{
			input->dashPressed = true;
			
		} break;
		}
	}
}

void CharacterController::OnKeyRelease(PKey key)
{
	if (!IsPossessing()) return;

	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		if (!input->inputEnabled) return;

		switch (key)
		{
		case PKey::W:
		{
			input->movementInput.y = InputSystem::IsKeyDown(PKey::S) ? 1.0f : 0.0f; 
		} break;
		case PKey::S:
		{
			input->movementInput.y = InputSystem::IsKeyDown(PKey::W) ? -1.0f : 0.0f;
		} break;
		case PKey::A:
		{
			input->movementInput.x = InputSystem::IsKeyDown(PKey::D) ? 1.0f : 0.0f;
		} break;
		case PKey::D:
		{
			input->movementInput.x = InputSystem::IsKeyDown(PKey::A) ? -1.0f : 0.0f;
		} break;
		case PKey::F:
		{
			input->firePressed = false;
		} break;
		case PKey::Space:
		{
			input->jumpPressed = false;
		} break;
		case PKey::LShift:
		{
			input->dashPressed = false;
			
		} break;
		}
	}
}

bool CharacterController::TryAddAmmo(int type)
{
	/*if (m_projectileLauncher)
	{
		if (auto launcher = m_projectileLauncher.try_get_mut<ProjectileLauncher>())
		{
			return launcher->TryAddAmmo(type);
		}
	}
	else
	{
		bool success = false;
		m_possessedPawn.children([this, &success, type](flecs::entity child)
			{
				if (auto* launcher = child.try_get_mut<ProjectileLauncher>())
				{
					success = launcher->TryAddAmmo(type);
				}
			});
		return success;
	}*/
	return false;
}

void CharacterController::CachePlayerIDForReload()
{
	if (!IsPossessing()) return;
	if (auto character = m_possessedPawn.try_get_mut<CharacterComponent>())
	{
		m_cachedPlayerID = character->characterID;
	}
}

void CharacterController::TryReposessCachedPlayerID()
{
	if (IsPossessing()) return;
	if (m_cachedPlayerID != -1)
	{
		PossessWithID(m_cachedPlayerID);

		m_cachedPlayerID = -1;
	}
}

void CharacterController::ResetCachedID()
{
	m_cachedPlayerID = -1;
}

void CharacterController::ResetReadyState()
{
	m_readied = false;
}

//Deprecated, use Grab(flecs::entity target)
void CharacterController::Grab()
{
	JPH::Body* heldBody = nullptr;
	JPH::Body* colliderBody = nullptr;

	auto* input = m_possessedPawn.try_get_mut<InputComponent>();

	//if (!input->canGrab) return;

	if (m_colliderChild)
	{
		const auto& sensorData = m_sensorModule->GetSensorDataForEntity(m_colliderChild);
		for (auto [entity, i] : sensorData.overlappedEntities)
		{
			if (!entity) continue;
			if (entity.has<Grabbable>() && entity.has<PhysicsBodyComponent>())
			{
				if (!entity.is_alive())
				{
					continue;
				}

				auto bodyID = m_physicsModule->GetBodyID(m_colliderChild.id());
				colliderBody = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);

				if (entity.has<RopeSegment>())
				{
					auto* physBody = entity.try_get_mut<PhysicsBodyComponent>();
					bodyID = (JPH::BodyID)physBody->bodyIdandSequence;
					heldBody = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);

					if (input)
					{
						input->segmentHeld = entity.try_get_mut<RopeSegment>();
						input->currentRopeSegment = input->segmentHeld->index;
						ropeEntity = m_world.get_alive(input->segmentHeld->owner);
					}

					m_heldEntity = entity;
				}
				else
				{
					bodyID = (JPH::BodyID)entity.try_get_mut<PhysicsBodyComponent>()->bodyIdandSequence;
					heldBody = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);

					m_heldEntity = entity;
				}
			}
		}
	}

	//std::cout << "Tried adding grab constraint:\n\tOtherBody:" << heldBody << "\tColliderBody:" << colliderBody << '\n';
	if (heldBody && colliderBody)
	{
		auto bodyID = m_characterModule->GetCharacter(m_possessedPawn.id())->GetBodyID();
		auto pawnBody = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);

		Cauda::DistanceConstraintSettings distanceSettings;
		distanceSettings.maxDistance = 1.3f;
		distanceSettings.minDistance = 0.f;

		Cauda::SpringConstraintSettings springSettings;
		springSettings.maxDistance = 0.2f;
		springSettings.minDistance = 0.0f;
		springSettings.stiffness = 0.0f;

		auto grabConstraint = m_constraintModule->CreateDistanceConstraint(pawnBody, heldBody, distanceSettings);
		auto springConstraint = m_constraintModule->CreateSpringConstraint(colliderBody, heldBody, springSettings);

		JPH::Constraint* ghostTether = nullptr;

		//if (heldBodyParent)
		//{
			//auto ropeEntity = m_world.get_alive(input->ropeHeld->owner);
			//RopeComponent* ropeComp = nullptr;
			//if (ropeEntity)
			//{
			//	ropeComp = ropeEntity.try_get_mut<RopeComponent>();

			//	Cauda::DistanceConstraintSettings ghostSettings;
			//	ghostSettings.maxDistance = ((ropeComp->length / ropeComp->numSegments) * input->ropeHeld->index) * 1.25f;
			//	ghostSettings.minDistance = 0.f;

			//	/*JPH::DistanceConstraintSettings set;
			//	set.mConstraintPriority = 1;*/

			//	ghostTether = m_constraintModule->CreateDistanceConstraint(heldBodyParent, pawnBody, ghostSettings);

			//	if (ghostTether)
			//	{
			//		m_physicsModule->GetPhysicsSystem()->AddConstraint(ghostTether);
			//		m_cachedGrabConstraints.push(ghostTether);
			//		std::cout << "Created ghost tether\n";
			//	}
			//}
		//}

		if (grabConstraint)
		{
			m_physicsModule->GetPhysicsSystem()->AddConstraint(grabConstraint);
			m_physicsModule->GetPhysicsSystem()->AddConstraint(springConstraint);

			auto* input = m_possessedPawn.try_get_mut<InputComponent>();
			input->isHoldingItem = true;

			m_cachedGrabConstraints.push(grabConstraint);
			m_cachedGrabConstraints.push(springConstraint);

			/*if (auto audio = m_world.try_get_mut<AudioModule>())
			{
				audio->TryPlaySoundWave3D("SW_GrabTail", PhysicsModule::ToGLM(pawnBody->GetPosition()), AudioChannel::EFFECTS);
			}*/
		}
	}
}

void CharacterController::Grab(flecs::entity target)
{
	if (!target.is_alive()) return;
	if (!target.has<PhysicsBodyComponent>()) return;

	JPH::Body* heldBody = nullptr;
	JPH::Body* colliderBody = nullptr;
	auto* input = m_possessedPawn.try_get_mut<InputComponent>();

	auto bodyID = m_physicsModule->GetBodyID(m_colliderChild.id());
	colliderBody = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);

	auto* physBody = target.try_get_mut<PhysicsBodyComponent>();
	bodyID = (JPH::BodyID)physBody->bodyIdandSequence;
	heldBody = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);

	if (target.has<RopeSegment>())
	{
		input->segmentHeld = target.try_get_mut<RopeSegment>();
		if (input->segmentHeld)
		{
			input->currentRopeSegment = input->segmentHeld->index;
			ropeEntity = m_world.get_alive(input->segmentHeld->owner);
		}
	}

	//std::cout << "Tried adding grab constraint:\n\tOtherBody:" << heldBody << "\tColliderBody:" << colliderBody << '\n';
	if (heldBody && colliderBody)
	{
		auto bodyID = m_characterModule->GetCharacter(m_possessedPawn.id())->GetBodyID();
		auto pawnBody = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);

		Cauda::DistanceConstraintSettings distanceSettings;
		distanceSettings.maxDistance = 1.3f;
		distanceSettings.minDistance = 0.f;

		Cauda::SpringConstraintSettings springSettings;
		springSettings.maxDistance = 0.2f;
		springSettings.minDistance = 0.0f;
		springSettings.stiffness = 0.0f;

		auto grabConstraint = m_constraintModule->CreateDistanceConstraint(pawnBody, heldBody, distanceSettings);
		auto springConstraint = m_constraintModule->CreateSpringConstraint(colliderBody, heldBody, springSettings);
		JPH::Constraint* ghostTether = nullptr;

		if (grabConstraint)
		{
			m_physicsModule->GetPhysicsSystem()->AddConstraint(grabConstraint);
			m_physicsModule->GetPhysicsSystem()->AddConstraint(springConstraint);

			auto* input = m_possessedPawn.try_get_mut<InputComponent>();
			input->isHoldingItem = true;

			m_cachedGrabConstraints.push(grabConstraint);
			m_cachedGrabConstraints.push(springConstraint);

			/*if (auto audio = m_world.try_get_mut<AudioModule>())
			{
				audio->TryPlaySoundWave3D("SW_GrabTail", PhysicsModule::ToGLM(pawnBody->GetPosition()), AudioChannel::EFFECTS);
			}*/
		}
	}
}

void CharacterController::Release()
{
	if (auto* input = m_possessedPawn.try_get_mut<InputComponent>())
	{
		bool wasConnected = false;
		if (m_ropeChild.is_alive())
		{
			auto* ropeComp = m_ropeChild.try_get_mut<RopeComponent>();
			wasConnected = ropeComp && ropeComp->entityB.is_alive();
		}

		//input->grabPressed = false;
		input->isHoldingItem = false;
		input->segmentHeld = nullptr;
		input->holdingRopeEnd = false;
		input->shouldReleaseItem = false;

		if (wasConnected && input->holdingRopeEnd)
		{
			input->isHarpoonLoaded = true;
		}
	}

	CleanupConstraints();
}

void CharacterController::CleanupConstraints()
{
	while (!m_cachedGrabConstraints.empty())
	{
		JPH::Ref<JPH::Constraint> constraint = m_cachedGrabConstraints.front();

		if (constraint != nullptr)
		{
			m_physicsModule->GetPhysicsSystem()->RemoveConstraint(constraint);
		}

		m_cachedGrabConstraints.pop();
	}
}
