#pragma once

// --- Fix: All Standard Library headers missing from CommonLibSSE/REL headers ---
#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <stdexcept>
#include <optional>
#include <format>
#include <bit>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <span>
#include <fstream>   // Fixes std::basic_ifstream
#include <algorithm> // Fixes std::lower_bound

// Fixes 'invalid literal suffix "sv"'
using namespace std::literals;

// --- CRITICAL FIX: SKSE-Specific namespace issues ---

// Include the internal SKSE PCH/core header (which likely defines stl::zwstring and utilities)
#include "SKSE/SKSE.h"
#include "SKSE/Impl/PCH.h"

// Alias the expected 'stl' namespace to the actual 'SKSE::stl' namespace
// This is necessary because CommonLibSSE headers (REL/*.h) use 'stl::' unqualified.
namespace stl = SKSE::stl;