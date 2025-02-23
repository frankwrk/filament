/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TNT_UTILS_RENDERPASS_H
#define TNT_UTILS_RENDERPASS_H

#include <filament/Viewport.h>

#include "details/Camera.h"
#include "details/Material.h"
#include "details/Scene.h"

#include "driver/DriverApiForward.h"

#include <private/filament/Variant.h>

#include <utils/compiler.h>
#include <utils/Slice.h>

namespace utils {
class JobSystem;
}

namespace filament {
namespace details {

class RenderPass {
public:
    static constexpr uint64_t DISTANCE_BITS_MASK            = 0xFFFFFFFFllu;
    static constexpr int DISTANCE_BITS_SHIFT                = 0;

    static constexpr uint64_t BLEND_ORDER_MASK              = 0xFFFEllu;
    static constexpr int BLEND_ORDER_SHIFT                  = 1;

    static constexpr uint64_t BLEND_TWO_PASS_MASK           = 0x1llu;
    static constexpr int BLEND_TWO_PASS_SHIFT               = 0;

    static constexpr uint64_t MATERIAL_INSTANCE_ID_MASK     = 0x0000FFFFllu;
    static constexpr int MATERIAL_INSTANCE_ID_SHIFT         = 0;

    static constexpr uint64_t MATERIAL_VARIANT_KEY_MASK     = 0x001F0000llu;
    static constexpr int MATERIAL_VARIANT_KEY_SHIFT         = 16;

    static constexpr uint64_t MATERIAL_ID_MASK              = 0xFFE00000llu;
    static constexpr int MATERIAL_ID_SHIFT                  = 21;

    static constexpr uint64_t BLEND_DISTANCE_MASK           = 0xFFFFFFFF0000llu;
    static constexpr int BLEND_DISTANCE_SHIFT               = 16;

    static constexpr uint64_t MATERIAL_MASK                 = 0xFFFFFFFFllu;
    static constexpr int MATERIAL_SHIFT                     = 0;

    static constexpr uint64_t Z_BUCKET_MASK                 = 0x3FF00000000llu;
    static constexpr int Z_BUCKET_SHIFT                     = 32;

    static constexpr uint64_t PRIORITY_MASK                 = 0x001C000000000000llu;
    static constexpr int PRIORITY_SHIFT                     = 50;

    static constexpr uint64_t BLENDING_MASK                 = 0x00E0000000000000llu;
    static constexpr int BLENDING_SHIFT                     = 53;

    static constexpr uint64_t PASS_MASK                     = 0xFF00000000000000llu;
    static constexpr int PASS_SHIFT                         = 56;


    enum class Pass : uint64_t {    // 8-bits max
        DEPTH    = 0llu << PASS_SHIFT,
        COLOR    = 1llu << PASS_SHIFT,
        BLENDED  = 2llu << PASS_SHIFT,
        SENTINEL = 0xffffffffffffffffllu
    };

    enum CommandTypeFlags : uint8_t {
        COLOR  = 0x1, // generate the color pass
        DEPTH  = 0x2, // generate the depth pass
        SHADOW = 0x4, // generate the shadow-map pass
        DEPTH_AND_COLOR = DEPTH | COLOR, // generate both depth and color pass
    };

    // Command key encoding
    // --------------------
    //
    // a     = alpha masking
    // bbb   = blending
    // ppp   = priority
    // t     = two-pass transparency ordering
    // 0     = reserved, must be zero
    //
    // DEPTH command
    // |    8   | 3 | 3 | 2|       16       |               32               |
    // +--------+---+---+--+----------------+--------------------------------+
    // |00000000|000|ppp|00|0000000000000000|          distanceBits          |
    // +--------+---+---+-------------------+--------------------------------+
    // | correctness    |     optimizations (truncation allowed)             |
    //
    //
    // COLOR command (with depth prepass)
    // |    8   | 3 | 3 | 2|       16       |               32               |
    // +--------+---+---+--+----------------+--------------------------------+
    // |00000001|00a|ppp|00|0000000000000000|          material-id           |
    // +--------+---+---+--+----------------+--------------------------------+
    // | correctness    |        optimizations (truncation allowed)          |
    //
    //
    // COLOR command (without depth prepass)
    // |    8   | 3 | 3 | 2|  6   |   10     |               32               |
    // +--------+---+---+--+------+----------+--------------------------------+
    // |00000001|00a|ppp|00|000000| Z-bucket |          material-id           |
    // +--------+---+---+--+------+----------+--------------------------------+
    // | correctness    |      optimizations (truncation allowed)             |
    //
    //
    // BLENDED command
    // |    8   | 3 | 3 | 2|              32                |         15    |1|
    // +--------+---+---+--+--------------------------------+---------------+-+
    // |00000010|bbb|ppp|00|         ~distanceBits          |   blendOrder  |t|
    // +--------+---+---+--+--------------------------------+---------------+-+
    // | correctness                                                          |
    //
    //
    // SENTINEL command
    // |                                   64                                  |
    // +--------.--------.--------.--------.--------.--------.--------.--------+
    // |11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111111|
    // +-----------------------------------------------------------------------+
    //
    using CommandKey = uint64_t;


