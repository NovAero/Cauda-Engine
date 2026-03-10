#include "cepch.h"
#include "CaribbeanModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Character/CharacterModule.h"
#include "Renderer/RendererModule.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/ShaderStorageBuffer.h"
#include "Platform/Win32/Application.h"


namespace ECaribbeanMode
{
	// prefix (++enumType)
	Type& operator++(Type& orig)
	{
		if (orig < ECaribbeanMode::END - 1)
			orig = static_cast<ECaribbeanMode::Type>(orig + 1);
		else
			orig = static_cast<ECaribbeanMode::Type>(0);
		return orig;
	}

	// postfix (enumType++)
	Type operator++(Type& orig, int)
	{
		Type out = orig;
		++orig;
		return out;
	}
}

CaribbeanModule::CaribbeanModule(flecs::world& world)
	: m_world(world),
	m_editorModule(m_world.try_get_mut<EditorModule>()),
	m_physicsModule(m_world.try_get_mut<PhysicsModule>()),
	m_rendererModule(m_world.try_get_mut<RendererModule>())
{
	
	assert(m_emptyVAO == 0);
	glGenVertexArrays(1, &m_emptyVAO);

	SetupRandomGenSSBO();
	SetupParticleBuffer();
	SetupShaders();
	SetupComputeShaders();

	SetupComponents();
	SetupSystems();
	SetupObservers();
	SetupQueries();

	RegisterWithEditor();

	EditorConsole::Inst()->AddConsoleInfo("CaribbeanModule initialised successfully.", true);
}

CaribbeanModule::~CaribbeanModule()
{
	Logger::PrintLog("CaribbeanModule shutdown starting...");

	glDeleteVertexArrays(1, &m_emptyVAO);

	if (m_caribbeanAddObserver && m_caribbeanAddObserver.is_alive())
		m_caribbeanAddObserver.destruct();
	if (m_caribbeanUpdateObserver && m_caribbeanUpdateObserver.is_alive())
		m_caribbeanUpdateObserver.destruct();
	if (m_caribbeanRemoveObserver && m_caribbeanRemoveObserver.is_alive())
		m_caribbeanRemoveObserver.destruct();

	Logger::PrintLog("CaribbeanModule shutdown completed.");
}


void CaribbeanModule::Render()
{
	if (!m_cachedParticleRenderShader || !m_rendererModule) return;

	//glDepthMask(GL_FALSE);
	glEnablei(GL_BLEND, 0);
	glBlendFunci(0, GL_SRC_ALPHA, GL_ONE);
	//glBlendFunci(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_cachedParticleRenderShader->Use();

	m_caribbeanQuery.each([this](flecs::entity entity, CaribbeanComponent& caribbean)
	{
		if (caribbean.visible)
		{
			UpdateUBOData(entity, caribbean);

			m_particleUBOs[entity.id()]->BindBufferBase(CARIBBEAN_PARTICLE_UBO_ID);
			m_particleSSBOs[entity.id()]->BindBufferBase(CARIBBEAN_PARTICLE_SSBO_ID);

			glBindVertexArray(m_emptyVAO);
			glDrawArraysInstanced(GL_TRIANGLES, 0, 6, caribbean.maxParticles);
			glBindVertexArray(0);
		}
	});

	glDisablei(GL_BLEND, 0);
	glDepthMask(GL_TRUE);
}

void CaribbeanModule::TryEnableParticlesFromEntity(flecs::entity entity, bool isEnabled)
{
	if (!entity.is_alive()) return;

	auto caribbean = entity.try_get_mut<CaribbeanComponent>();
	if (!caribbean) return;

	TryEnableParticlesFromComponent(*caribbean, isEnabled);
}

void CaribbeanModule::TryEnableParticlesFromComponent(CaribbeanComponent& component, bool isEnabled)
{
	component.enabled = isEnabled;
}

