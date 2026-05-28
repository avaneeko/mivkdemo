#ifndef _FMAT4_HPP_
#define _FMAT4_HPP_

#include <cstdint>

struct FVec3
{
    float x, y, z;
};

// Column-major 4x4 matrix.
struct FMat4 {
    float m[16];

    float& operator[](size_t i) { return m[i]; }
    float const& operator[](size_t i) const { return m[i]; }

    // Left-handed perspective, [0,1] depth range.
    // fovY in radians
    static FMat4 Perspective(float fovY, float aspect, float nearZ, float farZ);

    // Left-handed lookAt view matrix.
    static FMat4 LookAt(FVec3 eye, FVec3 target, FVec3 up);

    // Load from raw column-major float[16].
    static FMat4 FromFloat16(float const* src);

    // Returns a matrix that converts from glTF (Y-up, RH) to our (Z-up, LH) space.
    // converted = this * original * this^T (similarity transform).
    static FMat4 YUpRHSwizzleMatrix();

    // Converts a world matrix from glTF Y-up RH to our Z-up LH.
    static FMat4 ConvertYUpRHToZUpLH(FMat4 const& original);

    static FMat4 Identity();

    // Returns this * other (column-matrix multiplication).
    FMat4 Multiply(FMat4 const& other) const;

    FMat4 operator*(FMat4 const& other) const { return Multiply(other); }
};

#endif