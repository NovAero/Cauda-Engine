#include "cepch.h"
#include "AudioModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Core/Editor/EditorModule.h"
#include "Platform/Win32/Application.h"
#include "Config/EngineDefaults.h"

AudioModule::AudioModule(flecs::world& world) :
	m_world(world)
{
	SetupComponents();
	SetupObservers();
	SetupQueries();
	SetupSystems();

	RegisterWithEditor();

	m_audioEngine = std::make_unique<SL::Soloud>();

	m_audioEngine->init();
	SetupMixerSettings();
}

AudioModule::~AudioModule()
{
	Shutdown();
}

void AudioModule::Shutdown()
{
	if (!m_audioEngine) return;
	m_audioEngine->stopAll();
	m_audioEngine->deinit();
}

const SL::Soloud* AudioModule::GetSoundEngine()
{
	return m_audioEngine.get();
}

void AudioModule::SelectSoundAsset(AudioAsset* asset)
{
	m_selectedAsset = asset;
}

void AudioModule::SetListenerTransform(TransformComponent& transform)
{
	glm::vec3 up = transform.rotation * glm::vec3(0, 1, 0);

	m_audioEngine->set3dListenerParameters(
		transform.position.x, transform.position.y, transform.position.z,
		transform.cachedEuler.x, transform.cachedEuler.y, transform.cachedEuler.z,
		up.x, up.y, up.z);
}

void AudioModule::SetListenerPosition(glm::vec3 pos)
{
	m_audioEngine->set3dListenerPosition(pos.x, pos.y, pos.z);
}

void AudioModule::DrawAudioMixerInWindow()
{
	static float master = m_masterBus.mVolume;
	static float BGM = m_BGMBus.mVolume;
	static float SFX = m_SFXBus.mVolume;
	static float ambient = m_ambientBus.mVolume;

	bool edited = false;
	static bool shouldSave = false;

	if (ImGui::SliderFloat("Master", &master, 0.f, 1.f))
	{
		m_audioEngine->setVolume(m_masterBus.mChannelHandle, master);
		edited = true;
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) shouldSave = true;

	if (ImGui::SliderFloat("BGM", &BGM, 0.f, 1.f))
	{
		m_audioEngine->setVolume(m_BGMBus.mChannelHandle, BGM);
		edited = true;
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) shouldSave = true;

	if (ImGui::SliderFloat("SFX", &SFX, 0.f, 1.f))
	{
		m_audioEngine->setVolume(m_SFXBus.mChannelHandle, SFX);
		edited = true;
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) shouldSave = true;

	if (ImGui::SliderFloat("Ambient", &ambient, 0.f, 1.f))
	{
		m_audioEngine->setVolume(m_ambientBus.mChannelHandle, ambient);
		edited = true;
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) shouldSave = true;

	if (edited) {
		g_EngineSettingsEntity.set<AudioMixerSettings>(AudioMixerSettings{
			master,
			BGM,
			SFX,
			ambient
			});
	}

	if (shouldSave)
	{
		shouldSave = false;
		SaveEngineSettings();
	}

}

void AudioModule::SetupMixerSettings()
{
	m_audioEngine->setMaxActiveVoiceCount(128);

	m_masterBus.mSoloud = m_audioEngine.get();
	m_SFXBus.mSoloud = m_audioEngine.get();
	m_BGMBus.mSoloud = m_audioEngine.get();
	m_ambientBus.mSoloud = m_audioEngine.get();

	if (auto mixerSettings = g_EngineSettingsEntity.try_get_mut<AudioMixerSettings>())
	{
		m_masterBus.setVolume(mixerSettings->masterVolume);
		m_BGMBus.setVolume(mixerSettings->bgmVolume);
		m_SFXBus.setVolume(mixerSettings->sfxVolume);
		m_ambientBus.setVolume(mixerSettings->ambientVolume);
	}

	//Attach all the buses into the master bus
	m_audioEngine->play(m_masterBus);
	m_masterBus.play(m_BGMBus);
	m_masterBus.play(m_SFXBus);
	m_masterBus.play(m_ambientBus);
}

