#pragma once
#include <cstdint>
#include <glm/glm.hpp>

struct ivec3Hash {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        std::size_t h1 = std::hash<int>{}(v.x);
        std::size_t h2 = std::hash<int>{}(v.y);
        std::size_t h3 = std::hash<int>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

enum class PhysicsBodyType : uint8_t 
{
    STANDARD = 0,
    VOXEL_OBJECT = 1,
    BRICK_STATIC = 2,
    CHARACTER = 3
};

enum class BodyDataSlot : uint8_t
{
    FIRST_U8 = 0,
    SECOND_U8 = 1,
    THIRD_U8 = 2,
    FOURTH_U8 = 3,

    FIRST_U16 = 4,
    SECOND_U16 = 5,

    FIRST_U32 = 6
};

enum class VoxelPhysicsType : uint8_t {
    INTERIOR = 0,
    FACE = 1,
    CORNER = 2,
    UNKNOWN = 255
};

class PhysicsUserData 
{
public:

    //Broken functions - dont use
    static uint64_t EncodeBodyData(uint32_t data, BodyDataSlot slot, uint32_t entityId)
    {
        switch (slot)
        {
        case BodyDataSlot::FIRST_U8:
            return (static_cast<uint64_t>(data) & 0xFF) | ((entityId & 0xFFFFFFFFFFFFFFFF) << 32);
        case BodyDataSlot::SECOND_U8:
            return (static_cast<uint64_t>(data) & 0xFFFF) << 8 | ((entityId & 0xFFFFFFFFFFFFFFFF) << 32);
        case BodyDataSlot::THIRD_U8:
            return (static_cast<uint64_t>(data) & 0xFFFFFFFF) << 16| ((entityId & 0xFFFFFFFFFFFFFFFF) << 32);
        case BodyDataSlot::FOURTH_U8:
            return (static_cast<uint64_t>(data) & 0xFFFFFFFF) << 24 | ((entityId & 0xFFFFFFFFFFFFFFFF) << 32);
        case BodyDataSlot::FIRST_U16:
            return (static_cast<uint64_t>(data) & 0xFFFFFFFF) | ((entityId & 0xFFFFFFFFFFFFFFFF) << 32);
        case BodyDataSlot::SECOND_U16:
            return (static_cast<uint64_t>(data) & 0xFFFFFFFF) << 16| ((entityId & 0xFFFFFFFFFFFFFFFF) << 32);
        default:
            return (static_cast<uint64_t>(data) & 0xFFFFFFFFFFFFFFFF) | ((entityId & 0xFFFFFFFFFFFFFFFF) << 32);
        }
    }

    static uint32_t GetBodyData(uint64_t userData, BodyDataSlot slot)
    {
        switch (slot) {
        case BodyDataSlot::FIRST_U8:
        case BodyDataSlot::SECOND_U8:
        case BodyDataSlot::THIRD_U8:
        case BodyDataSlot::FOURTH_U8:
            return (uint8_t(userData >> (int)slot * 8));
        case BodyDataSlot::FIRST_U16:
            return (uint16_t(userData));
        case BodyDataSlot::SECOND_U16:
            return (uint16_t(userData >> 16));
        case BodyDataSlot::FIRST_U32:
            return (uint32_t(userData));
        }
    }

    static uint64_t GetEntityId(uint64_t userData) {
        return (userData >> 32) & 0xFFFFFFFFFFFFFFFF;
    }

};

class VoxelSubShapeData 
{
public:
    static uint64_t PackCoordinate(const glm::ivec3& coord) {
        const int32_t OFFSET = 1048575; 
        
        uint64_t x = static_cast<uint64_t>(coord.x + OFFSET) & 0x1FFFFF; 
        uint64_t y = static_cast<uint64_t>(coord.y + OFFSET) & 0x1FFFFF;  
        uint64_t z = static_cast<uint64_t>(coord.z + OFFSET) & 0x1FFFFF; 
        
        return (x << 42) | (y << 21) | z;
    }
    
    static glm::ivec3 UnpackCoordinate(uint64_t packed) {
        const int32_t OFFSET = 1048575;
        
        int32_t x = static_cast<int32_t>((packed >> 42) & 0x1FFFFF) - OFFSET;
        int32_t y = static_cast<int32_t>((packed >> 21) & 0x1FFFFF) - OFFSET;
        int32_t z = static_cast<int32_t>(packed & 0x1FFFFF) - OFFSET;
        
        return glm::ivec3(x, y, z);
    }
};