void CaribbeanModule::TryRenderParticlesFromEntity(flecs::entity entity, bool isVisible)
{
	if (!entity.is_alive()) return;

	auto caribbean = entity.try_get_mut<CaribbeanComponent>();
	if (!caribbean) return;

	TryRenderParticlesFromComponent(*caribbean, isVisible);
}

void CaribbeanModule::TryRenderParticlesFromComponent(CaribbeanComponent& component, bool isVisible)
{
	component.visible = isVisible;
}

void CaribbeanModule::TrySpawnParticlesFromEntity(flecs::entity entity, int spawnCount)
{
	if (!entity.is_alive()) return;

	auto caribbean = entity.try_get<CaribbeanComponent>();
	if (!caribbean) return;

	SpawnParticles(entity, *caribbean, spawnCount);
}


void CaribbeanModule::SetupRandomGenSSBO()
{
	if (m_randomGenSSBO) return; 

	// Generate four random unsigned ints.
	glm::uvec4 randomSeeds = glm::linearRand(glm::uvec4(0), glm::uvec4(0xFFFFFFFE));

	RandGenSSBO tempBuffer { .seeds = randomSeeds };

	m_randomGenSSBO = std::make_unique<ShaderStorageBuffer>();
	m_randomGenSSBO->Bind();
	m_randomGenSSBO->BufferData(sizeof(RandGenSSBO), &tempBuffer);
	m_randomGenSSBO->Unbind();
	m_randomGenSSBO->BindBufferBase(RANDGEN_SSBO_ID);

	EditorConsole::Inst()->AddConsoleInfo("Generated and filled RNG SSBO.", true);
}

void CaribbeanModule::SetupParticleBuffer()
{
	defaultParticle =
	{
		.position = glm::vec4(0.f),
		.velocity = glm::vec3(0.f),
		.radius = 0.f,
		.lifetime = 0.f,
		.timeAlive = FLT_MAX
	};

	for (unsigned int i = 0; i < PARTICLE_WORKGROUP_SIZE; i++)
	{
		m_particleBuffer[i] = defaultParticle;
	}
}

void CaribbeanModule::SetupShaders()
{
	ResourceLibrary::LoadShader("particle_render", "shaders/particle_render.vert", "shaders/particle_render.frag");
	m_cachedParticleRenderShader = ResourceLibrary::GetShader("particle_render");
}

void CaribbeanModule::SetupComputeShaders()
{
	ResourceLibrary::LoadComputeShader("particle_spawn", "shaders/particle_spawn.glsl");
	ResourceLibrary::LoadComputeShader("particle_compute", "shaders/particle_compute.glsl");
	m_cachedParticleComputeShader = ResourceLibrary::GetComputeShader("particle_compute");

	//ResourceLibrary::LoadComputeShader("sph_compute", "shaders/SPH_compute.glsl");
	//ResourceLibrary::LoadComputeShader("sph_hashTable", "shaders/SPH_hashTable.glsl");
	//ResourceLibrary::LoadComputeShader("sph_density", "shaders/SPH_density.glsl");
	//ResourceLibrary::LoadComputeShader("sph_pressure", "shaders/SPH_pressure.glsl");
}

void CaribbeanModule::SetupComponents()
{
	flecs::entity transformComp = m_world.component<TransformComponent>("TransformComponent");

	m_world.component<CaribbeanComponent>("CaribbeanComponent")
		.member<ECaribbeanMode::Type>("computeMode")
		.member<bool>("enabled")
		.member<bool>("visible")
		.member<int>("maxSteps")
		.member<float>("fixedTimeStep")
		.member<glm::vec3>("originOffset")

		.member<bool>("hasBounds")
		.member<glm::vec3>("boxHalfExtent")

		.member<int>("maxParticles")
		.member<int>("spawnCount")
		.member<float>("spawnInterval")
		.member<glm::vec3>("spawnOffset")
		.member<glm::vec3>("particleColour")
		.member<glm::vec3>("accelerationForce")

		.member<float>("minRadius")
		.member<float>("maxRadius")
		.member<float>("minLifetime")
		.member<float>("maxLifetime")
		.member<float>("minInitialSpeed")
		.member<float>("maxInitialSpeed")
		.add(flecs::With, transformComp);
}

