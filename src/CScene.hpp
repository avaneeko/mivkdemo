#ifndef _CSCENE_HPP_
#define _CSCENE_HPP_

#include <string>
#include <vector>
#include <cstdint>

struct FSceneInstance
{
    uint32_t MeshIndex;
    float WorldMatrix[16]; // Column-major 4x4 transform
};

class CScene
{
public:
    std::string Name;
    std::vector<FSceneInstance> Instances;
};

#endif