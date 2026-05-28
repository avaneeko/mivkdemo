#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include "FMat4.hpp"

#include <cglm/cglm.h>
#include <cstring>

FMat4 FMat4::Perspective(float fovY, float aspect, float nearZ, float farZ)
{
    mat4 tmp;
    glm_perspective_lh_zo(fovY, aspect, nearZ, farZ, tmp);
    FMat4 out;
    std::memcpy(out.m, tmp, sizeof(float) * 16);
    out.m[5] = -out.m[5]; // Inverse due to Vulkan negative viewport Y.
    return out;
}

FMat4 FMat4::LookAt(FVec3 eye, FVec3 target, FVec3 up)
{
    // UNDONE: Fix this, because memcpy isn't getting optimized out.
    mat4 tmp;
    vec3 e = { eye.x, eye.y, eye.z };
    vec3 t = { target.x, target.y, target.z };
    vec3 u = { up.x, up.y, up.z };
    glm_lookat(e, t, u, tmp);
    FMat4 out;
    std::memcpy(out.m, tmp, sizeof(float) * 16);
    return out;
}

FMat4 FMat4::FromFloat16(float const* src)
{
    FMat4 out;
    for (int i = 0; i < 16; i++)
        out.m[i] = src[i];
    return out;
}

FMat4 FMat4::Identity()
{
    FMat4 out = {};
    out.m[0] = out.m[5] = out.m[10] = out.m[15] = 1.0f;
    return out;
}

FMat4 FMat4::YUpRHSwizzleMatrix()
{
    // glTF (Y-up, RH) -> (Z-up, LH):
    //   glTF X -> our Y
    //   glTF Y -> our Z
    //   glTF Z -> our -X (negate for LH)
    // FMat4 m = {};
    // m.m[1]  = 1.0f;  // col0 = (0,1,0,0)
    // m.m[6]  = 1.0f;  // col1 = (0,0,1,0)
    // m.m[8]  = -1.0f; // col2 = (-1,0,0,0)
    // m.m[15] = 1.0f;  // col3 = (0,0,0,1)

    FMat4 m = {};
    m.m[0]  = -1.0f;
    m.m[6]  =  1.0f;
    m.m[9]  = -1.0f;
    m.m[15] =  1.0f;
    return m;
}

FMat4 FMat4::ConvertYUpRHToZUpLH(FMat4 const& original)
{
    FMat4 swizzle = YUpRHSwizzleMatrix();
    // swizzle * original * swizzle^T
    // return swizzle * original * swizzle; // WRONG!! swizzle is orthonormal, so ^T = inverse = itself here
    return swizzle * original;
}

FMat4 FMat4::Multiply(FMat4 const& other) const
{
    mat4 tmp;
    glm_mat4_mul((float(*)[4])m, (float(*)[4])other.m, tmp);
    FMat4 out;
    std::memcpy(out.m, tmp, sizeof(float) * 16);
    return out;
}