void CaribbeanModule::SetupSystems()
{
	if (!m_cachedParticleComputeShader)
	{
		Logger::PrintLog("Warning: CaribbeanModule failed to get 'particle_compute' shader");
		return;
	}

	flecs::entity gameTick = Cauda::Application::GetGameTickSource();
	m_computeParticlesSystem = m_world.system<CaribbeanComponent>("ComputeParticlesUpdate")
		.kind(flecs::PreStore)
		//.tick_source(gameTick)
		.each([this](flecs::iter& it, size_t row, CaribbeanComponent& caribbean)
		{
			if (!caribbean.enabled || caribbean.maxParticles == 0) return;

			float deltaTime = it.delta_system_time();
			flecs::entity entity = it.entity(row);
			//std::cout << entity.name() << std::endl;

			if (!m_particleUBOs.contains(entity.id()) || !m_particleSSBOs.contains(entity.id())) return;

			// Can't use flecs system intervals since every caribbean instance can have different intervals.
			caribbean.accumulatedSpawnTime += deltaTime;
			caribbean.accumulatedSimTime += deltaTime;

			bool needsSpawn = caribbean.accumulatedSpawnTime > caribbean.spawnInterval;
			bool needsSim = caribbean.accumulatedSimTime > caribbean.fixedTimeStep;
			if (!needsSpawn && !needsSim) return;

			m_particleUBOs[entity.id()]->BindBufferBase(CARIBBEAN_PARTICLE_UBO_ID);
			m_particleSSBOs[entity.id()]->BindBufferBase(CARIBBEAN_PARTICLE_SSBO_ID);

			// Particle Spawning
			if (caribbean.accumulatedSpawnTime > caribbean.spawnInterval)
			{
				//might need to cap the spawn count later
				int spawnIterations = (int)glm::floor(caribbean.accumulatedSpawnTime / caribbean.spawnInterval);
				float spawnTimePassed = spawnIterations * caribbean.spawnInterval;
				caribbean.accumulatedSpawnTime -= spawnTimePassed;

				int spawnCount = spawnIterations * caribbean.spawnCount;
				this->SpawnParticles(entity, caribbean, spawnCount);
			}

			// Particle Kinematic Update
			if (caribbean.accumulatedSimTime > caribbean.fixedTimeStep)
			{
				int requiredSimSteps = glm::min(caribbean.maxSteps, (int)glm::floor(caribbean.accumulatedSimTime / caribbean.fixedTimeStep));
				float simTimePassed = requiredSimSteps * caribbean.fixedTimeStep;
				caribbean.accumulatedSimTime -= simTimePassed;

				m_cachedParticleComputeShader->Use();
				unsigned int dispatchGroups = 1 + (caribbean.maxParticles - 1) / PARTICLE_WORKGROUP_SIZE;

				for (unsigned int step = 0; step < requiredSimSteps; step++)
				{
					m_cachedParticleComputeShader->Dispatch(dispatchGroups, 1, 1);

					if (step < requiredSimSteps - 1)
					{
						glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
					}
				}
			}
		});
}

void CaribbeanModule::SetupObservers()
{
	m_caribbeanAddObserver = m_world.observer<CaribbeanComponent>()
		.event(flecs::OnAdd)
		.each([this](flecs::entity entity, CaribbeanComponent& caribbean)
		{
			CreateCaribbeanSystem(entity, caribbean);
		});

	m_caribbeanUpdateObserver = m_world.observer<CaribbeanComponent, TransformComponent>()
		.event(flecs::OnSet)
		.event<OnPhysicsUpdateTransform>()
		.event<OnCharacterUpdateTransform>()
		.each([this](flecs::entity entity, CaribbeanComponent& caribbean, TransformComponent& transform)
		{
			AdjustComponentData(entity, caribbean);
		});

	m_caribbeanRemoveObserver = m_world.observer<CaribbeanComponent>()
		.event(flecs::OnRemove)
		.event(flecs::OnDelete)
		.each([this](flecs::entity entity, CaribbeanComponent& caribbean)
		{
			RemoveCaribbeanSystem(entity);
		});
}

