#include "core/AssimpImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

namespace pmx {

namespace {

void setError(QString* errorMessage, const QString& msg) {
    if (errorMessage != nullptr) {
        *errorMessage = msg;
    }
}

uint8_t calcVertexIndexSize(size_t count) {
    if (count > 65535) {
        return 4;
    }
    if (count > 255) {
        return 2;
    }
    return 1;
}

uint8_t calcNonVertexIndexSize(size_t count) {
    if (count > 32767) {
        return 4;
    }
    if (count > 127) {
        return 2;
    }
    return 1;
}

unsigned int buildPostProcessFlags(const AssimpImportOptions& options) {
    unsigned int flags = aiProcess_Triangulate |
                         aiProcess_JoinIdenticalVertices |
                         aiProcess_SortByPType |
                         aiProcess_MakeLeftHanded |
                         aiProcess_FlipWindingOrder;

    if (options.generateNormals) {
        flags |= aiProcess_GenSmoothNormals;
    }
    if (options.flipUVs) {
        flags |= aiProcess_FlipUVs;
    }
    return flags;
}

std::unique_ptr<PmxDocument> processScene(
    const aiScene* scene,
    const AssimpImportOptions& options,
    const QString& filePath,
    QString* errorMessage)
{
    if (scene == nullptr) {
        setError(errorMessage, QStringLiteral("Null scene passed to processScene"));
        return nullptr;
    }

    if (scene->mNumMeshes == 0) {
        setError(errorMessage, QStringLiteral("Scene contains no meshes"));
        return nullptr;
    }

    auto doc = std::make_unique<PmxDocument>();
    doc->sourceFilePath = filePath;

    // Model name and comments
    if (!filePath.isEmpty()) {
        QFileInfo fi(filePath);
        doc->modelInfo.modelNameJp = fi.completeBaseName();
        doc->modelInfo.modelNameEn = fi.completeBaseName();
    } else {
        doc->modelInfo.modelNameJp = QStringLiteral("Accessory");
        doc->modelInfo.modelNameEn = QStringLiteral("Accessory");
    }

    if (scene->mMetaData != nullptr) {
        aiString metaStr;
        if (scene->mMetaData->Get("Title", metaStr) || scene->mMetaData->Get("Name", metaStr)) {
            QString name = QString::fromUtf8(metaStr.C_Str()).trimmed();
            if (!name.isEmpty()) {
                doc->modelInfo.modelNameJp = name;
                doc->modelInfo.modelNameEn = name;
            }
        }
    }

    doc->modelInfo.commentJp = QStringLiteral("Imported via Assimp");
    doc->modelInfo.commentEn = QStringLiteral("Imported via Assimp");

    // Default material fallback for out-of-bounds or missing material indices
    auto createDefaultWhiteMaterial = []() -> PmxMaterial {
        PmxMaterial def;
        def.nameJp = QString::fromUtf8("材質");
        def.nameEn = QStringLiteral("Material");
        def.diffuse = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        def.specular = glm::vec3(0.0f, 0.0f, 0.0f);
        def.specularPower = 5.0f;
        def.ambient = glm::vec3(0.5f, 0.5f, 0.5f);
        def.drawFlags = 0x1F;
        def.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        def.edgeSize = 1.0f;
        def.textureIndex = -1;
        def.sphereTextureIndex = -1;
        def.sphereMode = 0;
        def.sharedToonFlag = 0;
        def.toonIndex = -1;
        def.memo = QString();
        def.faceVertexCount = 0;
        return def;
    };

    // Pre-convert Assimp materials
    std::vector<PmxMaterial> convertedMaterials;
    if (scene->mMaterials != nullptr) {
        convertedMaterials.reserve(scene->mNumMaterials);

        for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {
            const aiMaterial* aiMat = scene->mMaterials[m];
            if (aiMat == nullptr) {
                convertedMaterials.push_back(createDefaultWhiteMaterial());
                continue;
            }
            PmxMaterial pmxMat;

            aiString matName;
            if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
                QString name = QString::fromUtf8(matName.C_Str()).trimmed();
                if (!name.isEmpty()) {
                    pmxMat.nameJp = name;
                    pmxMat.nameEn = name;
                }
            }
            if (pmxMat.nameJp.isEmpty()) {
                pmxMat.nameJp = QStringLiteral("Material_%1").arg(m);
                pmxMat.nameEn = pmxMat.nameJp;
            }

            // Diffuse RGBA + Opacity
            aiColor4D diffColor(1.0f, 1.0f, 1.0f, 1.0f);
            if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffColor) == AI_SUCCESS) {
                pmxMat.diffuse = glm::vec4(diffColor.r, diffColor.g, diffColor.b, diffColor.a);
            }
            float opacity = 1.0f;
            if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
                pmxMat.diffuse.a = opacity;
            }

