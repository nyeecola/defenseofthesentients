#include <assimp/cimport.h>     // C++ importer interface
#include <assimp/scene.h>       // Output data structure
#include <assimp/postprocess.h> // Post processing flags
#include <vector>
#include <map>

#define MAX_BONES_PER_VERTEX 4

typedef struct {
    int bone_ids[MAX_BONES_PER_VERTEX] = { 0 };
    float weights[MAX_BONES_PER_VERTEX] = { 0.0f };
    int num_bones = 0;
} VertexBoneData;

typedef struct {
    //GLuint VAO;

    std::vector<VertexBoneData> bones;
    std::vector<int> indices;
    std::vector<float[3]> positions;
    std::vector<float[3]> normals;
    std::vector<float[2]> tex_coords;

    std::map<std::string, int> bone_name2id;
} Model;

std::vector<VertexBoneData> vertex2bones;
std::vector<int> mesh_base_vertex;
std::map<std::string, int> bone_name2id;

bool loadModel(char *filename)
{
    // Release the previously loaded mesh (if it exists)
    //Clear();

    // Create the VAO
    //glGenVertexArrays(1, &m_VAO);
    //glBindVertexArray(m_VAO);

    // Create the buffers for the vertices attributes
    //glGenBuffers(ARRAY_SIZE_IN_ELEMENTS(m_Buffers), m_Buffers);

    const aiScene *scene = aiImportFile(filename, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices);
    if (!scene) {
        fprintf(stderr, "Error parsing '%s': '%s'\n", filename, aiGetErrorString());
        return false;
    }

    mesh_base_vertex.resize(scene->mNumMeshes);

    int total_vertices = 0;

    for (int i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
        fprintf(stderr, "Loaded Mesh #%d with %d vertices and % bones.\n", i, mesh->mNumVertices, mesh->mNumBones);

        mesh_base_vertex[i] = total_vertices;
        total_vertices += mesh->mNumVertices;

        vertex2bones.resize(total_vertices);

        for (int b = 0; b < mesh->mNumBones; b++) {
            const aiBone* bone = mesh->mBones[b];

            std::string bone_name(bone->mName.C_Str());
            if (bone_name2id.find(bone_name) == bone_name2id.end()) {
                bone_name2id[bone_name] = bone_name2id.size();
            }
            int bone_id = bone_name2id[bone_name];

            for (int w = 0; w < bone->mNumWeights; w++) {
                const aiVertexWeight vw = bone->mWeights[w];

                int vertex_id = mesh_base_vertex[i] + vw.mVertexId;

                assert(vertex_id < vertex2bones.size());
                vertex2bones[vertex_id].bone_ids[vertex2bones[vertex_id].num_bones] = bone_id;
                vertex2bones[vertex_id].weights[vertex2bones[vertex_id].num_bones] = vw.mWeight;
                vertex2bones[vertex_id].num_bones++;
            }
        }
    }

    // Make sure the VAO is not changed from the outside
    //glBindVertexArray(0);

    return true;
}

