#pragma once

#include "gui_forward_declares.h"
#include "gui_enums.h"
#include "Registry.h"
#include <string>
#include <vector>

namespace lcf::gui {
    struct WindowCreateInfo
    {
        using Self = WindowCreateInfo;
        WindowCreateInfo(
            Registry * registry_p = nullptr,
            uint32_t width = 1280,
            uint32_t height = 720,
            SurfaceType surface_type = SurfaceType::eNone,
            std::string_view title = "") :
            m_registry_p(registry_p),
            m_width(width),
            m_height(height),
            m_surface_type(surface_type),
            m_title(title) { }
        WindowCreateInfo(const Self & other) = delete;
        Self & operator=(const Self & other) = delete;
        WindowCreateInfo(Self && other) = default;
        Self & operator=(Self && other) = default;
        ~WindowCreateInfo() = default;
        Self & setRegistryPtr(Registry * registry_p) noexcept { m_registry_p = registry_p; return *this; }
        Registry * getRegistryPtr() const noexcept { return m_registry_p; }
        Self & setWidth(uint32_t width) noexcept { m_width = width; return *this; }
        uint32_t getWidth() const noexcept { return m_width; }
        Self & setHeight(uint32_t height) noexcept { m_height = height; return *this; }
        uint32_t getHeight() const noexcept { return m_height; }
        Self & setSurfaceType(SurfaceType surface_type) noexcept { m_surface_type = surface_type; return *this; }
        SurfaceType getSurfaceType() const noexcept { return m_surface_type; }
        Self & setTitle(std::string_view title) noexcept { m_title = title; return *this; }
        const std::string & getTitle() const noexcept { return m_title; }

        Registry * m_registry_p;
        uint32_t m_width;
        uint32_t m_height;
        SurfaceType m_surface_type;
        std::string m_title;
    };


    struct DisplayModeInfo
    {
        using Self = DisplayModeInfo;
        DisplayModeInfo(
            uint32_t width,
            uint32_t height,
            double pixel_ratio,
            double refresh_rate) :
            m_width(width),
            m_height(height),
            m_pixel_ratio(pixel_ratio),
            m_refresh_rate(refresh_rate) { }
        DisplayModeInfo(const Self & other) = default;
        Self & operator=(const Self & other) = default;
        DisplayModeInfo(Self && other) = default;
        Self & operator=(Self && other) = default;
        ~DisplayModeInfo() = default;
        uint32_t getLogicalWidth() const noexcept { return m_width; }
        uint32_t getLogicalHeight() const noexcept { return m_height; }
        std::pair<uint32_t, uint32_t> getLogicalExtent() const noexcept { return { this->getLogicalWidth(), this->getLogicalHeight() }; }
        uint32_t getLogicalSize() const noexcept { return this->getLogicalWidth() * this->getLogicalHeight(); }
        uint32_t getRenderWidth() const noexcept { return static_cast<uint32_t>(m_width * m_pixel_ratio); }
        uint32_t getRenderHeight() const noexcept { return static_cast<uint32_t>(m_height * m_pixel_ratio); }
        std::pair<uint32_t, uint32_t> getRenderExtent() const noexcept { return { this->getRenderWidth(), this->getRenderHeight() }; }
        uint32_t getRenderSize() const noexcept { return this->getRenderWidth() * this->getRenderHeight(); }
        double getPixelRatio() const noexcept { return m_pixel_ratio; }
        double getRefreshRate() const noexcept { return m_refresh_rate; }

        uint32_t m_width;
        uint32_t m_height;
        double m_pixel_ratio;
        double m_refresh_rate;
    };

    struct DisplayerInfo
    {
        using Self = DisplayerInfo;
        DisplayerInfo(
            bool is_primary,
            uint32_t index,
            std::string_view name,
            const DisplayModeInfo & desktop_mode_info,
            const DisplayModeInfo & current_mode_info,
            const std::vector<DisplayModeInfo> & available_mode_infos) :
            m_is_primary(is_primary),
            m_index(index),
            m_name(name),
            m_desktop_mode_info(desktop_mode_info),
            m_current_mode_info(current_mode_info),
            m_available_mode_infos(available_mode_infos) { }
        DisplayerInfo(const Self & other) = default;
        Self & operator=(const Self & other) = default;
        DisplayerInfo(Self && other) = default;
        Self & operator=(Self && other) = default;
        ~DisplayerInfo() = default;
        bool isPrimary() const noexcept { return m_is_primary; }
        uint32_t getIndex() const noexcept { return m_index; }
        const std::string & getName() const noexcept { return m_name; }
        const DisplayModeInfo & getDesktopModeInfo() const noexcept { return m_desktop_mode_info; }
        const DisplayModeInfo & getCurrentModeInfo() const noexcept { return m_current_mode_info; }
        const std::vector<DisplayModeInfo> & getAvailableModeInfos() const noexcept { return m_available_mode_infos; }

        bool m_is_primary;
        uint32_t m_index;
        std::string m_name;
        DisplayModeInfo m_desktop_mode_info;
        DisplayModeInfo m_current_mode_info;
        std::vector<DisplayModeInfo> m_available_mode_infos;
    };
}

#include "SurfaceBridge.h" 

#ifdef USE_SDL3
#include "sdl/SDLWindow.h"
#include "sdl/SDLWindowSystem.h"

namespace lcf::gui {
    using Window = SDLWindow;
    using WindowSystem = SDLWindowSystem;
}
#endif // USE_SDL3