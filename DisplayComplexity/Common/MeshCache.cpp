#include "pch.h"
#include "Common\MeshCache.h"

#include "tiny_obj_loader.h"

using namespace DisplayComplexity;
using namespace DirectX;

MeshCache::~MeshCache()
{
    for ( auto it = meshes.begin();
        it != meshes.end(); ++it )
    {
        if ( it->second != nullptr )
        {
            delete it->second;
            it->second = nullptr;
        }
    }

    meshes.clear();
}

Mesh* MeshCache::GetMesh(const std::string& path, const std::string& base_dir)
{
    if ( meshes.find(path) != meshes.end() )
        return meshes[path];

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, ".\\Assets\\bunny_2.obj", ".\\Assets\\", false);
    if ( !err.empty() )
    {
        OutputDebugStringA(err.c_str());
    }

    if ( !ret )
    {
		OutputDebugStringA("Failed to load/parse .obj.\n");
	}

	Mesh* mesh = new Mesh();

	int N = 1;
	for (unsigned int n = 0; n < N; ++n) {
		for (unsigned int i = 0; i < attrib.vertices.size() / 3; ++i)
		{
			XMFLOAT3 v;
			v.x = attrib.vertices[3 * i + 0] * 0.01f + n * 0.025f * 0.5f;
			v.y = attrib.vertices[3 * i + 1] * 0.01f;
			v.z = attrib.vertices[3 * i + 2] * 0.01f;

			mesh->meshVertices.push_back(VertexPositionColor(v, XMFLOAT3(1, 1, 1)));
		}

		for (unsigned int si = 0; si < shapes.size(); ++si)
		{
			int baseIndex = n * attrib.vertices.size() / 3;

			for (unsigned int ii = 0; ii < shapes[si].mesh.indices.size() / 3; ++ii)
			{
				unsigned int a = baseIndex + shapes[si].mesh.indices[3 * ii + 0].vertex_index;
				unsigned int b = baseIndex + shapes[si].mesh.indices[3 * ii + 1].vertex_index;
				unsigned int c = baseIndex + shapes[si].mesh.indices[3 * ii + 2].vertex_index;

				// CW order
				mesh->meshIndices.push_back(a);
				mesh->meshIndices.push_back(c);
				mesh->meshIndices.push_back(b);
			}
		}
	}
	meshes[path] = mesh;

	return mesh;
}