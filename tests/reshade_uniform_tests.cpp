#include <cstdlib>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "runtime/reshade_uniforms.hpp"

using namespace std::chrono_literals;

TEST_CASE("ReShade frame time is expressed in milliseconds")
{
    REQUIRE(vkShade::reshade_frame_time(16ms) == Catch::Approx(16.0f));
}

TEST_CASE("ReShade generated uniforms use the reference defaults")
{
    vkShade::ReshadePingPongState pingPong;
    pingPong.advance(1.0f, pingPong.stepMin);

    // With ReShade's implicit max=1, the minimum 0.05/s increment moves
    // forward instead of immediately bouncing off an implicit max=0.
    REQUIRE(pingPong.value[0] == Catch::Approx(0.05f));
    REQUIRE(pingPong.value[1] >= 0.0f);

    pingPong.stepMin = 0.5f;
    pingPong.stepMax = 1.0f;
    REQUIRE(pingPong.next_step(2) == Catch::Approx(1.0f));

    const vkShade::ReshadeRandomRange random;
    REQUIRE(random.min == 0);
    REQUIRE(random.max == RAND_MAX);
    REQUIRE(random.value(42) == 42);

    REQUIRE(vkShade::reshade_uniform_uses_initializer(""));
    REQUIRE_FALSE(vkShade::reshade_uniform_uses_initializer("addon_defined"));
}

TEST_CASE("ReShade key input modes")
{
    vkShade::ReshadeKeyState state;
    state.mode = vkShade::ReshadeInputMode::Down;
    REQUIRE(state.evaluate(true, false));
    REQUIRE_FALSE(state.evaluate(false, false));

    state.mode = vkShade::ReshadeInputMode::Pressed;
    REQUIRE(state.evaluate(true, true));
    REQUIRE_FALSE(state.evaluate(true, false));

    state.mode = vkShade::ReshadeInputMode::Toggle;
    REQUIRE(state.evaluate(false, true));
    REQUIRE(state.evaluate(false, false));
    REQUIRE_FALSE(state.evaluate(false, true));
}

TEST_CASE("ReShade mouse wheel clamp")
{
    vkShade::ReshadeMouseWheelState wheel;
    wheel.min = 0.0f;
    wheel.max = 1.0f;
    wheel.step = 0.25f;
    REQUIRE(wheel.advance(1.0f) == Catch::Approx(0.25f));
    REQUIRE(wheel.advance(10.0f) == Catch::Approx(1.0f));
}
