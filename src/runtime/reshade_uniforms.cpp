#include "reshade_uniforms.hpp"

#include <chrono>
#include <cstring>

#include <effect_module.hpp>
#include <glm/vec2.hpp>

#include "vk/buffer.hpp"

namespace
{
    bool uniform_has_source(const reshadefx::uniform& uniform, const std::string& source)
    {
        auto it = std::find_if(uniform.annotations.begin(), uniform.annotations.end(), [](const auto& a)
        {
            return a.name == "source";
        });

        return it != uniform.annotations.end() && it->value.string_data == source;
    }

    float annotation_as_float(const reshadefx::annotation& a, int idx = 0)
    {
        return a.type.is_floating_point() ? a.value.as_float[idx]
                                          : static_cast<float>(a.value.as_int[idx]);
    }

    int annotation_as_int(const reshadefx::annotation& a, int idx = 0)
    {
        return a.type.is_integral() ? a.value.as_int[idx]
                                    : static_cast<int>(a.value.as_float[idx]);
    }

    vkShade::ReshadeInputMode parse_input_mode(const reshadefx::uniform& uniform)
    {
        bool toggle = false;
        for (const auto& a : uniform.annotations)
        {
            if (a.name == "toggle" && annotation_as_int(a) != 0)
                toggle = true;
            else if (a.name == "mode")
            {
                const auto& mode = a.value.string_data;
                if (mode == "press")
                    return vkShade::ReshadeInputMode::Pressed;
                if (mode == "toggle")
                    return vkShade::ReshadeInputMode::Toggle;
                return vkShade::ReshadeInputMode::Down;
            }
        }
        return toggle ? vkShade::ReshadeInputMode::Toggle : vkShade::ReshadeInputMode::Down;
    }

    void write_bool_value(vkShade::VulkanBuffer& buffer, uint32_t offset, uint32_t size, bool value)
    {
        if (size >= sizeof(uint32_t))
        {
            const VkBool32 v = value ? VK_TRUE : VK_FALSE;
            buffer.write(&v, sizeof(v), offset);
        }
        else if (size >= sizeof(uint8_t))
        {
            const uint8_t v = value ? 1u : 0u;
            buffer.write(&v, sizeof(v), offset);
        }
    }
} // namespace

vkShade::FrameTimeUniform::FrameTimeUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "frametime"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::FrameTimeUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    buffer.write(&frame.frameTime, m_size, m_offset);
}

vkShade::FrameCountUniform::FrameCountUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "framecount"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::FrameCountUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    if (m_size == sizeof(VkBool32) || m_size == sizeof(bool))
    {
        const bool even = (frame.frameCount % 2) == 0;
        write_bool_value(buffer, m_offset, m_size, even);
        return;
    }

    buffer.write(&frame.frameCount, m_size, m_offset);
}

vkShade::DateUniform::DateUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "date"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::DateUniform::update(VulkanBuffer& buffer, const ReshadeFrameState&)
{
    auto        now         = std::chrono::system_clock::now();
    std::time_t nowC        = std::chrono::system_clock::to_time_t(now);
    struct tm*  currentTime = std::localtime(&nowC);
    float       year        = 1900.0f + static_cast<float>(currentTime->tm_year);
    float       month       = 1.0f + static_cast<float>(currentTime->tm_mon);
    float       day         = static_cast<float>(currentTime->tm_mday);
    float       seconds     = static_cast<float>((currentTime->tm_hour * 60 + currentTime->tm_min) * 60 + currentTime->tm_sec);
    float       date[]      = {year, month, day, seconds};

    buffer.write(date, m_size, m_offset);
}

vkShade::TimerUniform::TimerUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "timer"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::TimerUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    buffer.write(&frame.timer, m_size, m_offset);
}

vkShade::PingPongUniform::PingPongUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "pingpong"));

    for (const auto& a : uniform.annotations)
    {
        if      (a.name == "min")       m_state.min       = annotation_as_float(a);
        else if (a.name == "max")       m_state.max       = annotation_as_float(a);
        else if (a.name == "smoothing") m_state.smoothing = annotation_as_float(a);
        else if (a.name == "step")
        {
            m_state.stepMin = annotation_as_float(a, 0);
            m_state.stepMax = annotation_as_float(a, 1);
        }
    }

    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::PingPongUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    m_state.advance(frame.frameTime * 0.001f, m_state.next_step(std::rand()));
    buffer.write(m_state.value.data(), m_size, m_offset);
}

