#ifndef _FMATH_HPP_
#define _FMATH_HPP_

// Column-major 4x4 matrix (16 floats)
struct FMat4 {
    float m[16];

    float& operator[](size_t i) { return m[i]; }
    float const& operator[](size_t i) const { return m[i]; }
};

struct FVec3 {
    float x, y, z;
};

// UNDONE: Move FMath into FMat4.
namespace FMath {

FMat4 Identity();

// Perspective projection (Vulkan: right-handed, depth range [0,1])
// fovY in radians, aspect = width/height
FMat4 Perspective(float fovY, float aspect, float nearZ, float farZ);

// View matrix: right-handed lookAt
FMat4 LookAt(FVec3 eye, FVec3 target, FVec3 up);

// Matrix multiplication: out = a * b
FMat4 Multiply(FMat4 const& a, FMat4 const& b);

// Load a mat4 from a raw float[16] array (column-major)
FMat4 FromFloat16(float const* src);

} // namespace FMath

#endif