void CaribbeanModule::SetupQueries()
{
	m_caribbeanQuery = m_world.query_builder<CaribbeanComponent>()
		.build();
}

void CaribbeanModule::RegisterWithEditor()
{
	if (!m_editorModule) return;

	m_editorModule->RegisterComponent<CaribbeanComponent>(
		"Caribbean",
		"Rendering",
		[this](flecs::entity entity, CaribbeanComponent& component)
		{
			DrawCaribbeanInspector(entity, component);
		},
		[](flecs::entity entity) -> bool
		{
			return true;
		}
	);
}

// Adjusts the necessary flecs component data based on the compute mode.
void CaribbeanModule::AdjustComponentData(flecs::entity entity, CaribbeanComponent& caribbean)
{
	switch (caribbean.computeMode)
	{
		case ECaribbeanMode::Generic:
		{
			
		} break;
		case ECaribbeanMode::Fluid:
		{
			caribbean.hasBounds = true;
		} break;
	}
}

// No Safety Checks, UpdateUBOData assumes entity is alive and has an existing entry in the m_particleUBOs map.
void CaribbeanModule::UpdateUBOData(flecs::entity entity, const CaribbeanComponent& caribbean)
{
	CaribbeanConfigUBO tempBuffer
	{
		.origin = glm::vec4(Math::GetWorldPosition(entity) + caribbean.originOffset, 1.f),
		.boxHalfExtent = glm::vec4(caribbean.boxHalfExtent, 0.f),
		.spawnOffset = glm::vec4(caribbean.spawnOffset, 0.f),
		.accForce = glm::vec4(caribbean.accForce, 0.f),
		.particleColour = glm::vec4(caribbean.particleColour, 1.f),
		.fixedTimeStep = caribbean.fixedTimeStep,
		.maxParticles = caribbean.maxParticles,
		.hasBounds = caribbean.hasBounds, // storing bool as int for alignment

		// Spawning Parameters
		.minRadius = caribbean.minRadius,
		.maxRadius = caribbean.maxRadius,
		.minLifetime = caribbean.minLifetime,
		.maxLifetime = caribbean.maxLifetime,
		.minInitSpeed = caribbean.minInitSpeed,
		.maxInitSpeed = caribbean.maxInitSpeed
	};

	auto particleUBO = m_particleUBOs[entity.id()].get();
	particleUBO->Bind();
	particleUBO->BufferSubData(0, sizeof(CaribbeanConfigUBO), &tempBuffer);
	particleUBO->Unbind();
}

// Uses Compute Shader to spawn particles. Make sure correct UBOs and SSBOs are bound before calling this function.
void CaribbeanModule::SpawnParticles(flecs::entity entity, const CaribbeanComponent& caribbean, int spawnCount, glm::vec3 spawnOffset)
{
	auto particleSpawn = ResourceLibrary::GetComputeShader("particle_spawn");
	if (!particleSpawn) return;

	if (caribbean.hasBounds)
		spawnOffset = glm::clamp(spawnOffset, -caribbean.boxHalfExtent, caribbean.boxHalfExtent);

	m_particleUBOs[entity.id()]->BufferSubData(sizeof(glm::vec4) * 2, sizeof(glm::vec3), &spawnOffset);

	m_particleSSBOs[entity.id()]->Bind();
	m_particleSSBOs[entity.id()]->BufferSubData(0, sizeof(int), &spawnCount);
	m_particleSSBOs[entity.id()]->Unbind();

	particleSpawn->Use();
	int dispatchGroups = 1 + (caribbean.maxParticles - 1) / PARTICLE_WORKGROUP_SIZE;
	particleSpawn->Dispatch(dispatchGroups, 1, 1);
}