vkShade::RandomUniform::RandomUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "random"));

    for (const auto& a : uniform.annotations)
    {
        if      (a.name == "min") m_range.min = annotation_as_int(a);
        else if (a.name == "max") m_range.max = annotation_as_int(a);
    }

    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::RandomUniform::update(VulkanBuffer& buffer, const ReshadeFrameState&)
{
    const int32_t value = m_range.value(static_cast<uint32_t>(std::rand()));
    buffer.write(&value, m_size, m_offset);
}

vkShade::KeyUniform::KeyUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "key"));

    for (const auto& a : uniform.annotations)
    {
        if (a.name == "keycode")
            m_state.keycode = static_cast<uint32_t>(annotation_as_int(a));
    }
    m_state.mode = parse_input_mode(uniform);

    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::KeyUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    bool pressed = false;
    if (m_state.keycode > 7 && m_state.keycode < 256)
    {
        pressed = m_state.evaluate(
            frame.keysDown.test(m_state.keycode),
            frame.keysPressed.test(m_state.keycode));
    }
    write_bool_value(buffer, m_offset, m_size, pressed);
}

vkShade::MouseButtonUniform::MouseButtonUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "mousebutton"));

    for (const auto& a : uniform.annotations)
    {
        if (a.name == "keycode")
            m_state.keycode = static_cast<uint32_t>(annotation_as_int(a));
    }
    m_state.mode = parse_input_mode(uniform);

    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::MouseButtonUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    bool pressed = false;
    if (m_state.keycode < frame.mouseButtonsDown.size())
    {
        pressed = m_state.evaluate(
            frame.mouseButtonsDown[m_state.keycode],
            frame.mouseButtonsPressed[m_state.keycode]);
    }
    write_bool_value(buffer, m_offset, m_size, pressed);
}

vkShade::MousePointUniform::MousePointUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "mousepoint"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::MousePointUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    buffer.write(frame.mousePoint.data(), m_size, m_offset);
}

vkShade::MouseDeltaUniform::MouseDeltaUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "mousedelta"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::MouseDeltaUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    buffer.write(frame.mouseDelta.data(), m_size, m_offset);
}

vkShade::MouseWheelUniform::MouseWheelUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "mousewheel"));

    for (const auto& a : uniform.annotations)
    {
        if      (a.name == "min")  m_state.min  = annotation_as_float(a);
        else if (a.name == "max")  m_state.max  = annotation_as_float(a);
        else if (a.name == "step") m_state.step = annotation_as_float(a);
    }

    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::MouseWheelUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    const float accumulated = m_state.advance(frame.mouseWheel);
    const float values[2] = {accumulated, frame.mouseWheel};
    buffer.write(values, m_size, m_offset);
}

vkShade::DepthUniform::DepthUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "bufready_depth"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::DepthUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    write_bool_value(buffer, m_offset, m_size, frame.hasDepth);
}

vkShade::OverlayOpenUniform::OverlayOpenUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "overlay_open") ||
           uniform_has_source(uniform, "ui_open"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::OverlayOpenUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    write_bool_value(buffer, m_offset, m_size, frame.overlayOpen);
}

vkShade::OverlayActiveUniform::OverlayActiveUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "overlay_active") ||
           uniform_has_source(uniform, "ui_active"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::OverlayActiveUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    const int32_t value = static_cast<int32_t>(frame.overlayActive);
    buffer.write(&value, m_size, m_offset);
}

vkShade::OverlayHoveredUniform::OverlayHoveredUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "overlay_hovered") ||
           uniform_has_source(uniform, "ui_hovered"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::OverlayHoveredUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    const int32_t value = static_cast<int32_t>(frame.overlayHovered);
    buffer.write(&value, m_size, m_offset);
}

vkShade::ScreenshotUniform::ScreenshotUniform(reshadefx::uniform uniform)
{
    assert(uniform_has_source(uniform, "screenshot"));
    m_offset = uniform.offset;
    m_size   = uniform.size;
}

void vkShade::ScreenshotUniform::update(VulkanBuffer& buffer, const ReshadeFrameState& frame)
{
    write_bool_value(buffer, m_offset, m_size, frame.screenshot);
}