            // Specular RGB + Shininess
            aiColor3D specColor(0.0f, 0.0f, 0.0f);
            if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specColor) == AI_SUCCESS) {
                pmxMat.specular = glm::vec3(specColor.r, specColor.g, specColor.b);
            }
            float shininess = 5.0f;
            if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
                pmxMat.specularPower = shininess;
            }

            // Ambient RGB
            aiColor3D ambColor(0.5f, 0.5f, 0.5f);
            if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, ambColor) == AI_SUCCESS) {
                pmxMat.ambient = glm::vec3(ambColor.r, ambColor.g, ambColor.b);
            }

            // Flags: double-sided (0x01), ground shadow (0x02), cast shadow (0x04),
            // receive shadow (0x08), edge outline (0x10) -> 0x1F
            pmxMat.drawFlags = 0x1F;
            pmxMat.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            pmxMat.edgeSize = 1.0f;

            // Diffuse texture (or base color for glTF/PBR)
            aiString texPath;
            bool hasTexture = false;
            if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
                aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                hasTexture = true;
            } else if (aiMat->GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
                       aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
                hasTexture = true;
            }
            if (hasTexture) {
                QString pathStr = QString::fromUtf8(texPath.C_Str()).trimmed();
                if (!pathStr.isEmpty()) {
                    pathStr.replace('\\', '/');
                    int32_t texIdx = -1;
                    for (size_t t = 0; t < doc->textures.size(); ++t) {
                        if (doc->textures[t] == pathStr) {
                            texIdx = static_cast<int32_t>(t);
                            break;
                        }
                    }
                    if (texIdx == -1) {
                        texIdx = static_cast<int32_t>(doc->textures.size());
                        doc->textures.push_back(pathStr);
                    }
                    pmxMat.textureIndex = texIdx;
                }
            }

            pmxMat.sphereTextureIndex = -1;
            pmxMat.sphereMode = 0;
            pmxMat.sharedToonFlag = 0;
            pmxMat.toonIndex = -1;
            pmxMat.memo = QString();
            pmxMat.faceVertexCount = 0;

            convertedMaterials.push_back(std::move(pmxMat));
        }
    }

    if (convertedMaterials.empty()) {
        convertedMaterials.push_back(createDefaultWhiteMaterial());
    }

    std::vector<std::vector<int32_t>> materialTriangles(convertedMaterials.size());

    // Process meshes and vertices
    if (scene->mMeshes != nullptr) {
        for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx) {
            const aiMesh* mesh = scene->mMeshes[meshIdx];
            if (mesh == nullptr || mesh->mVertices == nullptr || mesh->mNumVertices == 0) {
                continue;
            }

            // Skip meshes without triangle primitives
            if ((mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0 && mesh->mNumFaces > 0) {
                continue;
            }

            const int32_t baseVertex = static_cast<int32_t>(doc->vertices.size());
            doc->vertices.reserve(doc->vertices.size() + mesh->mNumVertices);

            for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
                PmxVertex vert;
                vert.position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z) * options.scale;

                if (mesh->HasNormals() && mesh->mNormals != nullptr) {
                    glm::vec3 n(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
                    const float len = glm::length(n);
                    if (len > 1e-6f) {
                        n /= len;
                    } else {
                        n = glm::vec3(0.0f, 1.0f, 0.0f);
                    }
                    vert.normal = n;
                } else {
                    vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                if (mesh->HasTextureCoords(0) && mesh->mTextureCoords[0] != nullptr) {
                    vert.uv = glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
                } else {
                    vert.uv = glm::vec2(0.0f, 0.0f);
                }

                vert.weightType = WeightType::BDEF1;
                if (options.createRootBone) {
                    vert.boneIndices = {0, -1, -1, -1};
                    vert.boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
                } else {
                    vert.boneIndices = {-1, -1, -1, -1};
                    vert.boneWeights = {0.0f, 0.0f, 0.0f, 0.0f};
                }
                vert.edgeScale = 1.0f;

                doc->vertices.push_back(vert);
            }

            // Determine material index for this mesh
            unsigned int matIdx = mesh->mMaterialIndex;
            if (matIdx >= convertedMaterials.size()) {
                matIdx = static_cast<unsigned int>(convertedMaterials.size());
                convertedMaterials.push_back(createDefaultWhiteMaterial());
                materialTriangles.resize(convertedMaterials.size());
            }

            // Accumulate face indices
            if (mesh->mFaces != nullptr) {
                for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                    const aiFace& face = mesh->mFaces[f];
                    if (face.mNumIndices == 3 && face.mIndices != nullptr) {
                        materialTriangles[matIdx].push_back(baseVertex + static_cast<int32_t>(face.mIndices[0]));
                        materialTriangles[matIdx].push_back(baseVertex + static_cast<int32_t>(face.mIndices[1]));
                        materialTriangles[matIdx].push_back(baseVertex + static_cast<int32_t>(face.mIndices[2]));
                    }
                }
            }
        }
    }

    // Partition material faceVertexCount and build doc->faceIndices
    doc->materials.clear();
    for (size_t m = 0; m < convertedMaterials.size(); ++m) {
        const auto& tris = materialTriangles[m];
        if (!tris.empty()) {
            convertedMaterials[m].faceVertexCount = static_cast<int32_t>(tris.size());
            doc->faceIndices.insert(doc->faceIndices.end(), tris.begin(), tris.end());
            doc->materials.push_back(std::move(convertedMaterials[m]));
        }
    }

    if (doc->materials.empty()) {
        PmxMaterial def = createDefaultWhiteMaterial();
        def.faceVertexCount = 0;
        doc->materials.push_back(std::move(def));
    }

    // Root bone
    if (options.createRootBone) {
        PmxBone rootBone;
        rootBone.nameJp = options.rootBoneNameJp.isEmpty() ? QString::fromUtf8("全ての親") : options.rootBoneNameJp;
        rootBone.nameEn = options.rootBoneNameEn.isEmpty() ? QStringLiteral("Root") : options.rootBoneNameEn;
        rootBone.position = glm::vec3(0.0f, 0.0f, 0.0f);
        rootBone.parentBoneIndex = -1;
        rootBone.deformTier = 0;
        rootBone.boneFlags = 0x001E; // Rotatable, Translatable, Visible, Enabled
        rootBone.tailOffset = glm::vec3(0.0f, 0.0f, 0.0f);
        rootBone.tailBoneIndex = -1;
        doc->bones.push_back(std::move(rootBone));
    }

    // Default display frames
    PmxDisplayFrame rootFrame;
    rootFrame.nameJp = QStringLiteral("Root");
    rootFrame.nameEn = QStringLiteral("Root");
    rootFrame.specialFlag = 1;
    if (options.createRootBone && !doc->bones.empty()) {
        rootFrame.elements.push_back(PmxDisplayElement{0, 0});
    }
    doc->displayFrames.push_back(std::move(rootFrame));

    PmxDisplayFrame expFrame;
    expFrame.nameJp = QString::fromUtf8("表情");
    expFrame.nameEn = QStringLiteral("Exp");
    expFrame.specialFlag = 1;
    doc->displayFrames.push_back(std::move(expFrame));

    // PMX Header
    doc->header.version = 2.0f;
    doc->header.textEncoding = 1; // UTF-8
    doc->header.additionalVec4Count = 0;
    doc->header.preserveIndexSizesOnSave = false;

    doc->header.vertexIndexSize = calcVertexIndexSize(doc->vertices.size());
    doc->header.textureIndexSize = calcNonVertexIndexSize(doc->textures.size());
    doc->header.materialIndexSize = calcNonVertexIndexSize(doc->materials.size());
    doc->header.boneIndexSize = calcNonVertexIndexSize(doc->bones.size());
    doc->header.morphIndexSize = calcNonVertexIndexSize(doc->morphs.size());
    doc->header.rigidBodyIndexSize = calcNonVertexIndexSize(doc->rigidBodies.size());

    return doc;
}

} // namespace

