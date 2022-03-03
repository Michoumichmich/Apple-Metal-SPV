//-*- mode: c++ -*-
// build:
//        xcrun -sdk macosx metal -Wall -Wextra -std=osx-metal1.1 Shaders.metal -c -o Shaders.air
//        xcrun -sdk macosx metal-ar rcs Shaders.metal-ar Shaders.air
//        xcrun -sdk macosx metallib -o Shaders.metallib Shaders.metal-ar
// OR:
//        xcrun -sdk macosx metal -Wall -Wextra -std=macos-metal2.4 Shaders.metal -c -o Shaders.air -Ofast
//        xcrun -sdk macosx metallib -o Shaders.metallib Shaders.air

#include <metal_stdlib>

kernel void sqrtf(const device float *input [[buffer(0)]], device float *output [[buffer(1)]], metal::uint id [[thread_position_in_grid]]) {
    output[id] = metal::sqrt(input[id]);
}

kernel void sqrth(const device half *input [[buffer(0)]], device half *output [[buffer(1)]], metal::uint id [[thread_position_in_grid]]) {
    output[id] = metal::sqrt(input[id]);
}
