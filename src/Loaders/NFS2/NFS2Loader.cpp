#include "NFS2Loader.h"

#include <array>

using namespace LibOpenNFS::NFS2;

template class NFS2Loader<PS1>;
template class NFS2Loader<PC>;

template <typename Platform>
std::shared_ptr<Car> NFS2Loader<Platform>::LoadCar(const std::string &carBasePath, NFSVer nfsVersion)
{
    boost::filesystem::path p(carBasePath);
    std::string carName = p.filename().string();

    std::stringstream geo_path, psh_path, qfs_path, car_out_path;
    geo_path << carBasePath << ".GEO";
    psh_path << carBasePath << ".PSH";
    qfs_path << carBasePath << ".QFS";

    // For every file in here that's a BMP, load the data into a Texture object. This lets us easily access textures by an ID.
    std::map<unsigned int, Texture> carTextures;
    std::map<std::string, uint32_t> remappedTextureIds;
    uint32_t remappedTextureID = 0;

    /*switch (version)
    {
    case NFS_3_PS1:
    case NFS_2_PS1:
        car_out_path << CAR_PATH << ToString(version) << "/" << car_name << "/";
        ImageLoader::ExtractPSH(psh_path.str(), car_out_path.str());
        break;
    case NFS_2:
    case NFS_2_SE:
        car_out_path << CAR_PATH << ToString(version) << "/" << car_name << "/";
        ImageLoader::ExtractQFS(qfs_path.str(), car_out_path.str());
        break;
    default:
        ASSERT(false, "I shouldn't be loading this version (" << version << ") and you know it.");
    }

    for (boost::filesystem::directory_iterator itr(car_out_path.str()); itr != boost::filesystem::directory_iterator(); ++itr)
    {
        if (itr->path().filename().string().find("BMP") != std::string::npos && itr->path().filename().string().find("-a") == std::string::npos)
        {
            // Map texture names, strings, into numbers so I can use them for indexes into the eventual Texture Array
            remapped_texture_ids[itr->path().filename().replace_extension("").string()] = remappedTextureID++;
            switch (version)
            {
            case NFS_3_PS1:
            case NFS_2_PS1:
                GLubyte *data;
                GLsizei width;
                GLsizei height;
                ASSERT(ImageLoader::LoadBmpCustomAlpha(itr->path().string().c_str(), &data, &width, &height, 0u),
                       "Texture " << itr->path().string() << " did not load succesfully!");
                car_textures[remapped_texture_ids[itr->path().filename().replace_extension("").string()]] =
                  Texture(remapped_texture_ids[itr->path().filename().replace_extension("").string()], data, static_cast<unsigned int>(width), static_cast<unsigned int>(height));
                break;
            case NFS_2:
            case NFS_2_SE:
                bmpread_t bmpAttr; // This will leak.
                ASSERT(bmpread(itr->path().string().c_str(), BMPREAD_ANY_SIZE | BMPREAD_ALPHA, &bmpAttr), "Texture " << itr->path().string() << " did not load succesfully!");
                car_textures[remapped_texture_ids[itr->path().filename().replace_extension("").string()]] =
                  Texture(remapped_texture_ids[itr->path().filename().replace_extension("").string()],
                          bmpAttr.data,
                          static_cast<unsigned int>(bmpAttr.width),
                          static_cast<unsigned int>(bmpAttr.height));
                break;
            default:
                ASSERT(false, "I shouldn't be loading this version (" << version << ") and you know it.");
            }
        }
    }*/

    // This must run before geometry load, as it will affect UV's
    GLint textureArrayID = Texture::MakeTextureArray(carTextures, false);
    CarData carData;
    // carData.meshes = LoadGEO(geo_path.str(), car_textures, remapped_texture_ids);
    return std::make_shared<Car>(carData, NFSVer::NFS_2, carName, textureArrayID);
}

