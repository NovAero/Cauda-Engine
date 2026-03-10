#pragma once
#include "Grimorium.h"
#include "Core/Editor/ResourceLibrary.h"
//#include "Content/Grimoires/ExampleScript.h"
//#include "Content/Grimoires/TestScript.h"
#include "Content/Grimoires/LootSpawner.h"
#include "Content/Grimoires/LootElevator.h"
#include "Content/Grimoires/Rotator.h"
#include "Content/Grimoires/WorldMouse.h"
#include "Content/Grimoires/Stamina.h"
//#include "Content\Grimoires\JumpBoost.h"
//#include "Content\Grimoires\SpeedBoost.h"
//#include "Content\Grimoires\GiantPowerup.h"
//#include "Content\Grimoires\Powerup.h"
#include "Content\Grimoires\PlayerBehaviour.h"
#include "Content\Grimoires\KillVolumeScript.h"
#include "Content\Grimoires\MovingPlatform.h"
#include "Content/Grimoires/GravityWell.h"
//#include "Content/Grimoires/LogScript.h"
//#include "Content/Grimoires/LogScriptKinematic.h"
//#include "Content\Grimoires\CannonScript.h"
//#include "Content/Grimoires/ProjectileBehaviour.h"
#include "Content/Grimoires/Projectile.h"
#include "Content/Grimoires/ProjectileLauncher.h"
#include "Content/Grimoires/DiegeticButton.h"
#include "Content/Grimoires/PlayerScoring.h"
#include "Content/Grimoires/FallingTileBehaviour.h"
#include "Content/Grimoires/BumperBehaviour.h"
//#include "Content/Grimoires/ChickenBehaviour.h"
#include "Content/Grimoires/Attractor.h"
#include "Content/Grimoires/AimAssist.h"
#include "Content/Grimoires/AimTarget.h"
#include "Content/Grimoires/Dash.h"
#include "Content/Grimoires/AmmoPickup.h"
#include "Content/Grimoires/HitFX.h"
#include "Content/Grimoires/Health.h"
#include "Content/Grimoires/SafeZone.h"
#include "Content/Grimoires/RunningSmoke.h"
#include "Content/Grimoires/CameraTarget.h"
#include "Content/Grimoires/BreakableObject.h"
#include "Content/Grimoires/IntroCutscene.h"
#include "Content/Grimoires/DashFX.h"
#include "Content/Grimoires/DeathFX.h"

namespace Grimorium
{
	void InitialiseGrimoires(flecs::world& world)
	{
		Attractor::Register(world);
		AimAssist::Register(world);
		AimTarget::Register(world);
		AmmoPickup::Register(world);
		BumperBehaviour::Register(world);
		CameraTarget::Register(world);
		Dash::Register(world);
		FallingTileBehaviour::Register(world);
		GravityWell::Register(world);
		HitFX::Register(world);
		Health::Register(world);
		KillVolumeBehaviour::Register(world);
		LootSpawner::Register(world);
		LootElevator::Register(world);
		MovingPlatform::Register(world);
		PlayerScoring::Register(world);
		PlayerBehaviour::Register(world);
		ProjectileLauncher::Register(world);
		Projectile::Register(world);
		RunningSmoke::Register(world);
		Rotator::Register(world);
		Stamina::Register(world);
		SafeZone::Register(world);
		WorldMouse::Register(world);
		BreakableObject::Register(world);
		IntroCutscene::Register(world);
	}

	void BurnTheLibrary() {}
}