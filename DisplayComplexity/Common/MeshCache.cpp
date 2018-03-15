#include "pch.h"
#include "Common\MeshCache.h"

#include "tiny_obj_loader.h"

using namespace DisplayComplexity;
using namespace DirectX;

struct BoundingBox3D {
    XMFLOAT3 Min, Max;

    BoundingBox3D()
        : Min(FLT_MAX, FLT_MAX, FLT_MAX),
        Max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}

    BoundingBox3D(const XMFLOAT3& Min, const XMFLOAT3& Max)
        : Min(Min), Max(Max) {}

    void AddPoint(const XMFLOAT3& p) {
        Min.x = p.x < Min.x ? p.x : Min.x;
        Min.y = p.y < Min.y ? p.y : Min.y;
        Min.z = p.z < Min.z ? p.z : Min.z;

        Max.x = p.x > Max.x ? p.x : Max.x;
        Max.y = p.y > Max.y ? p.y : Max.y;
        Max.z = p.z > Max.z ? p.z : Max.z;
    }

    bool IncludePoint(const XMFLOAT3& p) {
        return Min.x < p.x && Min.y < p.y && Min.z < p.z
            && Max.x > p.x && Max.y > p.y && Max.z > p.z;
    }
};

struct VoxelNode {
    std::vector<int> position; // size of position is depth. Root is empty
    BoundingBox3D* boundingBox = nullptr;

    VoxelNode* parent = nullptr;
    VoxelNode* children[8];
    bool isCompleteSubtree = false;
    bool isLeaf = false;

    VoxelNode() {
        memset(children, 0, sizeof(VoxelNode*) * 8);
    }
};

struct VoxelOctree {
    VoxelNode* rootNode = nullptr;
    std::vector<const BoundingBox3D*> boxGeometries;
};

void createSuboctree(VoxelNode** parent, const std::vector<XMFLOAT3*>& vertices, const BoundingBox3D& boundingBox, int depth) {
    assert(parent != nullptr && *parent != nullptr);
    XMVECTOR mid, Min, Max;
    Min = XMLoadFloat3(&boundingBox.Min);
    Max = XMLoadFloat3(&boundingBox.Max);
    mid = (Min + Max) * 0.5f;

    auto halfLength = Max - mid;
    auto halfLengthX = XMVectorSet(XMVectorGetX(halfLength), 0, 0, 0);
    auto halfLengthY = XMVectorSet(0, XMVectorGetY(halfLength), 0, 0);
    auto halfLengthZ = XMVectorSet(0, 0, XMVectorGetZ(halfLength), 0);

    BoundingBox3D childrensBoundingBox[8];

    XMStoreFloat3(&childrensBoundingBox[0].Min, Min + halfLengthZ);
    XMStoreFloat3(&childrensBoundingBox[0].Max, mid + halfLengthZ);

    XMStoreFloat3(&childrensBoundingBox[1].Min, Min - halfLengthY);
    XMStoreFloat3(&childrensBoundingBox[1].Max, Max - halfLengthY);

    XMStoreFloat3(&childrensBoundingBox[2].Min, Min + halfLengthX);
    XMStoreFloat3(&childrensBoundingBox[2].Max, mid + halfLengthX);

    XMStoreFloat3(&childrensBoundingBox[3].Min, Min);
    XMStoreFloat3(&childrensBoundingBox[3].Max, mid);

    XMStoreFloat3(&childrensBoundingBox[4].Min, mid - halfLengthX);
    XMStoreFloat3(&childrensBoundingBox[4].Max, Max - halfLengthX);

    XMStoreFloat3(&childrensBoundingBox[5].Min, mid);
    XMStoreFloat3(&childrensBoundingBox[5].Max, Max);

    XMStoreFloat3(&childrensBoundingBox[6].Min, mid - halfLengthZ);
    XMStoreFloat3(&childrensBoundingBox[6].Max, Max - halfLengthZ);

    XMStoreFloat3(&childrensBoundingBox[7].Min, Min + halfLengthY);
    XMStoreFloat3(&childrensBoundingBox[7].Max, mid + halfLengthY);

    for (int i = 0; i < 8; ++i) {
        std::vector<XMFLOAT3*> subvertices;

        for (auto* vertex : vertices) {
            if (childrensBoundingBox[i].IncludePoint(*vertex)) {
                subvertices.push_back(vertex);
            }
        }

        if (subvertices.size() > 0) {
            (*parent)->children[i] = new VoxelNode();
            (*parent)->children[i]->parent = *parent;
            (*parent)->children[i]->position = (*parent)->position;
            (*parent)->children[i]->position.push_back(i);

            BoundingBox3D* pChildBoundingBox = new BoundingBox3D();
            *pChildBoundingBox = childrensBoundingBox[i];
            (*parent)->children[i]->boundingBox = pChildBoundingBox;

            if (depth - 1 > 0) {
                createSuboctree(&(*parent)->children[i], subvertices, childrensBoundingBox[i], depth - 1);
            }
            else {
                (*parent)->children[i]->isLeaf = true;
            }
        }
    }

    int isFull = 0;

    for (int i = 0; i < 8; ++i) {
        if (parent && (*parent)->children[i]
            && ((*parent)->children[i]->isLeaf || (*parent)->children[i]->isCompleteSubtree)) {
            isFull++;
        }
    }

    if (isFull == 8) {
        (*parent)->isCompleteSubtree = true;
    }
}