template <>
std::shared_ptr<Track> NFS2Loader<PC>::LoadTrack(const std::string &trackBasePath, NFSVer nfsVersion)
{
    LOG(INFO) << "Loading Track located at " << trackBasePath;

    auto track        = std::make_shared<Track>(Track());
    track->nfsVersion = nfsVersion;

    boost::filesystem::path p(trackBasePath);
    track->name = p.filename().string();
    std::string trkPath, colPath, canPath;

    trkPath = trackBasePath + ".TRK";
    colPath = trackBasePath + ".COL";
    switch (track->nfsVersion)
    {
    case NFS_2:
    case NFS_2_SE:
        canPath = RESOURCE_PATH + ToString(track->nfsVersion) + "/GAMEDATA/TRACKS/DATA/PC/" + track->name + "00.CAN";
        break;
    case NFS_2_PS1:
        canPath = trackBasePath + "00.CAN";
        break;
    default:
        ASSERT(false, "Attempting to load unknown NFS version with NFS2 PC Track parser");
    }

    TrkFile<PC> trkFile;
    ColFile<PC> colFile;
    CanFile canFile;

    ASSERT(Texture::ExtractTrackTextures(trackBasePath, track->name, track->nfsVersion), "Could not extract " << track->name << " texture pack.");
    ASSERT(CanFile::Load(canPath, canFile), "Could not load CAN file (camera animation): " << canPath);     // Load camera intro/outro animation data
    ASSERT(TrkFile<PC>::Load(trkPath, trkFile, track->nfsVersion), "Could not load TRK file: " << trkPath); // Load TRK file to get track block specific data
    ASSERT(ColFile<PC>::Load(colPath, colFile, track->nfsVersion), "Could not load COL file: " << colPath); // Load Catalogue file to get global (non block specific) data

    // Load up the textures
    auto textureBlock = colFile.GetExtraObjectBlock(ExtraBlockID::TEXTURE_BLOCK_ID);
    for (uint32_t texIdx = 0; texIdx < textureBlock.nTextures; texIdx++)
    {
        track->textureMap[textureBlock.polyToQfsTexTable[texIdx].texNumber] = Texture::LoadTexture(track->nfsVersion, textureBlock.polyToQfsTexTable[texIdx], track->name);
    }

    track->textureArrayID  = Texture::MakeTextureArray(track->textureMap, false);
    track->nBlocks         = trkFile.nBlocks;
    track->cameraAnimation = canFile.animPoints;
    track->trackBlocks     = _ParseTRKModels(trkFile, colFile, track);
    track->globalObjects   = _ParseCOLModels(colFile, track);
    track->virtualRoad     = _ParseVirtualRoad(trkFile);

    LOG(INFO) << "Track loaded successfully";

    return track;
}

template <>
std::shared_ptr<Track> NFS2Loader<PS1>::LoadTrack(const std::string &trackBasePath, NFSVer nfsVersion)
{
    LOG(INFO) << "Loading Track located at " << trackBasePath;
    auto track        = std::make_shared<Track>(Track());
    track->nfsVersion = nfsVersion;

    boost::filesystem::path p(trackBasePath);
    track->name = p.filename().string();
    std::string trkPath, colPath;

    trkPath = trackBasePath + ".TRK";
    colPath = trackBasePath + ".COL";
    colPath.replace(colPath.find("ZZ"), 2, "");

    TrkFile<PS1> trkFile;
    ColFile<PS1> colFile;

    ASSERT(Texture::ExtractTrackTextures(trackBasePath, track->name, track->nfsVersion), "Could not extract " << track->name << " texture pack.");
    ASSERT(TrkFile<PS1>::Load(trkPath, trkFile, track->nfsVersion), "Could not load TRK file: " << trkPath); // Load TRK file to get track block specific data
    ASSERT(ColFile<PS1>::Load(colPath, colFile, track->nfsVersion), "Could not load COL file: " << colPath); // Load Catalogue file to get global (non block specific) data

    // Load up the textures
    auto textureBlock = colFile.GetExtraObjectBlock(ExtraBlockID::TEXTURE_BLOCK_ID);
    for (uint32_t texIdx = 0; texIdx < textureBlock.nTextures; texIdx++)
    {
        track->textureMap[textureBlock.polyToQfsTexTable[texIdx].texNumber] = Texture::LoadTexture(track->nfsVersion, textureBlock.polyToQfsTexTable[texIdx], track->name);
    }

    track->textureArrayID = Texture::MakeTextureArray(track->textureMap, false);
    track->nBlocks        = trkFile.nBlocks;
    track->trackBlocks    = _ParseTRKModels(trkFile, colFile, track);
    track->globalObjects  = _ParseCOLModels(colFile, track);
    track->virtualRoad    = _ParseVirtualRoad(trkFile);

    LOG(INFO) << "Track loaded successfully";

    return track;
}