void AudioModule::SetupComponents()
{
	m_world.component<SoundType>()
		.constant("WAVE", SoundType::WAVE)
		.constant("STREAM", SoundType::STREAM);

	m_world.component<AttenuationModel>()
		.constant("NO_ATTENUATION", AttenuationModel::NO_ATTENUATION)
		.constant("INVERSE_DISTANCE", AttenuationModel::INVERSE_DISTANCE)
		.constant("LINEAR_DISTANCE", AttenuationModel::LINEAR_DISTANCE)
		.constant("EXPONENTIAL_DISTANCE", AttenuationModel::EXPONENTIAL_DISTANCE);

	m_world.component<AudioChannel>()
		.constant("MASTER", AudioChannel::MASTER)
		.constant("MUSIC", AudioChannel::MUSIC)
		.constant("EFFECTS", AudioChannel::EFFECTS)
		.constant("AMBIENT", AudioChannel::AMBIENT)
		.constant("NUM_BUSES", AudioChannel::NUM_BUSES);

	m_world.component<PlaybackSettings>()
		.member<SoundType>("type")
		.member<AudioChannel>("bus")
		.member<float>("relativeVolume")
		.member<float>("relativePlaySpeed")
		.member<float>("pan")
		.member<bool>("loop")
		.member<bool>("paused")
		.member<bool>("use3D")
		.member<glm::vec3>("position")
		.member<glm::vec3>("vector")
		.member<float>("minDistance")
		.member<float>("maxDistance")
		.member<float>("dopplerFactor")
		.member<AttenuationModel>("attenuationModel")
		.member<float>("attenuationFactor");

	m_world.component<AudioEmitterComponent>()
		.member<std::string>("audioName")
		.member<PlaybackSettings>("settings");
}

void AudioModule::SetupObservers()
{
	m_audioEmitterSetObserver = m_world.observer<AudioEmitterComponent>()
		.event(flecs::OnSet)
		.each([&](flecs::entity entity, AudioEmitterComponent& component)
			{
				auto it = std::find_if(m_emitterEntities.begin(), m_emitterEntities.end(), [&](const AudioEmitterComponent& comp) -> bool
					{
						return (comp.owningEntity == entity.id());
					});
				if (it == m_emitterEntities.end())
				{
					m_emitterEntities.push_back(component);
				}
				else
				{
					*it = component;
				}
			});

	m_audioEmitterRemoveObserver = m_world.observer<AudioEmitterComponent>()
		.event(flecs::OnRemove)
		.each([&](flecs::entity entity, AudioEmitterComponent& component)
			{
				auto it = std::find_if(m_emitterEntities.begin(), m_emitterEntities.end(), [&](const AudioEmitterComponent& comp) -> bool
					{
						return (comp.owningEntity == entity.id());
					});

				if (it != m_emitterEntities.end())
				{
					if (it->handle != -1)
					{
						TryStopAudioSource(it->handle);
					}
					m_emitterEntities.erase(it);
				}
			});
}

void AudioModule::SetupQueries()
{

}

void AudioModule::SetupSystems()
{
	m_cleanupEmptyHandlesSystem = m_world.system()
		.kind(flecs::PostLoad)
		.each([&]() 
			{
				for (auto& [assetHandle, audioHandles] : m_handles)
				{
					//Search for invalid handles
					std::vector<int> invalidHandles;
					for (int i = 0; i < audioHandles.size(); ++i)
					{
						if (!m_audioEngine->isValidVoiceHandle(audioHandles[i]))
						{
							invalidHandles.push_back(i);
						}
					}
					
					//Remove handles from the internal vector
					for (int index : invalidHandles)
					{
						if (index >= audioHandles.size()) continue;
						std::cout << "Cleaned up audio handle " << audioHandles[index] << '\n';
						auto it = std::find_if(m_emitterEntities.begin(), m_emitterEntities.end(), [&](const AudioEmitterComponent& comp)
							{
								return (comp.handle == audioHandles[index]);
							});
						if (it != m_emitterEntities.end())
						{
							it->handle = -1;
						}
						audioHandles.erase(audioHandles.begin() + index);
					}
				}
			});

	m_update3DAudioSystem = m_world.system()
		.kind(flecs::OnValidate)
		.each([&]()
			{
				m_audioEngine->update3dAudio();
			});
}


void AudioModule::RelinkBuses()
{
	m_audioEngine->play(m_masterBus);
	m_masterBus.play(m_BGMBus);
	m_audioEngine->setProtectVoice(m_BGMBus.mChannelHandle, true);
	m_masterBus.play(m_SFXBus);
	m_masterBus.play(m_ambientBus);
}

