
#ifndef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
#define JPH_FLOATING_POINT_EXCEPTIONS_ENABLED 0
#endif
#ifndef JPH_PROFILE_ENABLED
#define JPH_PROFILE_ENABLED 1
#endif
#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER 1
#endif
#ifndef JPH_OBJECT_STREAM
#define JPH_OBJECT_STREAM 1
#endif

// === CORE SYSTEM ===
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/Result.h>

// === MATH ===
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Vec4.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Math/Float4.h>
#include <Jolt/Math/Double3.h>

// === GEOMETRY UTILITIES ===
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Geometry/Sphere.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Geometry/OrientedBox.h>
#include <Jolt/Geometry/Plane.h>
#include <Jolt/Geometry/ConvexHullBuilder.h>
#include <Jolt/Geometry/ClosestPoint.h>

// === PHYSICS SYSTEM ===
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsLock.h>
#include <Jolt/Physics/PhysicsStepListener.h>

// === BODY SYSTEM ===
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Body/MotionType.h>

// === COLLISION SHAPES ===
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCylinderShape.h>
#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/DecoratedShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>

// === COLLISION DETECTION AND QUERIES ===
#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Collision/GroupFilter.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuadTree.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseBruteForce.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>

// === CONTACT AND MATERIAL SYSTEM ===
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/PhysicsMaterialSimple.h>
#include <Jolt/Physics/Collision/EstimateCollisionResponse.h>

// === ALL CONSTRAINTS ===
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/GearConstraint.h>
#include <Jolt/Physics/Constraints/PathConstraint.h>
#include <Jolt/Physics/Constraints/RackAndPinionConstraint.h>
#include <Jolt/Physics/Constraints/SpringSettings.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

// === CHARACTER CONTROLLERS ===
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

// === VEHICLE PHYSICS ===
//#include <Jolt/Physics/Vehicle/VehicleAntiRollBar.h>
//#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
//#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
//#include <Jolt/Physics/Vehicle/VehicleController.h>
//#include <Jolt/Physics/Vehicle/VehicleDifferential.h>
//#include <Jolt/Physics/Vehicle/VehicleEngine.h>
//#include <Jolt/Physics/Vehicle/VehicleTrack.h>
//#include <Jolt/Physics/Vehicle/VehicleTransmission.h>
//#include <Jolt/Physics/Vehicle/Wheel.h>
//#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
//#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
//#include <Jolt/Physics/Vehicle/MotorcycleController.h>

// === RAGDOLL PHYSICS ===
#include <Jolt/Physics/Ragdoll/Ragdoll.h>

// === SOFT BODY PHYSICS ===
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/SoftBody/SoftBodyShape.h>
#include <Jolt/Physics/SoftBody/SoftBodyVertex.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>

// === SKELETON SYSTEM ===
#include <Jolt/Skeleton/Skeleton.h>
#include <Jolt/Skeleton/SkeletonPose.h>
#include <Jolt/Skeleton/SkeletonMapper.h>

// === DEBUG RENDERER ===
#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Renderer/DebugRendererPlayback.h>
#include <Jolt/Renderer/DebugRendererRecorder.h>

// === SERIALIZATION ===
//#include <Jolt/ObjectStream/ObjectStream.h>
//#include <Jolt/ObjectStream/ObjectStreamIn.h>
//#include <Jolt/ObjectStream/ObjectStreamOut.h>
//#include <Jolt/ObjectStream/ObjectStreamBinaryIn.h>
//#include <Jolt/ObjectStream/ObjectStreamBinaryOut.h>
//#include <Jolt/ObjectStream/ObjectStreamTextIn.h>
//#include <Jolt/ObjectStream/ObjectStreamTextOut.h>
//#include <Jolt/ObjectStream/SerializableObject.h>
//#include <Jolt/ObjectStream/TypeDeclarations.h>