void CaribbeanModule::CreateCaribbeanSystem(flecs::entity entity, const CaribbeanComponent& caribbean)
{
	if (!entity.is_alive()) return;

	std::cout << "Created Caribbean system for: " << entity.name() << std::endl;
	//RemoveCaribbeanSystem(entity);

	if (!m_particleUBOs.contains(entity.id()))
	{
		m_particleUBOs[entity.id()] = std::make_unique<UniformBuffer>();
		m_particleUBOs[entity.id()]->Bind();
		m_particleUBOs[entity.id()]->BufferData(sizeof(CaribbeanConfigUBO));
		m_particleUBOs[entity.id()]->Unbind();
	}

	UpdateUBOData(entity, caribbean);

	if (!m_particleSSBOs.contains(entity.id()))
		m_particleSSBOs[entity.id()] = std::make_unique<ShaderStorageBuffer>();

	auto particleSSBO = m_particleSSBOs[entity.id()].get();
	particleSSBO->Bind();

	unsigned int currentBufferSize = particleSSBO->GetBufferSize();
	unsigned int desiredBufferSize = sizeof(glm::vec4) + sizeof(CaribbeanParticle) * (1 + ((caribbean.maxParticles - 1) / PARTICLE_WORKGROUP_SIZE)) * PARTICLE_WORKGROUP_SIZE;
	
	//std::cout << currentBufferSize << std::endl;
	//std::cout << desiredBufferSize << std::endl;

	// Resize buffer if necessary
	if (currentBufferSize != desiredBufferSize)
		particleSSBO->BufferData(desiredBufferSize);

	// Set SSBO particle data to default parameters
	for (unsigned int i = 0; i * PARTICLE_WORKGROUP_SIZE < caribbean.maxParticles; i++)
	{
		particleSSBO->BufferSubData(sizeof(glm::vec4) + sizeof(CaribbeanParticle) * PARTICLE_WORKGROUP_SIZE * i, sizeof(CaribbeanParticle) * PARTICLE_WORKGROUP_SIZE, m_particleBuffer);
	}

	//particleSSBO->ClearBufferSubData(GL_RGBA32F, sizeof(glm::vec4), sizeof(CaribbeanParticle) * caribbean.maxParticles, GL_RGBA, GL_FLOAT, &defaultParticle);
	//glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

	particleSSBO->Unbind();
}

void CaribbeanModule::RemoveCaribbeanSystem(flecs::entity entity)
{
	if(m_particleUBOs.contains(entity.id()))
		m_particleUBOs.erase(entity.id());
	
	if(m_particleSSBOs.contains(entity.id()))
		m_particleSSBOs.erase(entity.id());
}


