#pragma once
#include "cepch.h"
#include "JoltPhysics.h"
#include "ThirdParty/Flecs.h"

class PhysicsModule;
struct PhysicsBodyComponent;
struct ColliderComponent;
struct CharacterComponent;
struct RopeSegment;

namespace Cauda
{
    enum class ConstraintType
    {
        NONE = 0,
        PIN = 1,
        DISTANCE = 2
    };

    struct ConstraintSettings
    {
        ConstraintType type;
        std::string entityA = "";
        std::string entityB = "";
        bool worldSpace = false;
        int index = -1;
    };

    struct PinConstraintSettings : public ConstraintSettings
    {
        glm::vec3 mPoint1 = glm::vec3(0);
        glm::vec3 mPoint2 = glm::vec3(0);
    };

    struct DistanceConstraintSettings : public ConstraintSettings
    {
        float minDistance = 1.f;
        float maxDistance = 1.f;
        bool fixed = false;
    };

    struct SpringConstraintSettings : public DistanceConstraintSettings
    {
        float stiffness = 0.f;
        float damping = 0.f;
    };
}

struct DistanceConstraintComponent
{
    DistanceConstraintComponent() = default;
    DistanceConstraintComponent(Cauda::DistanceConstraintSettings initial) {
        m_constraintSettings.push_back(initial);
    }
    std::vector<Cauda::DistanceConstraintSettings> m_constraintSettings;
};

struct PinConstraintComponent
{
    PinConstraintComponent() = default;
    PinConstraintComponent(Cauda::PinConstraintSettings initial) {
        m_constraintSettings.push_back(initial);
    }
    std::vector<Cauda::PinConstraintSettings> m_constraintSettings;
};

struct ConstraintKey
{
    ConstraintKey() = default;
    ConstraintKey(flecs::entity_t A, flecs::entity_t B)
    {
        uint32_t a32 = (uint32_t)A;

        pair = (uint64_t)a32 | B << 32;
    }

    bool operator!=(ConstraintKey other)
    {
        return other.pair !=  this->pair;
    }
    bool operator==(ConstraintKey other)
    {
        return other.pair == this->pair;
    }

    //Checks if the entity ID exists in either half of data
    bool operator[](uint32_t id)
    {
        return (*this >> 0 == id) || (*this >> 1 == id);
    }

    //Not a direct bitshift operator, returns either the first or second
    //half of data to give entityA or B id
    uint32_t operator>>(bool getB)
    {
        if (!getB)
        {
            return (uint32_t)pair;
        }
        else
        {
            return pair >> 32;
        }
    }
private:

    uint64_t pair = 0;
};

struct ConstraintData
{
    JPH::Ref<JPH::Constraint> constraint;

    Cauda::ConstraintSettings settings;
};

struct ConstraintContainer
{
    ConstraintKey key;
    std::vector<ConstraintData> data;
};

struct ReloadConstraints {};

class ConstraintModule
{
    friend class PhysicsModule;
public:
    ConstraintModule(flecs::world& world);
    ~ConstraintModule();

    ConstraintModule(const ConstraintModule&) = delete;
    ConstraintModule& operator=(const ConstraintModule&) = delete;
    ConstraintModule(ConstraintModule&&) = delete;
    ConstraintModule& operator=(ConstraintModule&&) = delete;

    void OnImGuiRender();

    //Creates managed constraint, and adds a DistanceConstraintComponent to entityA
    bool CreateManagedDistanceConstraint(flecs::entity entityA, flecs::entity entityB, float minDistance, float maxDistance);
    //Creates managed constraint, and adds a PinConstraintComponent to entityA
    bool CreateManagedPinConstraint(flecs::entity entityA, flecs::entity entityB, glm::vec3 offsetA, glm::vec3 offsetB);

    //TODO - Fill these out with the new constraint components
    //bool CreateManagedSpringConstraint(flecs::entity entityA, flecs::entity entityB, float minDistance, float maxDistance, float stiffness = 10.f, float damping = 0.f);
    //bool CreateManagedHingeConstraint(flecs::entity entityA, flecs::entity entityB, JPH::Vec3 axis, float max, float min);

