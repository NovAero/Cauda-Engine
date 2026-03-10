#pragma once
#include "soloud.h"
#include "AudioAsset.h"

using AssetHandle = std::string;
namespace SL = SoLoud;

namespace Cauda
{
    class EditorLayer;
}

class AudioModule
{
    friend class Cauda::EditorLayer;
public:

    AudioModule(flecs::world& world);
    ~AudioModule();

    AudioModule(const AudioModule&) = delete;
    AudioModule& operator=(const AudioModule&) = delete;
    AudioModule(AudioModule&&) = delete;
    AudioModule& operator=(AudioModule&&) = delete;

    void OnImGuiRender();

    //Typeless functions

    //Plays a sound and handles type checking (don't use during tick events, involves a cast)
    int PlaySound2D(AudioAsset* sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    //Plays a sound and handles type checking (don't use during tick events, involves a cast)
    int ForceSound2D(AudioAsset* sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    //Plays a sound at a point and handles type checking (don't use during tick events, involves a cast)
    int PlaySound3D(AudioAsset* sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    //Plays a sound at a point and handles type checking (don't use during tick events, involves a cast)
    int ForceSound3D(AudioAsset* sound, AudioChannel channel = AudioChannel::NUM_BUSES);

    //Sound wave functions

    int PlaySoundWave2D(SoundWave& sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    int ForceSoundWave2D(SoundWave& sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    int PlaySoundWave3D(SoundWave& sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    int ForceSoundWave3D(SoundWave& sound, AudioChannel channel = AudioChannel::NUM_BUSES);

    //Sound stream functions

    int PlaySoundStream2D(SoundStream& sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    int ForceSoundStream2D(SoundStream& sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    int PlaySoundStream3D(SoundStream& sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    int ForceSoundStream3D(SoundStream& sound, AudioChannel channel = AudioChannel::NUM_BUSES);
    int PlaySoundStreamBackground(SoundStream& sound);
    
    //Type safe functions

    //Tries to find and play the sound, waits if the sound is already being played to avoid overlaps
    int TryPlaySound2D(std::string soundName, AudioChannel channel = AudioChannel::NUM_BUSES);
    //Tries to find and play the sound the sound regardless of other sounds playing, can cause overlaps
    int TryForceSound2D(std::string soundName, AudioChannel channel = AudioChannel::NUM_BUSES);
    //Plays audio in a point in space, waits if the sound is already being played to avoid overlaps
    int TryPlaySoundWave3D(std::string soundName, glm::vec3 pos, AudioChannel channel = AudioChannel::NUM_BUSES);
    //Plays audio in a point in space regardless of other sounds playing, can cause overlaps
    int TryForceSoundWave3D(std::string soundName, glm::vec3 pos, AudioChannel channel = AudioChannel::NUM_BUSES);
    //Plays audio with no panning, intended for BGM and other background noise
    int TryPlaySoundStreamBackground(std::string soundName);

    int TryPlaySoundFromEntity(flecs::entity entity);
    int TryPlaySoundFromComponent(AudioEmitterComponent& component);

    void SetGlobalVolume(float volume);
    float GetGlobalVolume();

    //Returns true if handle exists in audio engine
    bool IsValidHandle(int handle);
    //Returns true if handle exists for sound asset
    bool IsValidHandleForSource(std::string source, int handle);

    bool TryStopAudioSource(int handle = -1);
    bool TryStopAudioSource(std::string name, int handle = -1);
    bool TryStopAudioSource(AudioAsset* source, int handle = -1);
    void StopAllAudioFromSource(AudioAsset* source);
    void StopAllAudio();
    void RelinkBuses();

    void StopBGM();

    //Be careful with this thing, its 0.1MB by itself
    const SL::Soloud* GetSoundEngine();

    void SelectSoundAsset(AudioAsset* asset);

    void SetListenerTransform(TransformComponent& transform);
    void SetListenerPosition(glm::vec3 pos);

    void DrawAudioMixerInWindow();

private:

    void SetupMixerSettings();
    void SetupComponents();
    void SetupObservers();
    void SetupQueries();
    void SetupSystems();

    void RegisterWithEditor();

    void DrawSoundAssetEditor(bool& open);
    void DrawAudioEmitterInspector(flecs::entity entity, AudioEmitterComponent& component);
    bool DrawPlaybackSettings(PlaybackSettings& settings);
    bool UpdateSoundAfterEdit(int& handle, const std::string& audioAssetName, PlaybackSettings settings);

    flecs::world& m_world;
    std::unique_ptr<SL::Soloud> m_audioEngine;

    flecs::system m_cleanupEmptyHandlesSystem;
    flecs::system m_update3DAudioSystem;
    flecs::observer m_audioEmitterSetObserver;
    flecs::observer m_audioEmitterRemoveObserver;

    AudioAsset* m_selectedAsset = nullptr;

    /*Hold onto a bunch of audio handles, so we can pause or edit audio clips that are running.
      Holds onto the handles created from playing the sound asset
      map<Asset Name, vector<handles for the asset>>  
    */
    std::unordered_map<AssetHandle, std::vector<int>> m_handles;
    std::vector<AudioEmitterComponent> m_emitterEntities; //Enitity ID and handle attached to component
    
    SL::Bus m_masterBus;
    SL::Bus m_BGMBus;
    SL::Bus m_SFXBus;
    SL::Bus m_ambientBus;

    void Shutdown();
};