    // The sorting material key is 32 bits and encoded as:
    //
    // |    11     |  5  |       16       |
    // +-----------+-----+----------------+
    // | material  | var |   instance     |
    // +-----------+-----+----------------+
    //
    // The variant is inserted while building the commands, because we don't know it before that
    //
    static CommandKey makeMaterialSortingKey(uint32_t materialId, uint32_t instanceId) noexcept {
        CommandKey key = ((materialId << MATERIAL_ID_SHIFT) & MATERIAL_ID_MASK) |
                         ((instanceId << MATERIAL_INSTANCE_ID_SHIFT) & MATERIAL_INSTANCE_ID_MASK);
        return (key << MATERIAL_SHIFT) & MATERIAL_MASK;
    }

    template<typename T>
    static CommandKey makeField(T value, uint64_t mask, int shift) noexcept {
        assert(!((uint64_t(value) << shift) & ~mask));
        return uint64_t(value) << shift;
    }

    template<typename T>
    static CommandKey makeFieldTruncate(T value, uint64_t mask, int shift) noexcept {
        return (uint64_t(value) << shift) & mask;
    }


    template<typename T>
    static CommandKey select(T boolish) noexcept {
        return boolish ? -1llu : 0llu;
    }

    struct PrimitiveInfo { // 24 bytes
        FMaterialInstance const* mi = nullptr;              // 8 bytes (4)
        Handle<HwRenderPrimitive> primitiveHandle;          // 4 bytes
        Handle<HwUniformBuffer> perRenderableBones;         // 4 bytes
        Driver::RasterState rasterState;                    // 4 bytes
        uint16_t index = 0;                                 // 2 bytes
        Variant materialVariant;                            // 1 byte
        uint8_t reserved = { };                             // 1 byte
    };

    struct alignas(8) Command {     // 32 bytes
        CommandKey key = 0;         //  8 bytes
        PrimitiveInfo primitive;    // 24 bytes
        bool operator < (Command const& rhs) const noexcept { return key < rhs.key; }
        // placement new declared as "throw" to avoid the compiler's null-check
        inline void* operator new (std::size_t size, void* ptr) {
            assert(ptr);
            return ptr;
        }
    };
    static_assert(std::is_trivially_destructible<Command>::value,
            "Command isn't trivially destructible");


    using RenderFlags = uint8_t;
    static constexpr RenderFlags HAS_SHADOWING           = 0x01;
    static constexpr RenderFlags HAS_DIRECTIONAL_LIGHT   = 0x02;
    static constexpr RenderFlags HAS_DYNAMIC_LIGHTING    = 0x04;
    static constexpr RenderFlags HAS_INVERSE_FRONT_FACES = 0x08;

    explicit RenderPass(const char* name) noexcept : mName(name) { }

    virtual ~RenderPass() noexcept;

    // appends rendering commands for the given view
    void render(
            FEngine& engine, utils::JobSystem& js,
            FScene& scene, utils::Range<uint32_t> visibleRenderables,
            uint32_t commandTypeFlags, RenderFlags renderFlags,
            const CameraInfo& camera, Viewport const& viewport,
            utils::GrowingSlice<Command>& commands) noexcept;

private:
    // Called just before rendering, make sure all needed asynchronous tasks are finished.
    // Set-up the render-target as needed. At least call driver.beginRenderPass().
    virtual void beginRenderPass(
            driver::DriverApi& driver, Viewport const& viewport,
            const CameraInfo& camera) noexcept = 0;

    // Called just after rendering. Do what you have to do,
    // but at least call driver.endRenderPass().
    virtual void endRenderPass(driver::DriverApi& driver, Viewport const& viewport) noexcept = 0;

private:
    friend class FRenderer;

    // on 64-bits systems, we process batches of 4 (64 bytes) cache-lines, or 8 (32 bytes) commands
    // on 32-bits systems, we process batches of 8 (32 bytes) cache-lines, or 8 (32 bytes) commands
    static constexpr size_t JOBS_PARALLEL_FOR_COMMANDS_COUNT = 16;
    static constexpr size_t JOBS_PARALLEL_FOR_COMMANDS_SIZE  =
            sizeof(Command) * JOBS_PARALLEL_FOR_COMMANDS_COUNT;

    static_assert(JOBS_PARALLEL_FOR_COMMANDS_SIZE % utils::CACHELINE_SIZE == 0,
            "Size of Commands jobs must be multiple of a cache-line size");

    static inline void generateCommands(uint32_t commandTypeFlags, Command* commands,
            FScene::RenderableSoa const& soa, utils::Range<uint32_t> range, RenderFlags renderFlags,
            filament::math::float3 cameraPosition, filament::math::float3 cameraForward) noexcept;

    template<uint32_t commandTypeFlags>
    static inline void generateCommandsImpl(uint32_t, Command* commands, FScene::RenderableSoa const& soa,
            utils::Range<uint32_t> range, RenderFlags renderFlags, filament::math::float3 cameraPosition,
            filament::math::float3 cameraForward) noexcept;

    static void setupColorCommand(Command& cmdDraw, bool hasDepthPass,
            FMaterialInstance const* mi) noexcept;

    static void recordDriverCommands(FEngine::DriverApi& driver, FScene& scene,
            utils::Slice<Command> const& commands) noexcept;

    static void updateSummedPrimitiveCounts(
            FScene::RenderableSoa& renderableData, utils::Range<uint32_t> vr) noexcept;

    const char* const mName;
};

} // namespace details
} // namespace filament

#endif // TNT_UTILS_RENDERPASS_H