void AudioModule::RegisterWithEditor()
{
	auto editorModule = m_world.try_get_mut<EditorModule>();
	if (editorModule)
	{
		editorModule->RegisterComponent<AudioEmitterComponent>("Audio Emitter", "Audio",
			[&](flecs::entity entity, AudioEmitterComponent& comp)
			{
				DrawAudioEmitterInspector(entity, comp);
			},
			[](flecs::entity entity) -> bool
			{
				return true;
			});
	}
}

void AudioModule::DrawSoundAssetEditor(bool& open)
{
	if (open)
	{
		if (ImGui::Begin("Sound Asset Editor", &open, ImGuiWindowFlags_NoDocking))
		{
			static int handle = -1;
			bool playSound = false;

			ImGui::Text("%s | %s", m_selectedAsset->name.c_str(), (bool)m_selectedAsset->settings.type ? "Stream" : "Wave");

			if (ImGui::Button("Play Sound"))
			{
				playSound = true;
			}
			ImGui::SameLine();

			if (ImGui::Button("Stop Sound"))
				TryStopAudioSource(handle);

			DrawPlaybackSettings(m_selectedAsset->settings);

			if (playSound)
			{
				if (m_selectedAsset->settings.use3D)
				{
					handle = PlaySound3D(m_selectedAsset, (AudioChannel)m_selectedAsset->settings.bus);
				}
				else
				{
					handle = PlaySound2D(m_selectedAsset, (AudioChannel)m_selectedAsset->settings.bus);
				}
			}
			
		}
		ImGui::End();
	}
	else
	{
		m_selectedAsset = nullptr;
		open = true;
	}
}

void AudioModule::DrawAudioEmitterInspector(flecs::entity entity, AudioEmitterComponent& component)
{
	ImGui::Text("Handle: %i", component.handle);
	ImGui::Separator();

	std::vector<const char*> allSounds = ResourceLibrary::GetAllAudioHandles();

	if (ImGui::BeginCombo("Audio Asset", component.audioName.c_str()))
	{
		for (int i = 0; i < allSounds.size(); ++i)
		{
			std::string item = allSounds[i];
			bool selected = false;
			if (ImGui::Selectable(item.c_str()))
			{
				component.audioName = item;
				if (auto asset = ResourceLibrary::GetAudioAsset(item))
				{
					TryStopAudioSource(component.handle);
					component.settings = asset->settings;
				}
			}
		}
		ImGui::EndCombo();
	}

	if (DrawPlaybackSettings(component.settings))
	{
		UpdateSoundAfterEdit(component.handle, component.audioName, component.settings);
		entity.modified<AudioEmitterComponent>();
	}

	bool playing = IsValidHandle(component.handle);
	if (!playing)
	{
		if (ImGui::Button("Play Sound"))
		{
			TryPlaySoundFromComponent(component);
			playing = true;
		}
	}
	else
	{
		if (ImGui::Button("Stop Sound"))
		{
			playing = false;
			TryStopAudioSource(component.handle);
		}
	}
}

