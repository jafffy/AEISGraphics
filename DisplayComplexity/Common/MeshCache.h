#pragma once

#include "Content\ShaderStructures.h"
#include "Content\Singleton.h"
#include <map>
#include <vector>

namespace DisplayComplexity
{
    class Mesh
    {
    public:
        std::vector<VertexPositionColor> meshVertices;
        std::vector<unsigned short> meshIndices;
    };

    class MeshCache : public Singleton<MeshCache>
    {
    public:
        std::map<std::string, Mesh*> meshes;

        Mesh* GetMesh(const std::string& path, const std::string& base_dir);

        ~MeshCache();
    };
}