// template <typename Platform>
// CarData NFS2Loader<Platform>::_ParseGEOModels(const GeoFile<Platform> &geoFile)
// {
//     ASSERT(false, "Unimplemented");
//     return CarData();
// }

// One might question why a TRK parsing function requires the COL file too. Simples, we need XBID 2 for Texture remapping during ONFS texgen.
template <typename Platform>
std::vector<OpenNFS::TrackBlock> NFS2Loader<Platform>::_ParseTRKModels(const TrkFile<Platform> &trkFile, ColFile<Platform> &colFile, const std::shared_ptr<Track> &track)
{
    LOG(INFO) << "Parsing TRK file into ONFS GL structures";
    std::vector<OpenNFS::TrackBlock> trackBlocks;

    // Pull out a shorter reference to the texture table
    auto polyToQfsTexTable = colFile.GetExtraObjectBlock(ExtraBlockID::TEXTURE_BLOCK_ID).polyToQfsTexTable;

    // Parse out TRKBlock data
    for (const auto &superBlock : trkFile.superBlocks)
    {
        for (auto rawTrackBlock : superBlock.trackBlocks)
        {
            // Get position all vertices need to be relative to
            glm::quat orientation         = glm::normalize(glm::quat(glm::vec3(-SIMD_PI / 2, 0, 0)));
            glm::vec3 rawTrackBlockCenter = orientation * (Utils::PointToVec(trkFile.blockReferenceCoords[rawTrackBlock.serialNum]) / NFS2_SCALE_FACTOR);
            std::vector<uint32_t> trackBlockNeighbourIds;

            // Convert the neighbor int16_t's to uint32_t for OFNS trackblock representation
            if (rawTrackBlock.IsBlockPresent(ExtraBlockID::NEIGHBOUR_BLOCK_ID))
            {
                // if the numbers go beyond the track length they start back at 0, and if they drop below 0 they start back at the track length - 1
                for (auto &trackBlockNeighbourRaw : rawTrackBlock.GetExtraObjectBlock(ExtraBlockID::NEIGHBOUR_BLOCK_ID).blockNeighbours)
                {
                    trackBlockNeighbourIds.push_back(trackBlockNeighbourRaw % trkFile.nBlocks);
                }
            }

            OpenNFS::TrackBlock trackBlock(rawTrackBlock.serialNum, rawTrackBlockCenter, rawTrackBlock.serialNum, 1, trackBlockNeighbourIds);

            // Collate all available Structure References, 3 different ID types can store this information, check them all
            std::vector<StructureRefBlock> structureReferences;
            for (auto &structRefBlockId : {ExtraBlockID::STRUCTURE_REF_BLOCK_A_ID, ExtraBlockID::STRUCTURE_REF_BLOCK_B_ID, ExtraBlockID::STRUCTURE_REF_BLOCK_C_ID})
            {
                if (rawTrackBlock.IsBlockPresent(structRefBlockId))
                {
                    auto structureRefBlock = rawTrackBlock.GetExtraObjectBlock(structRefBlockId);
                    structureReferences.insert(structureReferences.end(), structureRefBlock.structureReferences.begin(), structureRefBlock.structureReferences.end());
                }
            }

            // Pull out structures from trackblock if present
            if (rawTrackBlock.IsBlockPresent(ExtraBlockID::STRUCTURE_BLOCK_ID))
            {
                // Check whether there are enough struct references for how many strucutres there are for this trackblock
                if (rawTrackBlock.GetExtraObjectBlock(ExtraBlockID::STRUCTURE_BLOCK_ID).nStructures != structureReferences.size())
                {
                    LOG(WARNING) << "Trk block " << (int) rawTrackBlock.serialNum << " is missing "
                                 << rawTrackBlock.GetExtraObjectBlock(ExtraBlockID::STRUCTURE_BLOCK_ID).nStructures - structureReferences.size() << " structure locations!";
                }

                // Shorter reference to structures for trackblock
                auto structures = rawTrackBlock.GetExtraObjectBlock(ExtraBlockID::STRUCTURE_BLOCK_ID).structures;

                // Structures
                for (uint32_t structureIdx = 0; structureIdx < rawTrackBlock.GetExtraObjectBlock(ExtraBlockID::STRUCTURE_BLOCK_ID).nStructures; ++structureIdx)
                {
                    // Mesh Data
                    std::vector<uint32_t> structureVertexIndices;
                    std::vector<glm::vec2> structureUVs;
                    std::vector<uint32_t> structureTextureIndices;
                    std::vector<glm::vec3> structureVertices;
                    std::vector<glm::vec4> structureShadingData;
                    std::vector<glm::vec3> structureNormals;

                    // Find the structure reference that matches this structure, else use block default
                    VERT_HIGHP structureReferenceCoordinates = trkFile.blockReferenceCoords[rawTrackBlock.serialNum];
                    bool refCoordsFound                      = false;

                    for (auto &structureReference : structureReferences)
                    {
                        // Only check fixed type structure references
                        if (structureReference.structureRef == structureIdx)
                        {
                            if (structureReference.recType == 1 || structureReference.recType == 4)
                            {
                                structureReferenceCoordinates = structureReference.refCoordinates;
                                refCoordsFound                = true;
                                break;
                            }
                            else if (structureReference.recType == 3)
                            {
                                // For now, if animated, use position 0 of animation sequence
                                structureReferenceCoordinates = structureReference.animationData[0].position;
                                refCoordsFound                = true;
                                break;
                            }
                        }
                    }
                    if (!refCoordsFound)
                    {
                        LOG(WARNING) << "Couldn't find a reference coordinate for Structure " << structureIdx << " in TB" << rawTrackBlock.serialNum;
                    }
                    for (uint16_t vertIdx = 0; vertIdx < structures[structureIdx].nVerts; ++vertIdx)
                    {
                        structureVertices.emplace_back(orientation * ((256.f * Utils::PointToVec(structures[structureIdx].vertexTable[vertIdx])) / NFS2_SCALE_FACTOR));
                        structureShadingData.emplace_back(glm::vec4(1.0, 1.0f, 1.0f, 1.0f));
                    }
                    for (uint32_t polyIdx = 0; polyIdx < structures[structureIdx].nPoly; ++polyIdx)
                    {
                        // Remap the COL TextureID's using the COL texture block (XBID2)
                        TEXTURE_BLOCK polygonTexture = polyToQfsTexTable[structures[structureIdx].polygonTable[polyIdx].texture];
                        Texture glTexture            = track->textureMap[polygonTexture.texNumber];
                        // Convert the UV's into ONFS space, to enable tiling/mirroring etc based on NFS texture flags
                        std::vector<glm::vec2> transformedUVs = glTexture.GenerateUVs(XOBJ, polygonTexture.alignmentData, polygonTexture);
                        structureUVs.insert(structureUVs.end(), transformedUVs.begin(), transformedUVs.end());

                        // Calculate the normal, as no provided data
                        glm::vec3 normal = Utils::CalculateQuadNormal(verts[structures[structureIdx].polygonTable[polyIdx].vertex[0]],
                                                                      verts[structures[structureIdx].polygonTable[polyIdx].vertex[1]],
                                                                      verts[structures[structureIdx].polygonTable[polyIdx].vertex[2]],
                                                                      verts[structures[structureIdx].polygonTable[polyIdx].vertex[3]]);

                        // Two triangles per raw quad, hence 6 vertices. Normal data and texture index required per-vertex.
                        for (auto &quadToTriVertNumber : quadToTriVertNumbers)
                        {
                            structureNormals.emplace_back(normal);
                            structureVertexIndices.emplace_back(structures[structureIdx].polygonTable[polyIdx].vertex[quadToTriVertNumber]);
                            structureTextureIndices.emplace_back(polygonTexture.texNumber);
                        }
                    }

                    TrackModel structureModel = TrackModel(structureVertices,
                                                           structureNormals,
                                                           structureUVs,
                                                           structureTextureIndices,
                                                           structureVertexIndices,
                                                           structureShadingData,
                                                           orientation * (Utils::PointToVec(structureReferenceCoordinates) / NFS2_SCALE_FACTOR));
                    Entity trackBlockEntity   = Entity(rawTrackBlock.serialNum, structureIdx, track->nfsVersion, OBJ_POLY, structureModel, 0);
                    trackBlock.objects.emplace_back(trackBlockEntity);
                }
            }

            // Track Mesh Data
            std::vector<uint32_t> trackBlockVertexIndices;
            std::vector<glm::vec2> trackBlockUVs;
            std::vector<uint32_t> trackBlockTextureIndices;
            std::vector<glm::vec3> trackBlockVertices;
            std::vector<glm::vec4> trackBlockShadingData;
            std::vector<glm::vec3> trackBlockNormals;

            // Base Track Geometry
            VERT_HIGHP blockRefCoord = {};

            for (int32_t vertIdx = 0; vertIdx < rawTrackBlock.nStickToNextVerts + rawTrackBlock.nHighResVert; vertIdx++)
            {
                if (vertIdx < rawTrackBlock.nStickToNextVerts)
                {
                    // If in last block go get ref coord of first block, else get ref of next block
                    blockRefCoord = (rawTrackBlock.serialNum == track->nBlocks - 1) ? trkFile.blockReferenceCoords[0] : trkFile.blockReferenceCoords[rawTrackBlock.serialNum + 1];
                }
                else
                {
                    blockRefCoord = trkFile.blockReferenceCoords[rawTrackBlock.serialNum];
                }

                trackBlockVertices.emplace_back(orientation *
                                                (((Utils::PointToVec(blockRefCoord) + (256.f * Utils::PointToVec(rawTrackBlock.vertexTable[vertIdx]))) / NFS2_SCALE_FACTOR)));
                trackBlockShadingData.emplace_back(glm::vec4(1.0, 1.0f, 1.0f, 1.0f));
            }
            for (int32_t polyIdx = (rawTrackBlock.nLowResPoly + rawTrackBlock.nMedResPoly);
                 polyIdx < (rawTrackBlock.nLowResPoly + rawTrackBlock.nMedResPoly + rawTrackBlock.nHighResPoly);
                 ++polyIdx)
            {
                // Remap the COL TextureID's using the COL texture block (XBID2)
                TEXTURE_BLOCK polygonTexture = polyToQfsTexTable[rawTrackBlock.polygonTable[polyIdx].texture];
                Texture glTexture            = track->textureMap[polygonTexture.texNumber];
                // Convert the UV's into ONFS space, to enable tiling/mirroring etc based on NFS texture flags
                std::vector<glm::vec2> transformedUVs = glTexture.GenerateUVs(ROAD, polygonTexture.alignmentData, polygonTexture);
                trackBlockUVs.insert(trackBlockUVs.end(), transformedUVs.begin(), transformedUVs.end());
                // Calculate the normal, as no provided data
                glm::vec3 normal = Utils::CalculateQuadNormal(trackBlockVertices[rawTrackBlock.polygonTable[polyIdx].vertex[0]],
                                                              trackBlockVertices[rawTrackBlock.polygonTable[polyIdx].vertex[1]],
                                                              trackBlockVertices[rawTrackBlock.polygonTable[polyIdx].vertex[2]],
                                                              trackBlockVertices[rawTrackBlock.polygonTable[polyIdx].vertex[3]]);

                // Two triangles per raw quad, hence 6 vertices. Normal data and texture index required per-vertex.
                for (auto &quadToTriVertNumber : quadToTriVertNumbers)
                {
                    trackBlockNormals.emplace_back(normal);
                    trackBlockVertexIndices.emplace_back(rawTrackBlock.polygonTable[polyIdx].vertex[quadToTriVertNumber]);
                    trackBlockTextureIndices.emplace_back(polygonTexture.texNumber);
                }
            }

            TrackModel trackBlockModel(trackBlockVertices, trackBlockNormals, trackBlockUVs, trackBlockTextureIndices, trackBlockVertexIndices, trackBlockShadingData, glm::vec3());
            Entity trackBlockEntity = Entity(rawTrackBlock.serialNum, rawTrackBlock.serialNum, track->nfsVersion, ROAD, trackBlockModel, 0);
            trackBlock.track.push_back(trackBlockEntity);

            // Add the parsed ONFS trackblock to the list of trackblocks
            trackBlocks.push_back(trackBlock);
        }
    }
    return trackBlocks;
}