    //Removes constraint from simulation, and the settings from container and components
    void RemoveConstraintFromEntity(ConstraintKey key, Cauda::ConstraintSettings settings);
    void RemoveAllConstraintsFromEntity(flecs::entity entity);
    bool TryRemoveConstraintFromEntity(ConstraintKey key, int index);

    JPH::Constraint* CreateDistanceConstraint(JPH::Body* bodyA, JPH::Body* bodyB, Cauda::DistanceConstraintSettings settings = {});
    JPH::Constraint* CreatePinConstraint(JPH::Body* bodyA, JPH::Body* bodyB, Cauda::PinConstraintSettings settings = {});
    JPH::Constraint* CreateSpringConstraint(JPH::Body* bodyA, JPH::Body* bodyB, Cauda::SpringConstraintSettings settings = {});
    
    std::vector<const ConstraintData*> GetConstraintsForEntity(flecs::entity entity);
    std::vector<const JPH::DistanceConstraint*> GetDistanceConstraintsForEntity(flecs::entity entity);
    std::vector<const JPH::PointConstraint*> GetPinConstraintsForEntity(flecs::entity entity);

    void SyncContainer(ConstraintContainer& container);
    void CleanupManagedConstraints();

private:
    
    struct BodyPair
    {
        JPH::Body* bodyA = nullptr;
        JPH::Body* bodyB = nullptr;
    };

    void SetupComponents();
    void SetupObservers();
    void SetupQueries();
    void SetupSystems();

    flecs::world& m_world;
    PhysicsModule* m_physicsModule;

    flecs::observer m_physbodyDeletedCleanupObserver;
    flecs::observer m_characterDeletedCleanupObserver;
    flecs::observer m_pinConstraintAddedObserver;
    flecs::observer m_pinConstraintRemovedObserver;
    flecs::observer m_distanceConstraintAddedObserver;
    flecs::observer m_distanceConstraintRemovedObserver;
    flecs::query<PhysicsBodyComponent, ColliderComponent> m_constrainableEntitiesQuery;
    flecs::query<CharacterComponent> m_characterEntityQuery;
    flecs::query<RopeSegment> m_ropeSegmentEntityQuery;

    //Entity pairs, and the constraints made between them
    std::vector<ConstraintContainer*> m_managedConstraints;

    ConstraintContainer* TryFindContainer(ConstraintKey key, bool createIfNot = false);
    BodyPair GetBodyPair(flecs::entity entityA, flecs::entity entityB);

    Cauda::PinConstraintSettings CreateEmptyPinConstraint(flecs::entity entity);
    Cauda::DistanceConstraintSettings CreateEmptyDistanceConstraint(flecs::entity entity);

    // Internal Managed constraint, for use on set of constraint components
    bool CreateManagedDistanceConstraint(flecs::entity entityA, flecs::entity entityB, Cauda::DistanceConstraintSettings& settings);
    // Internal Managed constraint, for use on set of constraint components
    bool CreateManagedPinConstraint(flecs::entity entityA, flecs::entity entityB, Cauda::PinConstraintSettings& settings);

    JPH::Constraint* CreateConstraint(JPH::Body* bodyA, JPH::Body* bodyB, Cauda::ConstraintSettings* settings);
    void UpdateConstraint(flecs::entity entity, Cauda::ConstraintSettings& settings);
    void PreUpdateConstraint(flecs::entity entityA, flecs::entity previous, Cauda::ConstraintSettings& settings);
    bool RemoveConstraintFromSystem(JPH::Constraint* constrain);

    std::vector<Cauda::ConstraintSettings> GetConstraintsSettingsForEntity(flecs::entity entity);

    void RegisterWithEditor();

    void DrawPinConstraintInspector(flecs::entity entity, PinConstraintComponent& component);
    void DrawDistanceConstraintInspector(flecs::entity entity, DistanceConstraintComponent& component);
};