void CaribbeanModule::DrawCaribbeanInspector(flecs::entity entity, CaribbeanComponent& component)
{
	bool changed = false;

	// Particle Compute Mode
	ImGui::TextUnformatted("Compute Mode");
	ImGui::SameLine();
	int selectedIndex = component.computeMode;
	if (ImGui::Combo("##compute_mode", &selectedIndex, ECaribbeanMode::Labels, ECaribbeanMode::END))
	{
		component.computeMode = static_cast<ECaribbeanMode::Type>(selectedIndex);
		changed = true;
	}


	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::BeginTable("##caribbean_props", 2, ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// Particle Enable and Render Toggles
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Enable Particles");
		ImGui::TableNextColumn();
		if (ImGui::Checkbox("##EnableParticles", &component.enabled))
			changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Render Particles");
		ImGui::TableNextColumn();
		if (ImGui::Checkbox("##RenderParticles", &component.visible))
			changed = true;

		// Max Simulation Steps per Frame
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Max Sim Steps");
		ImGui::TableNextColumn();
		//int imguiMaxSteps = (int)component.maxSteps;
		if (ImGui::DragInt("##MaxSteps", &component.maxSteps, 1, 1, 8))
			component.maxSteps = std::clamp(component.maxSteps, 1, 8);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Simulation Fixed Time Step
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Fixed Time Step");
		ImGui::TableNextColumn();
		if (ImGui::DragFloat("##FixedTimeStep", &component.fixedTimeStep, 0.001f, 0.005f, 0.1f))
			component.fixedTimeStep = std::clamp(component.fixedTimeStep, 0.005f, 0.1f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Origin Offset
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Offset");
		ImGui::TableNextColumn();
		ImGui::DragFloat3("##Offset", glm::value_ptr(component.originOffset), 0.1f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Particle Boundary
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Has Bounds");
		ImGui::TableNextColumn();
		if (component.computeMode == ECaribbeanMode::Fluid) ImGui::BeginDisabled();
		if (ImGui::Checkbox("##HasBounds", &component.hasBounds))
			changed = true;
		if (component.computeMode == ECaribbeanMode::Fluid) ImGui::EndDisabled();

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Box Half Extents");
		ImGui::TableNextColumn();
		if (ImGui::DragFloat3("##BoxHalfExtent", glm::value_ptr(component.boxHalfExtent), 0.1f, 0.5f))
			component.boxHalfExtent = glm::max(component.boxHalfExtent, 0.5f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Max Particle Count
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Max Particle Count");
		ImGui::TableNextColumn();
		//int imguiParticleCount = (int)component.maxParticles;
		if(ImGui::DragInt("##MaxParticleCount", &component.maxParticles, 1, 0, MAX_PARTICLES_PER_SYSTEM))
			component.maxParticles = std::clamp(component.maxParticles, 0, MAX_PARTICLES_PER_SYSTEM);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Particle Spawning
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Particle Spawn Count");
		ImGui::TableNextColumn();
		//int imguiSpawnCount = (int)component.spawnCount;
		if (ImGui::DragInt("##SpawnCount", &component.spawnCount, 1, 0, MAX_PARTICLE_SPAWN_COUNT))
			component.spawnCount = std::clamp(component.spawnCount, 0, MAX_PARTICLE_SPAWN_COUNT);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Particle Spawn Interval");
		ImGui::TableNextColumn();
		if (ImGui::DragFloat("##SpawnInterval", &component.spawnInterval, 0.01f, 0.01f))
			component.spawnInterval = std::max(component.spawnInterval, 0.01f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Particle Colour
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Particle Colour");
		ImGui::TableNextColumn();
		ImGui::ColorEdit3("##ParticleColour", &component.particleColour[0], ImGuiColorEditFlags_NoInputs);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Acceleration Force
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Acceleration Force");
		ImGui::TableNextColumn();
		ImGui::DragFloat3("##AccelerationForce", glm::value_ptr(component.accForce), 0.1f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Particle Spawning Parameters
		switch (component.computeMode)
		{
			case ECaribbeanMode::Generic:
			{
				// Radius
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Particle Radius");
				ImGui::TableNextColumn();
				ImGui::DragFloatRange2("##ParticleRadius", &component.minRadius, &component.maxRadius, 0.01f, 0.1f);
				component.minRadius = std::max(component.minRadius, 0.1f);
				component.maxRadius = std::max(component.maxRadius, component.minRadius);
				if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

				// Lifetime
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Particle Lifetime");
				ImGui::TableNextColumn();
				ImGui::DragFloatRange2("##ParticleLifetime", &component.minLifetime, &component.maxLifetime, 0.1f, 1.f);
				component.minLifetime = std::max(component.minLifetime, 1.f);
				component.maxLifetime = std::max(component.maxLifetime, component.minLifetime);
				if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

				// Initial Speed
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Particle Initial Speed");
				ImGui::TableNextColumn();
				ImGui::DragFloatRange2("##ParticleInitSpeed", &component.minInitSpeed, &component.maxInitSpeed, 0.1f, 0.f);
				component.minInitSpeed = std::max(component.minInitSpeed, 0.f);
				component.maxInitSpeed = std::max(component.maxInitSpeed, component.minInitSpeed);
				if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
			} break;
			case ECaribbeanMode::Fluid:
			{

			} break;
		}

		ImGui::EndTable();
	}

	if (changed) entity.modified<CaribbeanComponent>();
}