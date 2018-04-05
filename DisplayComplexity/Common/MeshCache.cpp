#include "pch.h"
#include "Common\MeshCache.h"

#include "tiny_obj_loader.h"

using namespace DisplayComplexity;
using namespace DirectX;

int granularity = 30;
int depth = 30;

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

struct BinaryTree;
struct BinaryTreeNode;

struct BinaryTreeNode {
public:
	std::vector<XMFLOAT3*> vertices;
	BinaryTreeNode * left = nullptr, *right = nullptr;
	BinaryTree* tree = nullptr;;
};

struct BinaryTree {
public:
	BinaryTreeNode * rootNode = nullptr;
	std::vector<BoundingBox3D> boundingBoxes;
};

void BuildBinarySubtree(BinaryTreeNode* parent, BoundingBox3D boundingBox, int depth)
{
	for (int i = 0; i < parent->vertices.size(); ++i) {
		boundingBox.AddPoint(*parent->vertices[i]);
	}

	float widths[3] = {
		boundingBox.Max.x - boundingBox.Min.x,
		boundingBox.Max.y - boundingBox.Min.y,
		boundingBox.Max.z - boundingBox.Min.z
	};

	float mid[] = {
		boundingBox.Min.x + widths[0] * 0.5f,
		boundingBox.Min.y + widths[1] * 0.5f,
		boundingBox.Min.z + widths[2] * 0.5f
	};

	int orderedIndices[3] = { 0 };
	float minValue = FLT_MAX, maxValue = -FLT_MAX;

	for (int i = 0; i < 3; ++i) {
		if (widths[i] > maxValue) {
			maxValue = widths[i];
			orderedIndices[0] = i;
		}
		if (widths[i] < minValue) {
			minValue = widths[i];
			orderedIndices[2] = i;
		}
	}

	for (int i = 0; i < 3; ++i) {
		if (i != orderedIndices[0] && i != orderedIndices[2]) {
			orderedIndices[1] = i;
			break;
		}
	}

	int currentIndex = 0;
FAILED:
	int maxIndices;
	maxIndices = orderedIndices[currentIndex];

	for (int i = 0; i < parent->vertices.size(); ++i) {
		const float* pVertices = (const float*)parent->vertices[i];

		if (pVertices[maxIndices] > mid[maxIndices]) {
			if (!parent->left) {
				parent->left = new BinaryTreeNode();
				parent->left->tree = parent->tree;
			}

			parent->left->vertices.push_back(parent->vertices[i]);
		}
		else {
			if (!parent->right) {
				parent->right = new BinaryTreeNode();
				parent->right->tree = parent->tree;
			}

			parent->right->vertices.push_back(parent->vertices[i]);
		}
	}

	if (!parent->left || !parent->right) {
		if (parent->left)
			parent->left->vertices.clear();
		if (parent->right)
			parent->right->vertices.clear();

		if (++currentIndex < 3)
			goto FAILED;
		else {
			assert(false);
		}
	}

	BoundingBox3D leftBB;

	for (int i = 0; i < parent->left->vertices.size(); ++i) {
		leftBB.AddPoint(*parent->left->vertices[i]);
	}

	BoundingBox3D rightBB;

	for (int i = 0; i < parent->right->vertices.size(); ++i) {
		rightBB.AddPoint(*parent->right->vertices[i]);
	}

	if (parent->left && parent->left->vertices.size() > granularity && depth - 1 > 0) {
		BuildBinarySubtree(parent->left, leftBB, depth - 1);
	}
	else if (parent->left) {
		parent->tree->boundingBoxes.push_back(leftBB);
	}

	if (parent->right && parent->right->vertices.size() > granularity && depth - 1 > 0) {
		BuildBinarySubtree(parent->right, rightBB, depth - 1);
	}
	else if (parent->right) {
		parent->tree->boundingBoxes.push_back(rightBB);
	}
}

BinaryTree* BuildBinaryTree(std::vector<XMFLOAT3*> vertices, int depth)
{
	BinaryTree* tree = new BinaryTree();

	BoundingBox3D boundingBox;

	for (int i = 0; i < vertices.size(); ++i) {
		boundingBox.AddPoint(*vertices[i]);
	}

	XMFLOAT3 median;
	median.x = (boundingBox.Max.x + boundingBox.Min.x) * 0.5f;
	median.y = (boundingBox.Max.y + boundingBox.Min.y) * 0.5f;
	median.z = (boundingBox.Max.z + boundingBox.Min.z) * 0.5f;

	tree->rootNode = new BinaryTreeNode();
	tree->rootNode->vertices = vertices;
	tree->rootNode->tree = tree;

	for (int i = 0; i < vertices.size(); ++i) {
		vertices[i]->x -= median.x;
		vertices[i]->y -= median.y;
		vertices[i]->z -= median.z;
	}

	BuildBinarySubtree(tree->rootNode, boundingBox, depth);

	for (auto& bb : tree->boundingBoxes) {
		bb.Max.x += median.x;
		bb.Max.y += median.y;
		bb.Max.z += median.z;

		bb.Min.x += median.x;
		bb.Min.y += median.y;
		bb.Min.z += median.z;
	}

	return tree;
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

	/*
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
	*/
    std::vector<XMFLOAT3*> vertices;

    for (int i = 0; i < attrib.vertices.size() / 3; ++i) {
        vertices.push_back(reinterpret_cast<XMFLOAT3*>(attrib.vertices.data() + 3 * i));
    }

	LARGE_INTEGER lastTime, currentTime, frequency;
	QueryPerformanceFrequency(&frequency);

	static const unsigned TicksPerSecond = 10'000'000;

	QueryPerformanceCounter(&lastTime);

	BuildBinaryTree(vertices, depth);

	QueryPerformanceCounter(&currentTime);

	uint64 timeDelta = currentTime.QuadPart - lastTime.QuadPart;

	timeDelta *= TicksPerSecond;
	timeDelta /= frequency.QuadPart;

	double dt = static_cast<double>(timeDelta) / TicksPerSecond;

	OutputDebugStringA(std::to_string(dt).c_str());
	OutputDebugStringA("\n");



	/*

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
	*/

	return mesh;
}