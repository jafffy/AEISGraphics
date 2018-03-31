// OctreeVoxelizerCmd.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <vector>
#include <cassert>

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <tiny_obj_loader.h>

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

void WriteObjOcttreeVoxels(VoxelOctree* octree)
{
    BuildOctreeGeometry(octree, octree->rootNode);

    unsigned int index = 1;

    std::vector<XMFLOAT3> vertices;
    std::vector<unsigned int> indices;

    for (const auto* leafNodeBox : octree->boxGeometries) {
        auto &Min = leafNodeBox->Min;
        auto &Max = leafNodeBox->Max;

        vertices.insert(vertices.end(),
            { XMFLOAT3(Min.x, Min.y, Min.z), 
              XMFLOAT3(Min.x, Min.y, Max.z),
              XMFLOAT3(Min.x, Max.y, Min.z), 
              XMFLOAT3(Min.x, Max.y, Max.z),
              XMFLOAT3(Max.x, Min.y, Min.z), 
              XMFLOAT3(Max.x, Min.y, Max.z),
              XMFLOAT3(Max.x, Max.y, Min.z), 
              XMFLOAT3(Max.x, Max.y, Max.z) });

        indices.insert(indices.end(),
        {
            index + 2,index + 0,index + 1, // -x
            index + 2,index + 1,index + 3,
            index + 6,index + 5,index + 4, // +x
            index + 6,index + 7,index + 5,
            index + 0,index + 5,index + 1, // -y
            index + 0,index + 4,index + 5,
            index + 2,index + 7,index + 6, // +y
            index + 2,index + 3,index + 7,
            index + 0,index + 6,index + 4, // -z
            index + 0,index + 2,index + 6,
            index + 1,index + 7,index + 3, // +z
            index + 1,index + 5,index + 7,
        });

        index += 8;
    }

    FILE* fp;

    fopen_s(&fp, "voxel.obj", "wt");

    for (int vi = 0; vi < vertices.size(); ++vi) {
        fprintf(fp, "v %f %f %f\n", vertices[vi].x, vertices[vi].y, vertices[vi].z);
    }
    for (int ii = 0; ii < indices.size() / 3; ++ii) {
        fprintf(fp, "f %d %d %d\n", indices[3 * ii + 0], indices[3 * ii + 1], indices[3 * ii + 2]);
    }
    fclose(fp);
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

int main()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, "bunny.obj", nullptr, true);

    std::vector<XMFLOAT3*> vertices;

    for (int i = 0; i < attrib.vertices.size() / 3; ++i) {
        vertices.push_back(reinterpret_cast<XMFLOAT3*>(attrib.vertices.data() + 3*i));
    }

    auto* voxelOctree = createOctree(vertices, 4);

    WriteObjOcttreeVoxels(voxelOctree);

    DirectX::BoundingBox bb;
    DirectX::BoundingBox::CreateFromPoints(bb, XMVectorSet(-1, -1, -1, 1), XMVectorSet(1, 1, 1, 1));

    XMFLOAT3 corners[8];
    bb.GetCorners(corners);

    printf("%d\n", bb.CORNER_COUNT);

    for (int i = 0; i < bb.CORNER_COUNT; ++i) {
        printf("%f %f %f\n", corners[i].x, corners[i].y, corners[i].z);
    }

    return 0;
}