template <typename Platform>
std::vector<VirtualRoad> NFS2Loader<Platform>::_ParseVirtualRoad(const TrkFile<Platform> &trkFile)
{
    std::vector<VirtualRoad> virtualRoad;

    for (const auto &superBlock : trkFile.superBlocks)
    {
        for (auto rawTrackBlock : superBlock.trackBlocks)
        {
            if (!rawTrackBlock.IsBlockPresent(ExtraBlockID::VROAD_BLOCK_ID))
            {
                LOG(WARNING) << "Trackblock: " << rawTrackBlock.serialNum << " is missing virtual road data.";
                continue;
            }

            if (trkFile.version == NFS_2_PS1)
            {
                std::vector<LibOpenNFS::NFS2::VROAD_VEC> vroadVectors = rawTrackBlock.GetExtraObjectBlock(ExtraBlockID::VROAD_BLOCK_ID).ps1VroadData;

                for (auto &vroadVector : vroadVectors)
                {
                    // Get VROAD forward vector, and fake a normal + right vector
                    glm::vec3 forward = Utils::PointToVec(vroadVector);
                    glm::vec3 normal  = glm::normalize(glm::quat(glm::vec3(0, -SIMD_PI / 2, 0))) * forward;
                    glm::vec3 right   = glm::normalize(glm::quat(glm::vec3(-SIMD_PI / 2, 0, 0))) * forward;

                    glm::vec3 leftWall  = (2.f) * normal;
                    glm::vec3 rightWall = (2.f) * normal;

                    virtualRoad.push_back(VirtualRoad(glm::vec3(0, 0, 0), 0, normal, forward, right, leftWall, rightWall));
                }
            }
            else
            {
                std::vector<LibOpenNFS::NFS2::VROAD> vroadData = rawTrackBlock.GetExtraObjectBlock(ExtraBlockID::VROAD_BLOCK_ID).vroadData;

                for (auto &vroadEntry : vroadData)
                {
                    // Get VROAD right vector
                    glm::vec3 forward = Utils::PointToVec(vroadEntry.forwardVec);
                    glm::vec3 normal  = Utils::PointToVec(vroadEntry.normalVec) / 128.f;
                    // TODO: Generate a vector 90 degs from forward
                    glm::vec3 right = glm::vec3(0, 0, 0);

                    glm::vec3 leftWall  = (2.f) * normal;
                    glm::vec3 rightWall = (2.f) * normal;

                    virtualRoad.push_back(VirtualRoad(glm::vec3(0, 0, 0), 0, normal, forward, right, leftWall, rightWall));
                }
            }
        }
    }

    return virtualRoad;
}