bool AudioModule::DrawPlaybackSettings(PlaybackSettings& settings)
{
	bool edited = false;
	if (ImGui::BeginTable("##Settings", 2))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		static int busType = (int)settings.bus;
		const char* buses[] = { "Master", "Music", "Effects", "Ambient" };
		ImGui::Text("Bus");

		ImGui::TableNextColumn();

		if (ImGui::Combo("##bus_Combo", &busType, buses, 4))
		{
			settings.bus = (AudioChannel)busType;
			edited = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Relative Volume");
		ImGui::TableNextColumn();
		ImGui::SliderFloat("##volume", &settings.relativeVolume, 0.f, 1.0f);
		if (ImGui::IsItemDeactivatedAfterEdit())
			edited = true;

		ImGui::SameLine();
		if (ImGui::Button("Reset Volume"))
		{
			settings.relativeVolume = -1;
			edited = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Rel. Play Speed");
		ImGui::TableNextColumn();
		ImGui::SliderFloat("##relative_speed", &settings.relativePlaySpeed, 0.01f, 5.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
				edited = true;

		ImGui::SameLine();
		if (ImGui::Button("Reset Speed"))
		{
			settings.relativePlaySpeed = 1;
			edited = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Panning");
		ImGui::TableNextColumn();
		ImGui::SliderFloat("##panning", &settings.pan, -1.f, 1.0f);
		if (ImGui::IsItemDeactivatedAfterEdit())
			edited = true;

		ImGui::SameLine();
		if (ImGui::Button("Reset Pan"))
		{
			settings.pan = 0;
			edited = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("Loop");
		ImGui::TableNextColumn();
		if (ImGui::Checkbox("##is_looping", &settings.loop)) edited = true;

		ImGui::EndTable();

		ImGui::Separator();

		if (ImGui::Checkbox("3D Audio", &settings.use3D)) edited = true;
		if (settings.use3D)
		{
			static int selectedAttType = (unsigned int)settings.attenuationModel;
			const char* attTypes[] = { "None", "Inverse Distance", "Linear Distance", "Exponential Distance" };

			if (ImGui::BeginTable("3D Audio Settings", 2))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				/* Come back to this
				static bool useTransform = false;
				ImGui::Checkbox("Use Transform", &useTransform);*/
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Position");
				ImGui::TableNextColumn();
				ImGui::DragFloat3("##emit_position", glm::value_ptr(settings.position), 1.f, -FLT_MAX, FLT_MAX);
				if(ImGui::IsItemDeactivatedAfterEdit())
					edited = true;

				ImGui::SameLine();
				if (ImGui::Button("Reset Position"))
				{
					settings.position = glm::vec3(0);
					edited = true;
				}

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Vector");
				ImGui::TableNextColumn();
				ImGui::DragFloat3("##directional_vector", glm::value_ptr(settings.vector), 1.f, -FLT_MAX, FLT_MAX);
				if (ImGui::IsItemDeactivatedAfterEdit())
					edited = true;

				ImGui::SameLine();
				if (ImGui::Button("Reset Vector"))
				{
					settings.vector = glm::vec3(0);
					edited = true;
				}

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Min Distance");
				ImGui::TableNextColumn();
				ImGui::DragFloat("##min_distance", &settings.minDistance, 1.f, 0.f, FLT_MAX);

				ImGui::SameLine();
				if (ImGui::Button("Reset Min"))
				{
					settings.minDistance = 1.f;
					edited = true;
				}

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Max Distance");
				ImGui::TableNextColumn();
				ImGui::DragFloat("##max_distance", &settings.maxDistance, 1.f, 0.f, FLT_MAX);
				if (ImGui::IsItemDeactivatedAfterEdit())
					edited = true;

				ImGui::SameLine();
				if (ImGui::Button("Reset Max"))
				{
					settings.maxDistance = 1000000.f;
					edited = true;
				}

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Doppler Factor");
				ImGui::TableNextColumn();
				ImGui::DragFloat("##doppler_factor", &settings.dopplerFactor, 1.f, 0.f, FLT_MAX);
				if (ImGui::IsItemDeactivatedAfterEdit())
					edited = true;

				ImGui::SameLine();
				if (ImGui::Button("Reset Doppler"))
				{
					settings.dopplerFactor = 1;
					edited = true;
				}

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Attenuation Model");
				ImGui::TableNextColumn();

				if (ImGui::Combo(attTypes[selectedAttType], &selectedAttType, attTypes, 4))
				{
					settings.attenuationModel = (AttenuationModel)selectedAttType;
					edited = true;
				}

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("Attenuation Factor");
				ImGui::TableNextColumn();
				ImGui::DragFloat("##attenuation_factor", &settings.attenuationFactor, 0.01f, 0.01f, FLT_MAX);
				if (ImGui::IsItemDeactivatedAfterEdit())
					edited = true;

				ImGui::SameLine();
				if (ImGui::Button("Reset Attenuation")) settings.attenuationFactor = 1.f;

				ImGui::EndTable();
			}
		}
	}
	return edited;
}

bool AudioModule::UpdateSoundAfterEdit(int& handle, const std::string& audioAssetName, PlaybackSettings settings)
{
	//if sound isn't playing
	if (!IsValidHandle(handle)) return false;

	m_audioEngine->setRelativePlaySpeed(handle, settings.relativePlaySpeed);
	m_audioEngine->setVolume(handle, settings.relativeVolume);
	m_audioEngine->setLooping(handle, settings.loop);
	m_audioEngine->setPause(handle, settings.paused);
	m_audioEngine->setPan(handle, settings.pan);

	if (settings.use3D) //Update 3D audio settings
	{
		m_audioEngine->set3dSourceParameters(handle,
			settings.position.x, settings.position.y, settings.position.z,
			settings.vector.x, settings.vector.y, settings.vector.z);
		m_audioEngine->set3dSourceMinMaxDistance(handle, settings.minDistance, settings.maxDistance);
		m_audioEngine->set3dSourceAttenuation(handle, (int)settings.attenuationModel, settings.attenuationFactor);
		m_audioEngine->set3dSourceDopplerFactor(handle, settings.dopplerFactor);
	}
	else //Reset to defaults
	{
		m_audioEngine->set3dSourceParameters(handle, 0, 0, 0);
		m_audioEngine->set3dSourceMinMaxDistance(handle, 1, 1000000);
		m_audioEngine->set3dSourceAttenuation(handle, 0, 1.f);
		m_audioEngine->set3dSourceDopplerFactor(handle, 1.f);
	}
	return true;

}

int AudioModule::PlaySound2D(AudioAsset* sound, AudioChannel channel)
{
	if (!sound) return -1;
	if (sound->settings.type == SoundType::WAVE)
	{
		return PlaySoundWave2D(*reinterpret_cast<SoundWave*>(sound), channel);
	}
	else
	{
		return PlaySoundStream2D(*reinterpret_cast<SoundStream*>(sound), channel);
	}
}

int AudioModule::ForceSound2D(AudioAsset* sound, AudioChannel channel)
{
	if (!sound) return -1;
	if (sound->settings.type == SoundType::WAVE)
	{
		return ForceSoundWave2D(*reinterpret_cast<SoundWave*>(sound), channel);
	}
	else
	{
		return ForceSoundStream2D(*reinterpret_cast<SoundStream*>(sound), channel);
	}
}

int AudioModule::PlaySound3D(AudioAsset* sound, AudioChannel channel)
{
	if (!sound) return -1;
	if (sound->settings.type == SoundType::WAVE)
	{
		return PlaySoundWave3D(*reinterpret_cast<SoundWave*>(sound), channel);
	}
	else
	{
		return PlaySoundStream3D(*reinterpret_cast<SoundStream*>(sound), channel);
	}
}

int AudioModule::ForceSound3D(AudioAsset* sound, AudioChannel channel)
{
	if (!sound) return -1;
	if (sound->settings.type == SoundType::WAVE)
	{
		return ForceSoundWave3D(*reinterpret_cast<SoundWave*>(sound), channel);
	}
	else
	{
		return ForceSoundStream3D(*reinterpret_cast<SoundStream*>(sound), channel);
	}
}

int AudioModule::PlaySoundWave2D(SoundWave& sound, AudioChannel channel)
{
	int handle = -1;
	float delta = Cauda::Application::Get().GetWorld().delta_time();
	switch (channel)
	{
	case AudioChannel::MASTER: { handle = m_masterBus.playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::MUSIC: { handle = m_BGMBus.playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::EFFECTS: { handle = m_SFXBus.playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::AMBIENT: { handle = m_ambientBus.playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	default: { handle = m_audioEngine->playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan, (int)sound.settings.bus); } break;
	}

	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		if (channel == AudioChannel::MUSIC) //Protect the bgm
		{
			m_audioEngine->setProtectVoice(handle, true);
		}
		m_audioEngine->setLooping(handle, sound.settings.loop);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::ForceSoundWave2D(SoundWave& sound, AudioChannel channel)
{
	int handle = -1;
	switch (channel)
	{
	case AudioChannel::MASTER: { handle = m_masterBus.play(sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::MUSIC: { handle = m_BGMBus.play(sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::EFFECTS: { handle = m_SFXBus.play(sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::AMBIENT: { handle = m_ambientBus.play(sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	default: { handle = m_audioEngine->play(sound.data, sound.settings.relativeVolume, sound.settings.pan, (int)sound.settings.bus); } break;
	}

	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		if (channel == AudioChannel::MUSIC) //Protect the bgm
		{
			m_audioEngine->setProtectVoice(handle, true);
		}
		m_audioEngine->setLooping(handle, sound.settings.loop);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::PlaySoundWave3D(SoundWave& sound, AudioChannel channel)
{
	int handle = -1;

	float delta = Cauda::Application::Get().GetWorld().delta_time();
	switch (channel)
	{
	case AudioChannel::MASTER:
	{
		handle = m_masterBus.play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::MUSIC:
	{
		m_BGMBus.play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::EFFECTS:
	{
		handle = m_SFXBus.play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::AMBIENT:
	{
		handle = m_ambientBus.play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	default:
	{
		handle = m_audioEngine->play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume, (int)sound.settings.bus);
	} break;
	}
	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		if (channel == AudioChannel::MUSIC) //Protect the bgm
		{
			m_audioEngine->setProtectVoice(handle, true);
		}
		m_audioEngine->setLooping(handle, sound.settings.loop);
		m_audioEngine->set3dSourceMinMaxDistance(handle, sound.settings.minDistance, sound.settings.maxDistance);
		m_audioEngine->set3dSourceAttenuation(handle, (unsigned int)sound.settings.attenuationModel, sound.settings.attenuationFactor);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::ForceSoundWave3D(SoundWave& sound, AudioChannel channel)
{
	int handle = -1;

	switch (channel)
	{
	case AudioChannel::MASTER:
	{
		handle = m_masterBus.play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::MUSIC:
	{
		handle = m_BGMBus.play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::EFFECTS:
	{
		handle = m_SFXBus.play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::AMBIENT:
	{
		handle = m_ambientBus.play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	default:
	{
		handle = m_audioEngine->play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume, (int)sound.settings.bus);
	} break;
	}

	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		if (channel == AudioChannel::MUSIC) //Protect the bgm
		{
			m_audioEngine->setProtectVoice(handle, true);
		}
		m_audioEngine->setLooping(handle, sound.settings.loop);
		m_audioEngine->set3dSourceMinMaxDistance(handle, sound.settings.minDistance, sound.settings.maxDistance);
		m_audioEngine->set3dSourceAttenuation(handle, (unsigned int)sound.settings.attenuationModel, sound.settings.attenuationFactor);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::PlaySoundStream2D(SoundStream& sound, AudioChannel channel)
{
	int handle = -1;
	float delta = Cauda::Application::Get().GetWorld().delta_time();
	switch (channel)
	{
	case AudioChannel::MASTER: { handle = m_masterBus.playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::MUSIC: { handle = m_BGMBus.playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::EFFECTS: { handle = m_SFXBus.playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::AMBIENT: { handle = m_ambientBus.playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	default: { handle = m_audioEngine->playClocked(delta, sound.data, sound.settings.relativeVolume, sound.settings.pan, (int)sound.settings.bus); } break;
	}

	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		if (channel == AudioChannel::MUSIC) //Protect the bgm
		{
			m_audioEngine->setProtectVoice(handle, true);
		}
		m_audioEngine->setLooping(handle, sound.settings.loop);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::ForceSoundStream2D(SoundStream& sound, AudioChannel channel)
{
	int handle = -1;
	switch (channel)
	{
	case AudioChannel::MASTER: { handle = m_masterBus.play(sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::MUSIC: { handle = m_BGMBus.play(sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::EFFECTS: { handle = m_SFXBus.play(sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	case AudioChannel::AMBIENT: { handle = m_ambientBus.play(sound.data, sound.settings.relativeVolume, sound.settings.pan); } break;
	default: { handle = m_audioEngine->play(sound.data, sound.settings.relativeVolume, sound.settings.pan, (int)sound.settings.bus); } break;
	}

	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		if (channel == AudioChannel::MUSIC) //Protect the bgm
		{
			m_audioEngine->setProtectVoice(handle, true);
		}
		m_audioEngine->setLooping(handle, sound.settings.loop);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::PlaySoundStream3D(SoundStream& sound, AudioChannel channel)
{
	int handle = -1;

	float delta = Cauda::Application::Get().GetWorld().delta_time();
	switch (channel)
	{
	case AudioChannel::MASTER:
	{
		handle = m_masterBus.play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::MUSIC:
	{
		m_BGMBus.play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::EFFECTS:
	{
		handle = m_SFXBus.play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::AMBIENT:
	{
		handle = m_ambientBus.play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	default:
	{
		handle = m_audioEngine->play3dClocked(delta, sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume, (int)sound.settings.bus);
	} break;
	}
	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		m_audioEngine->setLooping(handle, sound.settings.loop);
		if (channel == AudioChannel::MUSIC) //Protect the bgm
		{
			m_audioEngine->setProtectVoice(handle, true);
		}
		m_audioEngine->set3dSourceMinMaxDistance(handle, sound.settings.minDistance, sound.settings.maxDistance);
		m_audioEngine->set3dSourceAttenuation(handle, (unsigned int)sound.settings.attenuationModel, sound.settings.attenuationFactor);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::ForceSoundStream3D(SoundStream& sound, AudioChannel channel)
{
	int handle = -1;

	switch (channel)
	{
	case AudioChannel::MASTER:
	{
		handle = m_masterBus.play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::MUSIC:
	{
		handle = m_BGMBus.play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::EFFECTS:
	{
		handle = m_SFXBus.play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	case AudioChannel::AMBIENT:
	{
		handle = m_ambientBus.play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume);
	} break;
	default:
	{
		handle = m_audioEngine->play3d(sound.data,
			sound.settings.position.x, sound.settings.position.y, sound.settings.position.z,
			sound.settings.vector.x, sound.settings.vector.y, sound.settings.vector.z,
			sound.settings.relativeVolume, (int)sound.settings.bus);
	} break;
	}

	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		if (channel == AudioChannel::MUSIC) //Protect the bgm
		{
			m_audioEngine->setProtectVoice(handle, true);
		}
		m_audioEngine->setLooping(handle, sound.settings.loop);
		m_audioEngine->set3dSourceMinMaxDistance(handle, sound.settings.minDistance, sound.settings.maxDistance);
		m_audioEngine->set3dSourceAttenuation(handle, (unsigned int)sound.settings.attenuationModel, sound.settings.attenuationFactor);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::PlaySoundStreamBackground(SoundStream& sound)
{
	int handle = m_BGMBus.play(sound.data, sound.settings.relativeVolume, 0, sound.settings.paused);
	if (handle != -1)
	{
		m_handles[sound.name].push_back(handle);
		m_audioEngine->setLooping(handle, sound.settings.loop);
		m_audioEngine->setProtectVoice(handle, true);
		m_audioEngine->setRelativePlaySpeed(handle, sound.settings.relativePlaySpeed);
	}
	return handle;
}

int AudioModule::TryPlaySound2D(std::string soundName, AudioChannel channel)
{
	auto sound = ResourceLibrary::GetSoundWave(soundName);
	if (sound)
	{
		return PlaySoundWave2D(*sound, channel);
	}
	return -1;
}

int AudioModule::TryForceSound2D(std::string soundName, AudioChannel channel)
{
	auto sound = ResourceLibrary::GetSoundWave(soundName);
	if (sound)
	{
		ForceSoundWave2D(*sound, channel);
	}
	return -1;
}

int AudioModule::TryPlaySoundWave3D(std::string soundName, glm::vec3 pos, AudioChannel channel)
{
	int handle = -1;
	auto sound = ResourceLibrary::GetSoundWave(soundName);
	if (sound)
	{
		float delta = Cauda::Application::Get().GetWorld().delta_time();
		switch (channel)
		{
		case AudioChannel::MASTER:
		{
			handle = m_masterBus.play3dClocked(delta, sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume);
		} break;
		case AudioChannel::MUSIC:
		{
			m_BGMBus.play3dClocked(delta, sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume);
		} break;
		case AudioChannel::EFFECTS:
		{
			handle = m_SFXBus.play3dClocked(delta, sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume);
		} break;
		case AudioChannel::AMBIENT:
		{
			handle = m_ambientBus.play3dClocked(delta, sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume);
		} break;
		default:
		{
			handle = m_audioEngine->play3dClocked(delta, sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume, (int)sound->settings.bus);
		} break;
		}
		if (handle != -1)
		{
			m_handles[sound->name].push_back(handle);
			m_audioEngine->setLooping(handle, sound->settings.loop);
			m_audioEngine->set3dSourceMinMaxDistance(handle, sound->settings.minDistance, sound->settings.maxDistance);
			m_audioEngine->set3dSourceAttenuation(handle, (unsigned int)sound->settings.attenuationModel, sound->settings.attenuationFactor);
			m_audioEngine->setRelativePlaySpeed(handle, sound->settings.relativePlaySpeed);
		}
	}
	return handle;
}

int AudioModule::TryForceSoundWave3D(std::string soundName, glm::vec3 pos, AudioChannel channel)
{
	int handle = -1;
	auto sound = ResourceLibrary::GetSoundWave(soundName);
	if (sound)
	{
		switch (channel)
		{
		case AudioChannel::MASTER:
		{
			handle = m_masterBus.play3d(sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume);
		} break;
		case AudioChannel::MUSIC:
		{
			handle = m_BGMBus.play3d(sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume);
		} break;
		case AudioChannel::EFFECTS:
		{
			handle = m_SFXBus.play3d(sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume);
		} break;
		case AudioChannel::AMBIENT:
		{
			handle = m_ambientBus.play3d(sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume);
		} break;
		default:
		{
			handle = m_audioEngine->play3d(sound->data,
				sound->settings.position.x, sound->settings.position.y, sound->settings.position.z,
				sound->settings.vector.x, sound->settings.vector.y, sound->settings.vector.z,
				sound->settings.relativeVolume, (int)sound->settings.bus);
		} break;
		}

		if (handle != -1)
		{
			m_handles[sound->name].push_back(handle);
			m_audioEngine->setLooping(handle, sound->settings.loop);
			m_audioEngine->set3dSourceMinMaxDistance(handle, sound->settings.minDistance, sound->settings.maxDistance);
			m_audioEngine->set3dSourceAttenuation(handle, (unsigned int)sound->settings.attenuationModel, sound->settings.attenuationFactor);
			m_audioEngine->setRelativePlaySpeed(handle, sound->settings.relativePlaySpeed);
		}
	}

	return handle;
}

int AudioModule::TryPlaySoundStreamBackground(std::string soundName)
{
	auto sound = ResourceLibrary::GetSoundStream(soundName);
	if (sound)
	{
		return PlaySoundStreamBackground(*sound);
	}
	return -1;
}

int AudioModule::TryPlaySoundFromEntity(flecs::entity entity)
{
	if (auto audioComp = entity.try_get_mut<AudioEmitterComponent>())
	{
		return TryPlaySoundFromComponent(*audioComp);
	}
	return -1;
}

int AudioModule::TryPlaySoundFromComponent(AudioEmitterComponent& component)
{
	AudioAsset* asset = ResourceLibrary::GetAudioAsset(component.audioName);
	PlaybackSettings cache = asset->settings;
	asset->settings = component.settings;
	if (component.settings.use3D)
	{
		component.handle = PlaySound3D(asset, component.settings.bus);
	}
	else
	{
		component.handle = PlaySound2D(asset, component.settings.bus);
	}
	asset->settings = cache;
	return component.handle;
}

void AudioModule::SetGlobalVolume(float volume)
{
	m_audioEngine->setGlobalVolume(volume);
}

float AudioModule::GetGlobalVolume()
{
	return m_audioEngine->getGlobalVolume();
}

bool AudioModule::IsValidHandle(int handle)
{
	return m_audioEngine->isValidVoiceHandle(handle);
}

bool AudioModule::IsValidHandleForSource(std::string source, int handle)
{
	if (!m_handles.contains(source)) return false;
	return m_audioEngine->isValidVoiceHandle(handle) && 
		std::find(m_handles[source].begin(), m_handles[source].end(), handle) != m_handles[source].end();
}

bool AudioModule::TryStopAudioSource(int handle)
{
	if (IsValidHandle(handle))
	{
		m_audioEngine->stop(handle);
		return true;
	}
	return false;
}

bool AudioModule::TryStopAudioSource(std::string name, int handle)
{
	if (!m_handles.contains(name)) return false;

	for (int& h : m_handles[name])
	{
		if (handle == h)
		{
			m_audioEngine->stop(h);
			return true;
		}
	}
	return false;
}

bool AudioModule::TryStopAudioSource(AudioAsset* source, int handle)
{
	if (!m_handles.contains(source->name)) return false;

	for (int& h : m_handles[source->name])
	{
		if (handle == h)
		{
			m_audioEngine->stop(h);
			return true;
		}
	}
	return false;
}

void AudioModule::StopAllAudioFromSource(AudioAsset* source)
{
	if (!m_handles.contains(source->name)) return;

	for (int& h : m_handles[source->name])
	{
		m_audioEngine->stop(h);
	}
}

void AudioModule::StopAllAudio()
{
	m_audioEngine->stopAll();
}

void AudioModule::StopBGM()
{
}

void AudioModule::OnImGuiRender()
{
	static bool soundEditorOpen = true;
	if (m_selectedAsset)
	{
		DrawSoundAssetEditor(soundEditorOpen);
	}


	if(ImGui::Begin("Audio Settings")) {

		DrawAudioMixerInWindow();
		
		ImGui::Separator();
		ImGui::Text("Audio Debug");
		ImGui::Separator();
		static bool paused = false;
		if (ImGui::Button(paused ? "Play BGM" : "Pause BGM"))
		{
			paused = !paused;
			m_audioEngine->setPause(m_BGMBus.mChannelHandle, paused);
		}

		ImGui::Text("Use only when an unstoppable sound is in play");
		ImGui::Text("breaks the buses (Press 3 times)");
		if (ImGui::Button("Stop All Sounds"))
		{
			static int pressCount = 0;
			pressCount++;

			if (pressCount >= 3)
			{
				StopAllAudio();
				RelinkBuses();
				pressCount = 0;
			}
		}
	}
	ImGui::End();
}
