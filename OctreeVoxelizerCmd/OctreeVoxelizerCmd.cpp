// OctreeVoxelizerCmd.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <vector>
#include <cassert>

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "tiny_obj_loader.h"

using namespace DirectX;

const char* title = "bunnyGran1.obj";
int granularity = 1;
int binaryTreeDepth = 1;

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

	printf("%d: mid=%f\n", depth, mid[maxIndices]);

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

void WriteObjBinaryTreeVoxels(BinaryTree* tree)
{
	unsigned int index = 1;

	std::vector<XMFLOAT3> vertices;
	std::vector<unsigned int> indices;

	for (const auto leafNodeBox : tree->boundingBoxes) {
		auto &Min = leafNodeBox.Min;
		auto &Max = leafNodeBox.Max;

		float width = 1.f;

		vertices.insert(vertices.end(),
			{
				XMFLOAT3(Min.x * width, Min.y * width, Min.z * width),
			XMFLOAT3(Min.x * width, Min.y * width, Max.z * width),
			XMFLOAT3(Min.x * width, Max.y * width, Min.z * width),
			XMFLOAT3(Min.x * width, Max.y * width, Max.z * width),
			XMFLOAT3(Max.x * width, Min.y * width, Min.z * width),
			XMFLOAT3(Max.x * width, Min.y * width, Max.z * width),
			XMFLOAT3(Max.x * width, Max.y * width, Min.z * width),
			XMFLOAT3(Max.x * width, Max.y * width, Max.z * width)
			});

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

	fopen_s(&fp, title, "wt");

	for (int vi = 0; vi < vertices.size(); ++vi) {
		fprintf(fp, "v %f %f %f\n", vertices[vi].x, vertices[vi].y, vertices[vi].z);
	}
	for (int ii = 0; ii < indices.size() / 3; ++ii) {
		fprintf(fp, "f %d %d %d\n", indices[3 * ii + 0], indices[3 * ii + 1], indices[3 * ii + 2]);
	}
	fclose(fp);
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
		vertices.push_back(reinterpret_cast<XMFLOAT3*>(attrib.vertices.data() + 3 * i));
	}

	WriteObjBinaryTreeVoxels(BuildBinaryTree(vertices, binaryTreeDepth));

	return 0;
}