std::unique_ptr<PmxDocument> AssimpImporter::importFile(
    const QString& filePath,
    const AssimpImportOptions& options,
    QString* errorMessage)
{
    if (filePath.isEmpty()) {
        setError(errorMessage, QStringLiteral("File path is empty"));
        return nullptr;
    }

    Assimp::Importer importer;
    const unsigned int flags = buildPostProcessFlags(options);

    const aiScene* scene = importer.ReadFile(filePath.toUtf8().constData(), flags);
    if (scene == nullptr) {
        setError(errorMessage, QString::fromUtf8(importer.GetErrorString()));
        return nullptr;
    }

    return processScene(scene, options, filePath, errorMessage);
}

std::unique_ptr<PmxDocument> AssimpImporter::importFromMemory(
    const void* data,
    size_t length,
    const QString& hintFormat,
    const AssimpImportOptions& options,
    QString* errorMessage)
{
    if (data == nullptr || length == 0) {
        setError(errorMessage, QStringLiteral("Input data buffer is empty or null"));
        return nullptr;
    }

    Assimp::Importer importer;
    const unsigned int flags = buildPostProcessFlags(options);

    QByteArray hintBytes = hintFormat.toLatin1();
    const char* hintPtr = hintBytes.isEmpty() ? "obj" : hintBytes.constData();

    const aiScene* scene = importer.ReadFileFromMemory(data, length, flags, hintPtr);
    if (scene == nullptr) {
        setError(errorMessage, QString::fromUtf8(importer.GetErrorString()));
        return nullptr;
    }

    return processScene(scene, options, QString(), errorMessage);
}

} // namespace pmx