void BuildOctreeGeometry(VoxelOctree* octree, VoxelNode* parent)
{
    if (!octree)
        return;

    if (parent->isCompleteSubtree) {
        octree->boxGeometries.push_back(parent->boundingBox);
    }
    else {
        for (int i = 0; i < 8; ++i) {
            auto* child = parent->children[i];

            if (child) {
                if (child->isLeaf) {
                    octree->boxGeometries.push_back(child->boundingBox);
                }
                else {
                    BuildOctreeGeometry(octree, parent->children[i]);
                }
            }
        }
    }
}

VoxelOctree* createOctree(const std::vector<XMFLOAT3*>& vertices, int depth) {
    VoxelOctree* voxelOctree = new VoxelOctree();
    voxelOctree->rootNode = new VoxelNode();

    BoundingBox3D *bb = new BoundingBox3D();
    for (auto* vertex : vertices) {
        bb->AddPoint(*vertex);
    }
    voxelOctree->rootNode->boundingBox = bb;

    createSuboctree(&voxelOctree->rootNode, vertices, *bb, depth);

    return voxelOctree;
}

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

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, ".\\Assets\\bunny.obj", ".\\Assets\\", false);
    if ( !err.empty() )
    {
        OutputDebugStringA(err.c_str());
    }

    if ( !ret )
    {
        OutputDebugStringA("Failed to load/parse .obj.\n");
    }

    Mesh* mesh = new Mesh();

    std::vector<XMFLOAT3*> vertices;

    for (int i = 0; i < attrib.vertices.size() / 3; ++i) {
        vertices.push_back(reinterpret_cast<XMFLOAT3*>(attrib.vertices.data() + 3 * i));
    }

    auto* voxelOctree = createOctree(vertices, 5);

    BuildOctreeGeometry(voxelOctree, voxelOctree->rootNode);

    {
        unsigned short index = 0;

        for (const auto* leafNodeBox : voxelOctree->boxGeometries) {
            auto &Min = leafNodeBox->Min;
            auto &Max = leafNodeBox->Max;

            mesh->meshVertices.insert(mesh->meshVertices.end(),
                { VertexPositionColor(XMFLOAT3(Min.x, Min.y, Min.z), XMFLOAT3(1, 1, 1)),
                VertexPositionColor(XMFLOAT3(Min.x, Min.y, Max.z), XMFLOAT3(1, 1, 1)),
                VertexPositionColor(XMFLOAT3(Min.x, Max.y, Min.z), XMFLOAT3(1, 1, 1)),
                VertexPositionColor(XMFLOAT3(Min.x, Max.y, Max.z), XMFLOAT3(1, 1, 1)),
                VertexPositionColor(XMFLOAT3(Max.x, Min.y, Min.z), XMFLOAT3(1, 1, 1)),
                VertexPositionColor(XMFLOAT3(Max.x, Min.y, Max.z), XMFLOAT3(1, 1, 1)),
                VertexPositionColor(XMFLOAT3(Max.x, Max.y, Min.z), XMFLOAT3(1, 1, 1)),
                VertexPositionColor(XMFLOAT3(Max.x, Max.y, Max.z), XMFLOAT3(1, 1, 1)) });

            mesh->meshIndices.insert(mesh->meshIndices.end(),
                {
                    unsigned short(index + 2),unsigned short(index + 0),unsigned short(index + 1), // -x
                    unsigned short(index + 2),unsigned short(index + 1),unsigned short(index + 3),
                    unsigned short(index + 6),unsigned short(index + 5),unsigned short(index + 4), // +x
                    unsigned short(index + 6),unsigned short(index + 7),unsigned short(index + 5),
                    unsigned short(index + 0),unsigned short(index + 5),unsigned short(index + 1), // -y
                    unsigned short(index + 0),unsigned short(index + 4),unsigned short(index + 5),
                    unsigned short(index + 2),unsigned short(index + 7),unsigned short(index + 6), // +y
                    unsigned short(index + 2),unsigned short(index + 3),unsigned short(index + 7),
                    unsigned short(index + 0),unsigned short(index + 6),unsigned short(index + 4), // -z
                    unsigned short(index + 0),unsigned short(index + 2),unsigned short(index + 6),
                    unsigned short(index + 1),unsigned short(index + 7),unsigned short(index + 3), // +z
                    unsigned short(index + 1),unsigned short(index + 5),unsigned short(index + 7),
                });

            index += 8;
        }

        OutputDebugStringA(("Index:" + std::to_string(index) + "\n").c_str());
    }

    meshes[path] = mesh;

    return mesh;
}