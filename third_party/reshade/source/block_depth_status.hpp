/*
 * Shared depth readiness flags for Block Home / OSD (updated by Generic Depth add-ons).
 */
#pragma once

#include <atomic>
#include <cstdint>

namespace block
{
	inline std::atomic<uint32_t> g_depth_ready { 0 };     // 1 = DEPTH bound
	inline std::atomic<uint32_t> g_depth_ii_ready { 0 };  // 1 = DEPTH_II bound
	inline std::atomic<uint32_t> g_depth_width { 0 };
	inline std::atomic<uint32_t> g_depth_height { 0 };
}
