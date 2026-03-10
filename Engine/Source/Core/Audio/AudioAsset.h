#pragma once
#include <soloud_wav.h>
#include <soloud_wavstream.h>

namespace SL = SoLoud;

enum class AttenuationModel
{
	NO_ATTENUATION = 0,
	INVERSE_DISTANCE,
	LINEAR_DISTANCE,
	EXPONENTIAL_DISTANCE
};

enum class AudioChannel
{
	MASTER,
	MUSIC,
	EFFECTS,
	AMBIENT,
	NUM_BUSES
};

enum class SoundType
{
	WAVE,
	STREAM
};

struct PlaybackSettings
{
	SoundType type = SoundType::STREAM;
	AudioChannel bus = AudioChannel::MASTER;
	//Volume relative to bus
	float relativeVolume = -1.f;
	float relativePlaySpeed = 1.f;
	float pan = 0.f;
	bool loop = false;
	bool paused = false;
	//3D audio settings
	bool use3D = false;
	glm::vec3 position = glm::vec3(0);
	glm::vec3 vector = glm::vec3(0);
	float minDistance = 1.f;
	float maxDistance = 500.f;
	float dopplerFactor = 1.f;
	AttenuationModel attenuationModel = AttenuationModel::LINEAR_DISTANCE;
	float attenuationFactor = 1.f;
};

struct AudioAsset
{
	std::string name;
	PlaybackSettings settings;
};

struct SoundWave : public AudioAsset
{
	SL::Wav data;
};

struct SoundStream : public AudioAsset
{
	SL::WavStream data;
};

struct AudioEmitterComponent
{
	std::string audioName;
	PlaybackSettings settings;
	int handle = -1; //Runtime handle to a playing sound
	uint32_t owningEntity = 0;
};