template <typename Platform>
std::vector<Entity> NFS2Loader<Platform>::_ParseCOLModels(ColFile<Platform> &colFile, const std::shared_ptr<Track> &track)
{
    LOG(INFO) << "Parsing COL file into ONFS GL structures";
    std::vector<Entity> colEntities;

    // All Vertices are stored so that the model is rotated 90 degs on X. Remove this at Vert load time.
    glm::quat orientation = glm::normalize(glm::quat(glm::vec3(-SIMD_PI / 2, 0, 0)));

    // Shorter reference to structures and texture table
    auto structures        = colFile.GetExtraObjectBlock(ExtraBlockID::STRUCTURE_BLOCK_ID).structures;
    auto polyToQfsTexTable = colFile.GetExtraObjectBlock(ExtraBlockID::TEXTURE_BLOCK_ID).polyToQfsTexTable;

    // Parse out COL data
    for (uint32_t structureIdx = 0; structureIdx < colFile.GetExtraObjectBlock(ExtraBlockID::STRUCTURE_BLOCK_ID).nStructures; ++structureIdx)
    {
        std::vector<uint32_t> globalStructureVertexIndices;
        std::vector<glm::vec2> globalStructureUVs;
        std::vector<uint32_t> globalStructureTextureIndices;
        std::vector<glm::vec3> globalStructureVertices;
        std::vector<glm::vec4> globalStructureShadingData;
        std::vector<glm::vec3> globalStructureNormals;

        VERT_HIGHP structureReferenceCoordinates = {};
        bool refCoordsFound                      = false;
        // Find the structure reference that matches this structure
        for (auto &structure : colFile.GetExtraObjectBlock(ExtraBlockID::STRUCTURE_REF_BLOCK_A_ID).structureReferences)
        {
            // Only check fixed type structure references
            if (structure.structureRef == structureIdx)
            {
                if (structure.recType == 1 || structure.recType == 4)
                {
                    structureReferenceCoordinates = structure.refCoordinates;
                    refCoordsFound                = true;
                    break;
                }
                else if (structure.recType == 3)
                {
                    // For now, if animated, use position 0 of animation sequence
                    structureReferenceCoordinates = structure.animationData[0].position;
                    refCoordsFound                = true;
                    break;
                }
            }
        }
        if (!refCoordsFound)
        {
            LOG(WARNING) << "Couldn't find a reference coordinate for Structure " << structureIdx << " in COL file";
        }
        for (uint16_t vertIdx = 0; vertIdx < structures[structureIdx].nVerts; ++vertIdx)
        {
            globalStructureVertices.emplace_back(orientation * ((256.f * Utils::PointToVec(structures[structureIdx].vertexTable[vertIdx])) / NFS2_SCALE_FACTOR));
            globalStructureShadingData.emplace_back(glm::vec4(1.0, 1.0f, 1.0f, 1.0f));
        }

        for (uint32_t polyIdx = 0; polyIdx < structures[structureIdx].nPoly; ++polyIdx)
        {
            // Remap the COL TextureID's using the COL texture block (XBID2)
            TEXTURE_BLOCK polygonTexture = polyToQfsTexTable[structures[structureIdx].polygonTable[polyIdx].texture];
            Texture glTexture            = track->textureMap[polygonTexture.texNumber];
            // Calculate the normal, as no provided data
            glm::vec3 normal = Utils::CalculateQuadNormal(globalStructureVertices[structures[structureIdx].polygonTable[polyIdx].vertex[0]],
                                                          globalStructureVertices[structures[structureIdx].polygonTable[polyIdx].vertex[1]],
                                                          globalStructureVertices[structures[structureIdx].polygonTable[polyIdx].vertex[2]],
                                                          globalStructureVertices[structures[structureIdx].polygonTable[polyIdx].vertex[3]]);

            // TODO: Use textures alignment data to modify these UV's
            globalStructureUVs.emplace_back(1.0f * glTexture.maxU, 1.0f * glTexture.maxV);
            globalStructureUVs.emplace_back(0.0f * glTexture.maxU, 1.0f * glTexture.maxV);
            globalStructureUVs.emplace_back(0.0f * glTexture.maxU, 0.0f * glTexture.maxV);
            globalStructureUVs.emplace_back(1.0f * glTexture.maxU, 1.0f * glTexture.maxV);
            globalStructureUVs.emplace_back(0.0f * glTexture.maxU, 0.0f * glTexture.maxV);
            globalStructureUVs.emplace_back(1.0f * glTexture.maxU, 0.0f * glTexture.maxV);

            // Two triangles per raw quad, hence 6 vertices. Normal data and texture index required per-vertex.
            for (auto &quadToTriVertNumber : quadToTriVertNumbers)
            {
                globalStructureNormals.push_back(normal);
                globalStructureVertexIndices.push_back(structures[structureIdx].polygonTable[polyIdx].vertex[quadToTriVertNumber]);
                globalStructureTextureIndices.push_back(polygonTexture.texNumber);
            }
        }

        glm::vec3 position = orientation * (Utils::PointToVec(structureReferenceCoordinates) / NFS2_SCALE_FACTOR);
        TrackModel globalStructureModel(globalStructureVertices, globalStructureNormals, globalStructureUVs, globalStructureTextureIndices, globalStructureVertexIndices,
                                        globalStructureShadingData, position);
        colEntities.emplace_back(Entity(0, structureIdx, NFS_2, GLOBAL, globalStructureModel, 0));
    }

    return colEntities;
}
