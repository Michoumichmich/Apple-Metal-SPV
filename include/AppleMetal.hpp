#pragma once
//https://developer.apple.com/metal/cpp/

#ifndef NS_PRIVATE_IMPLEMENTATION
#    define NS_PRIVATE_IMPLEMENTATION
#endif
#ifndef CA_PRIVATE_IMPLEMENTATION
#    define CA_PRIVATE_IMPLEMENTATION
#endif

#ifndef MTL_PRIVATE_IMPLEMENTATION
#    define MTL_PRIVATE_IMPLEMENTATION
#endif

#include "metal-cpp/Foundation/Foundation.hpp"
#include "metal-cpp/Metal/Metal.hpp"
#include "metal-cpp/QuartzCore/QuartzCore.hpp"

#undef MTL_PRIVATE_IMPLEMENTATION
#undef CA_PRIVATE_IMPLEMENTATION
#undef NS_PRIVATE_IMPLEMENTATION