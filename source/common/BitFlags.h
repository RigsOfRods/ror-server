#pragma once

#include <cstdint>

/// @file
/// @brief Bit operations
typedef uint32_t BitMask_t;

#define BITMASK( OFFSET )           ( 1  << ((OFFSET) - 1) )

