#pragma once

struct RopeSegment;

struct InputComponent
{
	bool useRawInput = true;
	float rotationSpeed = 30.f;
	float disconnectHoldTime = 0.1f;

	glm::vec2 movementInput = glm::vec2{ 0,0 };
	glm::vec2 rotationInput = glm::vec2{ 0,0 };
	bool jumpPressed = false;
	bool dashPressed = false;
	
	int iFramesLeft = 0;
	bool hasIFrames = false;
	float respawnGracePeriod = 0.f;

	bool isHoldingItem = false;
	bool shouldReleaseItem = false;

	int socketNumber = -1;
	bool inputEnabled = false;

	RopeSegment* segmentHeld = nullptr;
	bool holdingRopeEnd = false;

	bool iterateRope = true;
	int currentRopeSegment = 0;
	float slipTime = 0.025f;
	float ropeIterateAccum = 0.0f;

	bool isLoadingHarpoon = false;
	bool isHarpoonLoaded = false;

	bool firePressed = false;
	bool isPullingRope = false;
	bool isHoldingDisconnect = false;
	float disconnectHoldAccum = 0.0f;

	bool isPhysicsOverridden = false;

	void ResetValues()
	{
		jumpPressed = false;
		dashPressed = false;
		hasIFrames = false;
		shouldReleaseItem = false;
		isHoldingItem = false;
		isHoldingDisconnect = false;
		segmentHeld = nullptr;
		holdingRopeEnd = false;
		currentRopeSegment = false;
		ropeIterateAccum = 0.f;
		iterateRope = false;
		isLoadingHarpoon = false;
		isHarpoonLoaded = false;
		firePressed = false;
		isPullingRope = false;
		disconnectHoldAccum = 0.f;
	}
};