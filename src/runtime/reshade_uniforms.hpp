#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <string_view>

namespace reshadefx
{
    struct uniform;
}

namespace vkShade
{
    class VulkanBuffer;

    inline float reshade_frame_time(std::chrono::steady_clock::duration duration)
    {
        return std::chrono::duration<float, std::milli>(duration).count();
    }

    enum class ReshadeInputMode : uint8_t
    {
        Down = 0,
        Pressed = 1,
        Toggle = 2,
    };

    struct ReshadeKeyState
    {
        uint32_t keycode {0};
        ReshadeInputMode mode {ReshadeInputMode::Down};
        bool toggled {false};

        bool evaluate(bool down, bool pressed)
        {
            switch (mode)
            {
                case ReshadeInputMode::Pressed:
                    return pressed;
                case ReshadeInputMode::Toggle:
                    if (pressed)
                        toggled = !toggled;
                    return toggled;
                case ReshadeInputMode::Down:
                default:
                    return down;
            }
        }
    };

    struct ReshadeMouseWheelState
    {
        float min {0.0f};
        float max {0.0f};
        float step {1.0f};
        float value {0.0f};

        float advance(float delta)
        {
            value += delta * step;
            if (min != max)
                value = std::clamp(value, min, max);
            return value;
        }
    };

    struct ReshadeFrameState
    {
        float frameTime {0.0f};
        uint32_t frameCount {0};
        float timer {0.0f};

        std::array<float, 2> mousePoint {0.0f, 0.0f};
        std::array<float, 2> mouseDelta {0.0f, 0.0f};
        float mouseWheel {0.0f};
        std::array<bool, 5> mouseButtonsDown {};
        std::array<bool, 5> mouseButtonsPressed {};
        std::bitset<256> keysDown {};
        std::bitset<256> keysPressed {};

        bool overlayOpen {false};
        uint32_t overlayActive {0};
        uint32_t overlayHovered {0};
        bool hasDepth {false};
        bool screenshot {false};
    };

    struct ReshadePingPongState
    {
        float min {0.0f};
        float max {1.0f};
        float stepMin {0.0f};
        float stepMax {0.0f};
        float smoothing {0.0f};
        std::array<float, 2> value {0.0f, 0.0f};

        float next_step(int randomValue) const
        {
            return stepMax == 0.0f
                ? stepMin
                : stepMin + std::fmod(
                    static_cast<float>(randomValue), stepMax - stepMin + 1.0f);
        }

        void advance(float deltaTime, float step)
        {
            if (value[1] >= 0.0f)
            {
                const float smooth = std::max(0.0f, smoothing - (max - value[0]));
                value[0] += std::max(step - smooth, 0.05f) * deltaTime;
                if (value[0] >= max)
                {
                    value[0] = max;
                    value[1] = -1.0f;
                }
            }
            else
            {
                const float smooth = std::max(0.0f, smoothing - (value[0] - min));
                value[0] -= std::max(step - smooth, 0.05f) * deltaTime;
                if (value[0] <= min)
                {
                    value[0] = min;
                    value[1] = 1.0f;
                }
            }
        }
    };

    struct ReshadeRandomRange
    {
        int32_t min {0};
        int32_t max {RAND_MAX};

        int32_t value(uint32_t randomValue) const
        {
            const uint64_t width = static_cast<uint64_t>(
                std::abs(static_cast<int64_t>(max) - min)) + 1;
            return static_cast<int32_t>(
                static_cast<int64_t>(min) + static_cast<int64_t>(randomValue % width));
        }
    };

    // Base class for a ReShade built-in uniform. These require special handling.
    class ReshadeUniform
    {
    public:
        virtual ~ReshadeUniform() = default;
        virtual void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) = 0;

    protected:
        ReshadeUniform() = default;

        uint32_t m_size {0};
        uint32_t m_offset {0};
    };

    class FrameTimeUniform : public ReshadeUniform
    {
    public:
        FrameTimeUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class FrameCountUniform : public ReshadeUniform
    {
    public:
        FrameCountUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class DateUniform : public ReshadeUniform
    {
    public:
        DateUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class TimerUniform : public ReshadeUniform
    {
    public:
        TimerUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class PingPongUniform : public ReshadeUniform
    {
    public:
        PingPongUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;

    private:
        ReshadePingPongState m_state;
    };

    class RandomUniform : public ReshadeUniform
    {
    public:
        RandomUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;

    private:
        ReshadeRandomRange m_range;
    };

    class KeyUniform : public ReshadeUniform
    {
    public:
        KeyUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;

    private:
        ReshadeKeyState m_state;
    };

    class MouseButtonUniform : public ReshadeUniform
    {
    public:
        MouseButtonUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;

    private:
        ReshadeKeyState m_state;
    };

    class MousePointUniform : public ReshadeUniform
    {
    public:
        MousePointUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class MouseDeltaUniform : public ReshadeUniform
    {
    public:
        MouseDeltaUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class MouseWheelUniform : public ReshadeUniform
    {
    public:
        MouseWheelUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;

    private:
        ReshadeMouseWheelState m_state;
    };

    class DepthUniform : public ReshadeUniform
    {
    public:
        DepthUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class OverlayOpenUniform : public ReshadeUniform
    {
    public:
        OverlayOpenUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class OverlayActiveUniform : public ReshadeUniform
    {
    public:
        OverlayActiveUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class OverlayHoveredUniform : public ReshadeUniform
    {
    public:
        OverlayHoveredUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    class ScreenshotUniform : public ReshadeUniform
    {
    public:
        ScreenshotUniform(reshadefx::uniform uniform);
        void update(VulkanBuffer& buffer, const ReshadeFrameState& frame) override;
    };

    inline bool reshade_uniform_uses_initializer(std::string_view source)
    {
        return source.empty();
    }
} // namespace vkShade