bool initFromScene(const aiScene* pScene, const string& Filename)
{
    ...
        vector<VertexBoneData> Bones;
    ...
        Bones.resize(NumVertices);
    ...
        glBindBuffer(GL_ARRAY_BUFFER, m_Buffers[BONE_VB]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Bones[0]) * Bones.size(), &Bones[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(BONE_ID_LOCATION);
    glVertexAttribIPointer(BONE_ID_LOCATION, 4, GL_INT, sizeof(VertexBoneData), (const GLvoid*)0);
    glEnableVertexAttribArray(BONE_WEIGHT_LOCATION);
    glVertexAttribPointer(BONE_WEIGHT_LOCATION, 4, GL_FLOAT, GL_FALSE, sizeof(VertexBoneData), (const GLvoid*)16);
    ...
}

void Mesh::LoadBones(uint MeshIndex, const aiMesh* pMesh, vector& Bones)
{
    for (uint i = 0; i < pMesh->mNumBones; i++) {
        uint BoneIndex = 0;
        string BoneName(pMesh->mBones[i]->mName.data);

        if (m_BoneMapping.find(BoneName) == m_BoneMapping.end()) {
            BoneIndex = m_NumBones;
            m_NumBones++;
            BoneInfo bi;
            m_BoneInfo.push_back(bi);
        }
        else {
            BoneIndex = m_BoneMapping[BoneName];
        }

        m_BoneMapping[BoneName] = BoneIndex;
        m_BoneInfo[BoneIndex].BoneOffset = pMesh->mBones[i]->mOffsetMatrix;

        for (uint j = 0; j < pMesh->mBones[i]->mNumWeights; j++) {
            uint VertexID = m_Entries[MeshIndex].BaseVertex + pMesh->mBones[i]->mWeights[j].mVertexId;
            float Weight = pMesh->mBones[i]->mWeights[j].mWeight;
            Bones[VertexID].AddBoneData(BoneIndex, Weight);
        }
    }
}

void Mesh::VertexBoneData::AddBoneData(uint BoneID, float Weight)
{
    for (uint i = 0; i < ARRAY_SIZE_IN_ELEMENTS(IDs); i++) {
        if (Weights[i] == 0.0) {
            IDs[i] = BoneID;
            Weights[i] = Weight;
            return;
        }
    }

    // should never get here - more bones than we have space for
    assert(0);
}

Matrix4f Mesh::BoneTransform(float TimeInSeconds, vector<Matrix4f>& Transforms)
{
    Matrix4f Identity;
    Identity.InitIdentity();

    float TicksPerSecond = m_pScene->mAnimations[0]->mTicksPerSecond != 0 ?
        m_pScene->mAnimations[0]->mTicksPerSecond : 25.0f;
    float TimeInTicks = TimeInSeconds * TicksPerSecond;
    float AnimationTime = fmod(TimeInTicks, m_pScene->mAnimations[0]->mDuration);

    ReadNodeHeirarchy(AnimationTime, m_pScene->mRootNode, Identity);

    Transforms.resize(m_NumBones);

    for (uint i = 0; i < m_NumBones; i++) {
        Transforms[i] = m_BoneInfo[i].FinalTransformation;
    }
}

void Mesh::ReadNodeHeirarchy(float AnimationTime, const aiNode* pNode, const Matrix4f& ParentTransform)
{
    string NodeName(pNode->mName.data);

    const aiAnimation* pAnimation = m_pScene->mAnimations[0];

    Matrix4f NodeTransformation(pNode->mTransformation);

    const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, NodeName);

    if (pNodeAnim) {
        // Interpolate scaling and generate scaling transformation matrix
        aiVector3D Scaling;
        CalcInterpolatedScaling(Scaling, AnimationTime, pNodeAnim);
        Matrix4f ScalingM;
        ScalingM.InitScaleTransform(Scaling.x, Scaling.y, Scaling.z);

        // Interpolate rotation and generate rotation transformation matrix
        aiQuaternion RotationQ;
        CalcInterpolatedRotation(RotationQ, AnimationTime, pNodeAnim);
        Matrix4f RotationM = Matrix4f(RotationQ.GetMatrix());

        // Interpolate translation and generate translation transformation matrix
        aiVector3D Translation;
        CalcInterpolatedPosition(Translation, AnimationTime, pNodeAnim);
        Matrix4f TranslationM;
        TranslationM.InitTranslationTransform(Translation.x, Translation.y, Translation.z);

        // Combine the above transformations
        NodeTransformation = TranslationM * RotationM * ScalingM;
    }

    Matrix4f GlobalTransformation = ParentTransform * NodeTransformation;

    if (m_BoneMapping.find(NodeName) != m_BoneMapping.end()) {
        uint BoneIndex = m_BoneMapping[NodeName];
        m_BoneInfo[BoneIndex].FinalTransformation = m_GlobalInverseTransform * GlobalTransformation *
            m_BoneInfo[BoneIndex].BoneOffset;
    }

    for (uint i = 0; i < pNode->mNumChildren; i++) {
        ReadNodeHeirarchy(AnimationTime, pNode->mChildren[i], GlobalTransformation);
    }
}

void Mesh::CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    // we need at least two values to interpolate...
    if (pNodeAnim->mNumRotationKeys == 1) {
        Out = pNodeAnim->mRotationKeys[0].mValue;
        return;
    }

    uint RotationIndex = FindRotation(AnimationTime, pNodeAnim);
    uint NextRotationIndex = (RotationIndex + 1);
    assert(NextRotationIndex < pNodeAnim->mNumRotationKeys);
    float DeltaTime = pNodeAnim->mRotationKeys[NextRotationIndex].mTime - pNodeAnim->mRotationKeys[RotationIndex].mTime;
    float Factor = (AnimationTime - (float)pNodeAnim->mRotationKeys[RotationIndex].mTime) / DeltaTime;
    assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiQuaternion& StartRotationQ = pNodeAnim->mRotationKeys[RotationIndex].mValue;
    const aiQuaternion& EndRotationQ = pNodeAnim->mRotationKeys[NextRotationIndex].mValue;
    aiQuaternion::Interpolate(Out, StartRotationQ, EndRotationQ, Factor);
    Out = Out.Normalize();
}

uint Mesh::FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    assert(pNodeAnim->mNumRotationKeys > 0);

    for (uint i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
        if (AnimationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime) {
            return i;
        }
    }

    assert(0);
}