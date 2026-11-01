#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vkShade
{
    /// Application-specific compatibility hooks.
    /// Implementations must not claim support that has not been runtime-tested.
    class ApplicationProfile
    {
    public:
        virtual ~ApplicationProfile() = default;

        virtual std::string_view name() const = 0;

        /// Return true only when this profile positively identifies the host process.
        virtual bool detect() = 0;

        /// Prepare profile-specific options. Must be safe to call after a successful detect().
        virtual bool initialize() = 0;

        virtual void shutdown() = 0;
    };

    class GenericVulkanProfile final : public ApplicationProfile
    {
    public:
        std::string_view name() const override { return "GenericVulkan"; }
        bool detect() override { return true; }
        bool initialize() override { return true; }
        void shutdown() override {}
    };

    /// Second Life / Firestorm — Vulkan layer path.
    /// detect() is always false: Firestorm world rendering is OpenGL.
    /// Use the OpenGL injector (opengl32.dll / Block64.dll) for Firestorm;
    /// see profiles/SecondLife/README.md.
    class SecondLifeProfile final : public ApplicationProfile
    {
    public:
        std::string_view name() const override { return "SecondLife"; }
        bool detect() override { return false; }
        bool initialize() override { return false; }
        void shutdown() override {}
    };

    class CustomProfile final : public ApplicationProfile
    {
    public:
        explicit CustomProfile(std::string profileName)
            : m_name(std::move(profileName))
        {}

        std::string_view name() const override { return m_name; }
        bool detect() override { return false; }
        bool initialize() override { return false; }
        void shutdown() override {}

    private:
        std::string m_name;
    };

    class ProfileRegistry
    {
    public:
        ProfileRegistry();

        ApplicationProfile& active() { return *m_active; }
        const ApplicationProfile& active() const { return *m_active; }

        /// Selects the first profile whose detect() returns true.
        /// Falls back to GenericVulkan. Never fabricates Second Life support.
        void resolve();

    private:
        std::vector<std::unique_ptr<ApplicationProfile>> m_profiles;
        ApplicationProfile* m_active {nullptr};
    };
} // namespace vkShade
