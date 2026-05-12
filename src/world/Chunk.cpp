#include "Chunk.h"
#include "ChunkManager.h"

Chunk::Chunk(ChunkManager& cm, ChunkPos& cp):
    cm(cm),
    cp(cp),
    // 0: Normal buffer management with GL_STREAM_DRAW (dynamically decide buffer sizes.)
    // 1: Utilize glMapBufferRange to then memcopy to memory. (predefine buffer sizes.)
    // 2: Buffer orphaning and then glBufferSubData. (predefine buffer sizes.)
    bufferUpdateMethod(0), // set to either 0,1, or 2;
    modifierUpdateQueue()
    {
    // intiialize chunk to zeros.
    occupancyInts = std::vector<uint32_t>(cm.occupancyXsize*cm.occupancyYsize*cm.occupancyZsize, 0); 
    // intialize const for world position of chunk.
    worldcp = glm::vec3(cp.x*cm.chunkSizeMeters, 
                        cp.y*cm.chunkSizeMeters, 
                        cp.z*cm.chunkSizeMeters);
    // sanity check for update method.
    assert(bufferUpdateMethod >= 0 && bufferUpdateMethod <= 2);
}

void Chunk::queueModifier(const std::shared_ptr<IChunkModifier> &modifier)
{
    modifierUpdateQueue.push_back(modifier);
}

int Chunk::getLocalVoxelSizeX() const
{
    return cm.occupancyXsize * 32;
}

int Chunk::getLocalVoxelSizeY() const
{
    return cm.occupancyYsize;
}

int Chunk::getLocalVoxelSizeZ() const
{
    return cm.occupancyZsize;
}

bool Chunk::isLocalInBounds(int x, int y, int z) const
{
    return x >= 0 && x < cm.occupancyXsize * 32 &&
        y >= 0 && y < cm.occupancyYsize &&
        z >= 0 && z < cm.occupancyZsize;
}

bool Chunk::isOccupiedLocal(int x, int y, int z) const
{
    if (!isLocalInBounds(x, y, z)) {
        return false;
    }

    const int intX = x / 32;
    const int bit = 31 - (x % 32);
    const int occupancyIndex = z * cm.occupancyXsize * cm.occupancyYsize +
        y * cm.occupancyXsize + intX;
    return (occupancyInts[occupancyIndex] & (1u << bit)) != 0u;
}

void Chunk::setOccupiedLocal(int x, int y, int z, bool occupied)
{
    if (!isLocalInBounds(x, y, z)) {
        return;
    }

    const int intX = x / 32;
    const int bit = 31 - (x % 32);
    const int occupancyIndex = z * cm.occupancyXsize * cm.occupancyYsize +
        y * cm.occupancyXsize + intX;
    const uint32_t mask = (1u << bit);

    if (occupied) {
        occupancyInts[occupancyIndex] |= mask;
    } else {
        occupancyInts[occupancyIndex] &= ~mask;
    }
}

void Chunk::markMaterialDirtyCell(int x, int y, int z)
{
    if (!materialDirty_) {
        materialDirty_ = true;
        materialDirtyMin_ = glm::ivec3(x, y, z);
        materialDirtyMax_ = glm::ivec3(x, y, z);
        return;
    }

    materialDirtyMin_ = glm::min(materialDirtyMin_, glm::ivec3(x, y, z));
    materialDirtyMax_ = glm::max(materialDirtyMax_, glm::ivec3(x, y, z));
}

void Chunk::setMaterialLocal(int x, int y, int z, uint8_t materialID)
{
    if (!isLocalInBounds(x, y, z) || cTexData.empty()) {
        return;
    }

    const int texX = x / 2;
    const int texY = y / 2;
    const int texZ = z / 2;
    const int textureSize = cm.chunkSizeInts * 16;
    const int texIndex = texZ * textureSize * textureSize +
        texY * textureSize + texX;
    cTexData[texIndex] = materialID;
    markMaterialDirtyCell(texX, texY, texZ);
}

void Chunk::uploadDirtyMaterialRegion()
{
    if (!materialDirty_ || cTexData.empty()) {
        return;
    }

    const int textureSize = cm.chunkSizeInts * 16;
    const int width = materialDirtyMax_.x - materialDirtyMin_.x + 1;
    const int height = materialDirtyMax_.y - materialDirtyMin_.y + 1;
    const int depth = materialDirtyMax_.z - materialDirtyMin_.z + 1;
    std::vector<uint8_t> region(static_cast<size_t>(width * height * depth));

    for (int dz = 0; dz < depth; dz++) {
        for (int dy = 0; dy < height; dy++) {
            for (int dx = 0; dx < width; dx++) {
                const int srcX = materialDirtyMin_.x + dx;
                const int srcY = materialDirtyMin_.y + dy;
                const int srcZ = materialDirtyMin_.z + dz;
                const int srcIndex = srcZ * textureSize * textureSize +
                    srcY * textureSize + srcX;
                const int dstIndex = dz * width * height + dy * width + dx;
                region[dstIndex] = cTexData[srcIndex];
            }
        }
    }

    glBindTexture(GL_TEXTURE_3D, cTexID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage3D(GL_TEXTURE_3D, 0,
        materialDirtyMin_.x, materialDirtyMin_.y, materialDirtyMin_.z,
        width, height, depth,
        GL_RED_INTEGER, GL_UNSIGNED_BYTE, region.data());

    materialDirty_ = false;
}

void Chunk::generate(){
    // Color Things
    int textureSize = cm.chunkSizeInts*16;
    float colorScale = 255.0f / (textureSize - 1);
    cTexData = std::vector<uint8_t>(textureSize*textureSize*textureSize);
    uint8_t* ptr = cTexData.data();
    int change = 0;
    

    int yxOffset = cm.occupancyXsize*cm.occupancyYsize;
    for (int z=0; z<cm.occupancyZsize; z++)
    {
        change++;
        for (int y=0; y<cm.occupancyYsize; y++)
        {
            for (int x=0; x<cm.occupancyXsize; x++)
            {
                int occupancyIndex = z * yxOffset + y * cm.occupancyXsize + x;
                uint32_t& occupancyInt = occupancyInts[occupancyIndex];

                glm::vec3 voxPosCenter = glm::vec3(
                    x*32*cm.voxSizeMeters+0.5*cm.voxSizeMeters, // x position of the voxel.
                    y*cm.voxSizeMeters+0.5*cm.voxSizeMeters, // y position of the voxel.
                    z*cm.voxSizeMeters+0.5*cm.voxSizeMeters // z position of the voxel.
                );
                fillFloor(&occupancyInt, &voxPosCenter, x, z);

                if ((y & 1) == 0 && (z & 1) == 0)
                {
                    for (int bit = 0; bit < 32; bit += 2)
                    {
                        int voxelX = x * 32 + bit;
    
                        int texX = voxelX >> 1;
                        int texY = y >> 1;
                        int texZ = z >> 1;
    
                        int texIndex =
                            texZ * textureSize * textureSize +
                            texY * textureSize +
                            texX;
    
                        cTexData[texIndex] =
                            (y == 0) ? 0u : 1u;
                        // if (change%2==0){
                        //     cTexData[texIndex] = 0u;
                        // } else {
                        //     cTexData[texIndex] = 1u;
                        // }
                        // change++;
                    }
                }
            }
        }
    }

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, textureSize, textureSize, textureSize,
        0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, cTexData.data()
    );
    // glTextureSubImage3D(cTexID, 0, 0, 0, 0, chunkSizeVoxels, chunkSizeVoxels, chunkSizeVoxels, GL_RGBA, GL_UNSIGNED_BYTE, cTexData.data());
}

// Generate the vertex array, vertex buffer, and color buffer.
void Chunk::bindMesh()
{
    int size = 2500000;
    // VAO
    glGenVertexArrays(1, &vaoID);
    glBindVertexArray(vaoID);
    
    // Vertex Buffer
    glGenBuffers(1, &vBuffID);
    glBindBuffer(GL_ARRAY_BUFFER, vBuffID);
    if (bufferUpdateMethod == 2){
        glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STREAM_DRAW);
    }
    else if (bufferUpdateMethod == 1){
        glBufferStorage(GL_ARRAY_BUFFER, size, NULL,
            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        vPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, size,
            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    }

    // Normal Buffer
    glGenBuffers(1, &nBuffID);
    glBindBuffer(GL_ARRAY_BUFFER, nBuffID);
    if (bufferUpdateMethod == 2){
        glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STREAM_DRAW);
    }
    else if (bufferUpdateMethod == 1){
        glBufferStorage(GL_ARRAY_BUFFER, size, NULL,
            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        nPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, size,
            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    }

    // Element Buffer
    glGenBuffers(1, &eBuffID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eBuffID);
    if (bufferUpdateMethod == 2){
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, NULL, GL_STREAM_DRAW);
    }
    else if (bufferUpdateMethod == 1){
        glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, size, NULL,
            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        ePtr = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, size,
            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    }

    // Color Texture
    glGenTextures(1, &cTexID);
    glBindTexture(GL_TEXTURE_3D, cTexID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    int textureSize = cm.chunkSizeInts*16;
    // Allocate immutable storage is NOT core in 4.1, so use glTexImage3D
    glTexImage3D(
        GL_TEXTURE_3D,
        0,                  // mip level
        GL_R8UI,           // internal format
        textureSize,
        textureSize,
        textureSize,
        0,                  // border (must be 0)
        GL_RED_INTEGER,            // format
        GL_UNSIGNED_BYTE,   // type
        nullptr             // no initial data
    );



    // glCreateTextures(GL_TEXTURE_3D, 1, &cTexID);
    // glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // glBindTexture(GL_TEXTURE_3D, cTexID);
    // glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // int chunkSizeVoxels= cm.chunkSizeInts*32;
    // glTextureStorage3D(cTexID, 1, GL_RGBA8, chunkSizeVoxels, chunkSizeVoxels, chunkSizeVoxels);
}

void Chunk::updateOccupancy(){
    for (const auto &chunkMod : modifierUpdateQueue) {
        chunkMod->applyToChunk(*this, cp);
    }
    modifierUpdateQueue.clear();
    uploadDirtyMaterialRegion();
}

void Chunk::updateMesh()
{
    // Update Types
    // 0 = oringal
    // 1 = binaryMeshing
    // 2 = binaryMeshing and greedy meshing (WIP)
    int updateType = 2;
    int debugMode = 1; // 0 = off, 1 = on.
    float start;
    if (debugMode == 1) start = glfwGetTime();
    
    // Clear Buffers before generating a new mesh.
    vBuff.clear();
    eBuff.clear();  
    nBuff.clear();

    if (updateType >= 1){
        // CREATE BITMASKS

        // Allocate Memory 
        // - FUTURE: Some overhead since vectors initialize to zero.
        // - FUTURE: Possibly keep as a field variable to speed up updates.
        // - CURRENT: allocation time is about 0.0025 for Paul's computer.
                // float allocStart = glfwGetTime();

        // - BIG FUTURE CHANGEF 
        //   if (x != cm.occupancyXsize-1)
        //   if (x != 0)
        //   These checks could be optimized if make each int 30 bits of voxels and 1 padding on either side.

        // - CURRENT
        //   __builtin_clz() //count leading zeros.
        //   __builtin_ctz() //count trailing zeros.

        std::vector<uint32_t> mask = std::vector<uint32_t>(occupancyInts.size()*6);
        // get Pointers to each array.
        uint32_t *posXMask = mask.data() + occupancyInts.size()*0;
        uint32_t *negXMask = mask.data() + occupancyInts.size()*1;
        uint32_t *posYMask = mask.data() + occupancyInts.size()*2;
        uint32_t *negYMask = mask.data() + occupancyInts.size()*3;
        uint32_t *posZMask = mask.data() + occupancyInts.size()*4;
        uint32_t *negZMask = mask.data() + occupancyInts.size()*5;

        int yxOffset = cm.occupancyXsize*cm.occupancyYsize;// prevent inner calculation every iteration
        // indexes into occupancyBits
        for (int z=0; z<cm.occupancyZsize; z++)
        {
            for (int y=0; y<cm.occupancyYsize; y++)
            {
                for (int x=0; x<cm.occupancyXsize; x++)
                {
                    // Get occupancyInt
                    int occupancyIndex = z*yxOffset + y*cm.occupancyXsize + x;
                    uint32_t occupancyInt = occupancyInts[occupancyIndex];
                    if (occupancyInt == 0u) continue; // do nothing if empty.
                    // reused variables for each case.
                    uint32_t occupancyIntMod;
                    uint32_t neighbor;
                    bool carry;

                //   0 i-1   ->   i+1 +x
                    // 001 011100 100 orig
                    // 001 111001 100 shift left carry (right lmb)
                    // 001 000110 100 not
                    // 001 000100 100 and
                    // Positive X Faces
                    if (x != cm.occupancyXsize-1){ // make sure not at last x.
                        // get the next x value.
                        neighbor = (occupancyInts[occupancyIndex+1]);
                        // check next value for if the last bit of current should be culled.
                        carry = (neighbor!=0) && (0==__builtin_clz(neighbor));
                        if (carry){ // cull last bit because of neightbor
                            occupancyIntMod = (occupancyInt << 1u) | (1u); //carry one from the right
                        } else {
                            occupancyIntMod = occupancyInt << 1u;
                        }
                    } else {//if it is last x then just shift.
                        occupancyIntMod = occupancyInt << 1u;
                    }
                    // compare shifted int with current for culling. see above for example.
                    occupancyIntMod = occupancyInt & ~(occupancyIntMod);
                    posXMask[occupancyIndex] = occupancyIntMod; //capture the culling bit.

                //   0 i-1   ->   i+1 +X
                    // 001 011100 100 orig
                    // 001 001110 100 shift Right carry (left rmb)
                    // 001 110001 100 not
                    // 001 010000 100 and
                    // negative X faces
                    if (x!=0){
                        neighbor = occupancyInts[occupancyIndex-1];
                        carry = (neighbor!=0) && (0==__builtin_ctz(neighbor));
                        if (carry){
                            occupancyIntMod = (occupancyInt >> 1u) | (1u << 31u); //carry one from the left
                        } else {
                            occupancyIntMod = occupancyInt >> 1u;
                        }
                    } else {
                        occupancyIntMod = occupancyInt >> 1u;
                    }
                    occupancyIntMod = occupancyInt & ~(occupancyIntMod);
                    negXMask[occupancyIndex] = occupancyIntMod;

                    // positive Y faces
                    if (y != cm.occupancyYsize-1){
                        neighbor = occupancyInts[occupancyIndex + cm.occupancyXsize];
                        
                        occupancyIntMod = occupancyInt & ~(neighbor);
                        posYMask[occupancyIndex] = occupancyIntMod;
                    } else {
                        posYMask[occupancyIndex] = occupancyInt;
                    } 

                    // negative Y faces
                    if (y != 0){
                        neighbor = occupancyInts[occupancyIndex - cm.occupancyXsize];

                        occupancyIntMod = occupancyInt & ~(neighbor);
                        negYMask[occupancyIndex] = occupancyIntMod;
                    } else {
                        negYMask[occupancyIndex] = occupancyInt;
                    }

                    // positive Z faces
                    if (z != cm.occupancyZsize-1){
                        neighbor = occupancyInts[occupancyIndex + cm.occupancyXsize*cm.occupancyYsize];
                        
                        occupancyIntMod = occupancyInt & ~(neighbor);
                        posZMask[occupancyIndex] = occupancyIntMod;
                    } else {
                        posZMask[occupancyIndex] = occupancyInt;
                    } 

                    // negative z faces
                    if (z != 0){
                        neighbor = occupancyInts[occupancyIndex - cm.occupancyXsize*cm.occupancyYsize];

                        occupancyIntMod = occupancyInt & ~(neighbor);
                        negZMask[occupancyIndex] = occupancyIntMod;
                    } else {
                        negZMask[occupancyIndex] = occupancyInt;
                    }
                }
            }
        }
        
        // Simple Quads
        if (updateType == 1){
            // GENERATE QUADS FROM BITMASKS
            for (int z=0; z<cm.occupancyZsize; z++)
            {
                for (int y=0; y<cm.occupancyYsize; y++)
                {
                    for (int x=0; x<cm.occupancyXsize; x++)
                    {
                        // get mask ints
                        int maskIndex = z*yxOffset + y*cm.occupancyXsize + x;
                        uint32_t posXint = posXMask[maskIndex];
                        uint32_t negXint = negXMask[maskIndex];
                        uint32_t posYint = posYMask[maskIndex];
                        uint32_t negYint = negYMask[maskIndex];
                        uint32_t posZint = posZMask[maskIndex];
                        uint32_t negZint = negZMask[maskIndex];
                        
                        float xPos = x*32; // x position of the voxel.
                        float yPos = y;
                        float zPos = z;
            
                        // loops until posXint is 0
                        while (posXint){ //for every 1 bit within.
                            // counts the number of trailing zeros.
                            int bit = __builtin_ctz(posXint); // 0 to 31
                            // removes the closest trailing bit
                            posXint &= posXint-1;
                            // add face for the voxel in the direction of the mask. +x in this case.
                            addExtentQuad(xPos+(31-bit)+1, yPos, zPos, 1, 1, 0);
                        }
                        while (negXint){
                            int bit = __builtin_ctz(negXint);
                            negXint &= negXint-1;
                            addExtentQuad(xPos+(31-bit), yPos, zPos, 1, 1, 1);
                        }
                        while(posYint){
                            int bit = __builtin_ctz(posYint);
                            posYint &= posYint-1;
                            addExtentQuad(yPos+1, xPos+(31-bit), zPos, 1, 1, 2);
                        }
                        while(negYint){
                            int bit = __builtin_ctz(negYint);
                            negYint &= negYint-1;
                            addExtentQuad(yPos, xPos+(31-bit), zPos, 1, 1, 3);
                        }
                        while(posZint){
                            int bit = __builtin_ctz(posZint);
                            posZint &= posZint-1;
                            addExtentQuad(zPos+1, xPos+(31-bit), yPos , 1, 1, 4);
                        }
                        while(negZint){
                            int bit = __builtin_ctz(negZint);
                            negZint &= negZint-1;
                            addExtentQuad(zPos, xPos+(31-bit), yPos, 1,1, 5);
                        }
                    }
                }
            }
        // Greedy Meshing.
        } else {
            for (int z=0; z<cm.occupancyZsize; z++)
            {
                for (int y=0; y<cm.occupancyYsize; y++)
                {
                    for (int x=0; x<cm.occupancyXsize; x++)
                    {
                        int maskIndex = z*yxOffset + y*cm.occupancyXsize + x;

                        // go until the int in the mask has no more value.
                        while (posXMask[maskIndex] != 0u){
                            // find the most it can extend the quad and add the quad.
                            addGreedyFace(posXMask, maskIndex, x*32, y, z, 0);
                        }
                        while (negXMask[maskIndex] != 0u){
                            addGreedyFace(negXMask, maskIndex, x*32, y, z, 1);
                        }
                        while (posYMask[maskIndex] != 0u){
                            addGreedyFace(posYMask, maskIndex, x*32, y, z, 2);
                        }
                        while (negYMask[maskIndex] != 0u){
                            addGreedyFace(negYMask, maskIndex, x*32, y, z, 3);
                        }
                        while (posZMask[maskIndex] != 0u){
                            addGreedyFace(posZMask, maskIndex, x*32, y, z, 4);
                        }
                        while (negZMask[maskIndex] != 0u){
                            addGreedyFace(negZMask, maskIndex, x*32, y, z, 5);
                        }
                    }
                }
            }
        }
    }
    else if (updateType == 0){
        for (int z=0; z<cm.occupancyZsize; z++){
            for (int y=0; y<cm.occupancyYsize; y++){
                for (int x=0; x<cm.occupancyXsize; x++){
                    // Get occupancyInt
                    int occupancyIndex = z*cm.occupancyXsize*cm.occupancyYsize + y*cm.occupancyXsize + x;
                    uint32_t occupancyInt = occupancyInts[occupancyIndex];
                    // skip checking each bit if the whole int is empty.
                    if (occupancyInt == 0u) continue;
                    // check each bit.
                    for (int bit=0; bit<32; bit++){
                        // check if the bit is set.
                        if (occupancyInt & (1u << bit)){
                            // position is 0,0,0 for the first voxel.
                            glm::vec3 voxPos = glm::vec3(
                                x*32*cm.voxSizeMeters + bit*cm.voxSizeMeters, // x position of the voxel.
                                y*cm.voxSizeMeters, // y position of the voxel.
                                z*cm.voxSizeMeters // z position of the voxel.
                            );
                            int vertIndex = vBuff.size() / 3; // index of the first vertex for this voxel.
                            addCubePrimitive(&voxPos, vertIndex);
                        }
                    }
                }
            }
        }
    }
 
    if (debugMode == 1) std::cout << "MeshUpdate: " << std::fixed << std::setprecision(4) << glfwGetTime()-start << "s updateType:" << updateType << std::endl;
    // std::cout << "eBuff Size:" << eBuff.size() << std::endl;
    // std::cout << "nBuff Size:" << nBuff.size() << std::endl;
    // std::cout << "vBuff Size:" << vBuff.size() << std::endl;
    
    updateBuffer();

}

// Bind buffers and draw.
void Chunk::drawMesh(const Program& prog)
{
    // float start = glfwGetTime();
    // Quick Sanity Checks
    
    assert(vBuff.size() % 3 == 0);

    // Bind Uniforms
    glUniform3fv(prog.getUniform("chunkWorldPos"), 1, glm::value_ptr(worldcp));
    glUniform1f(prog.getUniform("chunkSizeMeters"), cm.chunkSizeMeters);
    glUniform1f(prog.getUniform("voxelSizeMeters"), cm.voxSizeMeters);

    // Enable and Bind arrays and buffers

    // VERTEX ARRAY
    glBindVertexArray(vaoID);
    GLuint vertAttr = prog.getAttribute("vertPos");
    if (vertAttr == -1) {
    std::cerr << "Shader vertex attribute not found in Chunk draw" << std::endl;
    return;
    }
    glEnableVertexAttribArray(vertAttr);
    glBindBuffer(GL_ARRAY_BUFFER, vBuffID);
    glVertexAttribPointer(vertAttr, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // COLOR ARRAY 
    GLuint normalAttr = prog.getAttribute("normalID");
    if (normalAttr == -1) {
    std::cerr << "Shader vertNormal attribute not found in Chunk draw" << std::endl;
    return;
    }
    glEnableVertexAttribArray(normalAttr);
    glBindBuffer(GL_ARRAY_BUFFER, nBuffID);
    // glVertexAttribPointer(normalAttr, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glVertexAttribIPointer(normalAttr, 1, GL_UNSIGNED_INT, 0, (void*)0);

    // COLOR TEXTURE
    GLuint colorTexUniform = prog.getUniform("matIDTex");
    // assert(colorTexUniform != -1);
    // glBindTextureUnit(0, cTexID); // bind to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, cTexID);
    glUniform1i(colorTexUniform, 0);
    // ELEMENT ARRAY
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eBuffID);
    
    
    // Draw mesh
    glDrawElements(GL_TRIANGLES, (int)eBuff.size(), GL_UNSIGNED_INT, (const void *)0);

    glDisableVertexAttribArray(vertAttr);
    glDisableVertexAttribArray(normalAttr);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

    // std::cout << "Mesh Draw Time: " << glfwGetTime()-start << std::endl;
}   


// Private Methods
void Chunk::fillMeterGrid(uint32_t* occupancyInt, int x, int y, int z)
{
    int bitShifts = 32;
    for(int shiftN = 0; shiftN < bitShifts; shiftN += cm.voxPerMeter){
        if (z%cm.voxPerMeter == 0 || y%cm.voxPerMeter == 0){
            // *occupancyInt |= (0b1 << cm.voxPerMeter*shiftN);
            *occupancyInt |= (1u << (31-shiftN));
        }
        if (y%cm.voxPerMeter == 0 && z%cm.voxPerMeter == 0){
            *occupancyInt |= 0b11111111111111111111111111111111;
        }
    } 
}

void Chunk::fillChunkGrid(uint32_t* occupancyInt, int x, int y, int z)
{
    // fill lines in the X direction
    if ((y == 0 || y == cm.occupancyYsize-1) && 
        (z == 0 || z == cm.occupancyZsize-1)){
        *occupancyInt |= 0b11111111111111111111111111111111;
    }
    // fill wall on the +x Side
    if ((x == cm.occupancyXsize-1) &&
             ((y == 0 || y == cm.occupancyYsize-1) ||
              (z == 0 || z == cm.occupancyZsize-1))){
         *occupancyInt |= 0b00000000000000000000000000000001;
    }
    // fill wall on the -x side
    if ((x == 0) &&
             ((y == 0 || y == cm.occupancyYsize-1) ||
              (z == 0 || z == cm.occupancyZsize-1))){
         *occupancyInt |= 0b10000000000000000000000000000000;
    }
}

void Chunk::fillFloor(uint32_t* occupancyInt, glm::vec3* voxPosCenter, int x, int z)
{
    
    if (voxPosCenter->y <= cm.voxSizeMeters){
        *occupancyInt |= 0b11111111111111111111111111111111;
    }
    // random grass stuff
    // else if (voxPosCenter->y <= cm.voxSizeMeters*3 && z%(x+5) == 0){
    //     *occupancyInt |= 0b1000000000001000000000001000000 >> (z%5);
    // }
}

void Chunk::updateBuffer(){
    // std::cout << "nBuff data: " << nBuff.size()*sizeof(GLfloat) << std::endl;
    // std::cout << "vBuff data: " << vBuff.size()*sizeof(GLfloat) << std::endl;
    // std::cout << "eBuff data: " << eBuff.size()*sizeof(unsigned int) << std::endl;


    if (bufferUpdateMethod == 2){
        // MORE ADVANCED AND FASTER
        // buffer orphaning and SubData update
        glBindBuffer(GL_ARRAY_BUFFER, nBuffID); 
        // glBufferData(GL_ARRAY_BUFFER, nBuff.size()*sizeof(GLfloat), NULL, GL_STREAM_DRAW);   
        // glBufferSubData(GL_ARRAY_BUFFER, 0, nBuff.size()*sizeof(GLfloat), nBuff.data());
        glBufferSubData(GL_ARRAY_BUFFER, 0, nBuff.size()*sizeof(GLuint), nBuff.data());

        glBindBuffer(GL_ARRAY_BUFFER, vBuffID);
        // glBufferData(GL_ARRAY_BUFFER, vBuff.size()*sizeof(GLfloat), NULL, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vBuff.size()*sizeof(GLfloat), vBuff.data());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eBuffID);
        // glBufferData(GL_ELEMENT_ARRAY_BUFFER, eBuff.size()*sizeof(unsigned int), NULL, GL_STREAM_DRAW);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, eBuff.size()*sizeof(unsigned int), eBuff.data());
    }
    else if (bufferUpdateMethod == 1){
        memcpy(nPtr, nBuff.data(), nBuff.size()*sizeof(GLuint));
        memcpy(vPtr, vBuff.data(), vBuff.size()*sizeof(GLfloat));
        memcpy(ePtr, eBuff.data(), eBuff.size()*sizeof(unsigned int));
    }
    else if (bufferUpdateMethod == 0){
        // PRIMITIVE BUFFER MANAGEMENT
        glBindBuffer(GL_ARRAY_BUFFER, nBuffID); 
        glBufferData(GL_ARRAY_BUFFER, nBuff.size()*sizeof(GLuint), nBuff.data(), GL_STREAM_DRAW);   

        glBindBuffer(GL_ARRAY_BUFFER, vBuffID);
        glBufferData(GL_ARRAY_BUFFER, vBuff.size()*sizeof(GLfloat), vBuff.data(), GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eBuffID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, eBuff.size()*sizeof(unsigned int), eBuff.data(), GL_STREAM_DRAW);
    }
}

void Chunk::addGreedyFace(uint32_t* mask, int maskIndex,
                          float xPos, float yPos, float zPos, int direction)
{
    if (direction == 0 || direction == 1){
        // +X / -X
    
        uint32_t* faceInt = &mask[maskIndex];
        assert(*faceInt != 0u);
        int leadingZeros = __builtin_clz(*faceInt);
        int width = 1;   // expands in +Y
        int height = 1;  // expands in +Z
        int shiftDistance = 32 - (width + leadingZeros);
        uint32_t compareInt = (*faceInt >> shiftDistance) << shiftDistance;
        *faceInt &= ~compareInt; // remove from current
    
        // Expand in Y direction
        bool expandWidth = true;
        int baseIndex = maskIndex;
        while (expandWidth){
            if ((int)yPos + width >= cm.occupancyYsize){
                break;
            }
            int nextIndex = baseIndex + width * cm.occupancyXsize;
            uint32_t* newFace = &mask[nextIndex];
    
            if ( (compareInt & *newFace) == compareInt ){
                *newFace &= ~compareInt;
                width++;
            } else {
                expandWidth = false;
            }
        }
    
        // Expand in the Z direction
        bool expandHeight = true;
        while (expandHeight){
            if ((int)zPos + height >= cm.occupancyZsize){
                break;
            }
    
            // check entire width row at next Z
            for (int w = 0; w < width; w++){
                int checkIndex = baseIndex 
                    + w * cm.occupancyXsize 
                    + height * cm.occupancyXsize * cm.occupancyYsize;
    
                uint32_t* newFace = &mask[checkIndex];
                if ( (compareInt & *newFace) != compareInt ){
                    expandHeight = false;
                    break;
                }
            }
    
            // if full row valid, remove bits
            if (expandHeight){
                for (int w = 0; w < width; w++){
                    int removeIndex = baseIndex 
                        + w * cm.occupancyXsize 
                        + height * cm.occupancyXsize * cm.occupancyYsize;
                    mask[removeIndex] &= ~compareInt;
                }
                height++;
            }
        }
    
        // +X
        if (direction == 0){
            addExtentQuad(xPos+leadingZeros+1, yPos, zPos, width, height, direction);
        }
        // -X
        else {
            addExtentQuad(xPos+leadingZeros, yPos, zPos, width, height, direction);
        }
    } else {
        // +Y-Y+Z-Z directions
        uint32_t* faceInt = &mask[maskIndex];
        assert(*faceInt != 0u);
        int leadingZeros = __builtin_clz(*faceInt);
        int width = __builtin_clz(~(*faceInt << leadingZeros)); // how many 1s in a row are there.
        int shiftDistance = 32 - (width+leadingZeros);
        uint32_t compareInt = (*faceInt >> (shiftDistance)) << shiftDistance; // isolates the 1s group.
        *faceInt &= ~compareInt; // removes the ones from the original int
    
        // // for Y 2or3
        // //  width = X direction
        // //  height = Z direction
        // // for Z
        // //  width = X direction
        // //  height = Y direction
        // int width = onesSize;
        int height = 1; 
    
        bool tryNext = true;
        while (tryNext){
            // get the next position row (by the correct direction)
            if ((direction == 2 || direction == 3)){
                if ((int)zPos + height >= cm.occupancyZsize){
                    break;
                }
                // move in the y direction.
                maskIndex = maskIndex + cm.occupancyXsize*cm.occupancyYsize;
            } else {
                if ((int)yPos + height >= cm.occupancyYsize){
                    break;
                }
                // move in the z direction.
                maskIndex = maskIndex + cm.occupancyXsize;
            }
            
            // compare the new integer for the same 1's to see if we can expand the mesh.
            uint32_t* newCompareInt = &mask[maskIndex];
            if ( (compareInt & *newCompareInt) == compareInt){ //if it has similar width
                *newCompareInt &= ~compareInt; // remove from newCompareInt
                height++;
            } 
            else { // if we fail to expand end the loop and then add the quad.
                tryNext = false;
            }
        }
        // +Y
        if (direction == 2){
            addExtentQuad(yPos+1, xPos+leadingZeros, zPos, width, height, direction);
        }
        // -Y
        else if (direction == 3){
            addExtentQuad(yPos, xPos+leadingZeros, zPos, width, height, direction);
        }
        // +Z
        else if (direction == 4){
            addExtentQuad(zPos+1, xPos+leadingZeros, yPos, width, height, direction);
        }
        // -Z
        else if (direction == 5){
            addExtentQuad(zPos, xPos+leadingZeros, yPos, width, height, direction);
        }
    }
}

 void Chunk::addExtentQuad(float anchor, float uStart, float vStart, float uExtent, int vExtent, int dir){
    // index
    int vi = vBuff.size() / 3;

    float baseX, baseY, baseZ;
    if (dir == 0 || dir == 1) {
        baseX = anchor;
        baseY = uStart;
        baseZ = vStart;
    }else if (dir == 2 || dir == 3) {
        baseX = uStart;
        baseY = anchor;
        baseZ = vStart;
    } else {
        baseX = uStart;
        baseY = vStart;
        baseZ = anchor;
    }

    // X faces
    // dir 0=+x, 1=-x
    // anchor is x axis
    // u = Y
    // v = Z
    if (dir == 0 || dir == 1) {   
        // v0
        vBuff.push_back(worldcp.x + anchor * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + (uStart+uExtent) * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + (vStart+vExtent) * cm.voxSizeMeters);
        nBuff.push_back(dir);
    
        // v1
        vBuff.push_back(worldcp.x + anchor * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + uStart * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + (vStart+vExtent) * cm.voxSizeMeters);
        nBuff.push_back(dir);
    
        // v2
        vBuff.push_back(worldcp.x + anchor * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + uStart * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + vStart * cm.voxSizeMeters);
        nBuff.push_back(dir);
    
        // v3
        vBuff.push_back(worldcp.x + anchor * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + (uStart+uExtent) * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + vStart * cm.voxSizeMeters);
        nBuff.push_back(dir);
    }

    // Y faces
    // dir 2=+Y, 3=-Y
    // anchor is Y axis
    // u = X
    // v = Z
    else if (dir == 2 || dir == 3) {
        // v0
        vBuff.push_back(worldcp.x + uStart * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + anchor * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + (vStart+vExtent) * cm.voxSizeMeters);    
        nBuff.push_back(dir);
    
        // v1
        vBuff.push_back(worldcp.x + (uStart+uExtent) * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + anchor * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + (vStart+vExtent) * cm.voxSizeMeters);
        nBuff.push_back(dir);
    
        // v2
        vBuff.push_back(worldcp.x + (uStart+uExtent) * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + anchor * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + vStart * cm.voxSizeMeters);
        nBuff.push_back(dir);
    
        // v3
        vBuff.push_back(worldcp.x + uStart * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + anchor * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + vStart * cm.voxSizeMeters);
        nBuff.push_back(dir);
    }

    // Z faces
    // directions 4=+Z, 5=-Z
    // anchor is Z axis
    // u = X
    // v = Y
    else if (dir == 4 || dir == 5) {
        // v0
        vBuff.push_back(worldcp.x + uStart * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + (vStart+vExtent) * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + anchor * cm.voxSizeMeters);
        nBuff.push_back(dir);
    
        // v1
        vBuff.push_back(worldcp.x + uStart * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + vStart * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + anchor * cm.voxSizeMeters);
        nBuff.push_back(dir);
    
        // v2
        vBuff.push_back(worldcp.x + (uStart+uExtent) * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + vStart * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + anchor * cm.voxSizeMeters);
        nBuff.push_back(dir);
    
        // v3
        vBuff.push_back(worldcp.x + (uStart+uExtent) * cm.voxSizeMeters);
        vBuff.push_back(worldcp.y + (vStart+vExtent) * cm.voxSizeMeters);
        vBuff.push_back(worldcp.z + anchor * cm.voxSizeMeters);
        nBuff.push_back(dir);
    }

    // correct winding
    if (dir % 2 == 0) {
        eBuff.push_back(vi+0);
        eBuff.push_back(vi+1);
        eBuff.push_back(vi+2);
        eBuff.push_back(vi+0);
        eBuff.push_back(vi+2);
        eBuff.push_back(vi+3);
    } else {
        eBuff.push_back(vi+0);
        eBuff.push_back(vi+2);
        eBuff.push_back(vi+1);
        eBuff.push_back(vi+0);
        eBuff.push_back(vi+3);
        eBuff.push_back(vi+2);
    } 
}

void Chunk::addCubePrimitive(glm::vec3* voxPos, int vertIndex)
{
    // ADD VERTEXES & COLOR
    // ID 0: 0,0,0 corner
    // ID 1: 1,0,0 +x
    // ID 2: 0,1,0 +y
    // ID 3: 1,1,0 +x+y
    // ID 4: 0,0,1 +z
    // ID 5: 1,0,1 +x+z
    // ID 6: 0,1,1 +y+z
    // ID 7: 1,1,1 +z+y+z
    for (int dz=0; dz<=1; dz++)
    {
        for (int dy=0; dy<=1; dy++)
        {
            for (int dx=0; dx<=1; dx++)
            {
                vBuff.push_back(worldcp.x + voxPos->x + dx*cm.voxSizeMeters);
                vBuff.push_back(worldcp.y + voxPos->y + dy*cm.voxSizeMeters);
                vBuff.push_back(worldcp.z + voxPos->z + dz*cm.voxSizeMeters);

                nBuff.push_back(2u);//normal up.
            }
        }
    }

    // ADD TRIANGLES
    // Add 12 triangles for the cube.
    // +x Face:
    eBuff.push_back(vertIndex + 3); // +x+y
    eBuff.push_back(vertIndex + 5); // +x+z
    eBuff.push_back(vertIndex + 1); // +x

    eBuff.push_back(vertIndex + 3); // +x+y 
    eBuff.push_back(vertIndex + 7); // +x+y+z
    eBuff.push_back(vertIndex + 5); // +x+z

    //-x Face
    eBuff.push_back(vertIndex + 6); // +y+z
    eBuff.push_back(vertIndex + 0); // corner
    eBuff.push_back(vertIndex + 4); // +Z
    
    eBuff.push_back(vertIndex + 6); // +y+z
    eBuff.push_back(vertIndex + 2); // +y
    eBuff.push_back(vertIndex + 0); // corner

    // +y Face:
    eBuff.push_back(vertIndex + 6); // +y+z
    eBuff.push_back(vertIndex + 3); // +x+y
    eBuff.push_back(vertIndex + 2); // +y
    
    eBuff.push_back(vertIndex + 6); // +y+z
    eBuff.push_back(vertIndex + 7); // +x+y+z
    eBuff.push_back(vertIndex + 3); // +x+y

    //-y Face
    eBuff.push_back(vertIndex + 5); // +x+z
    eBuff.push_back(vertIndex + 0); // corner
    eBuff.push_back(vertIndex + 1); // +x
    
    eBuff.push_back(vertIndex + 5); // +x+z
    eBuff.push_back(vertIndex + 4); // +z
    eBuff.push_back(vertIndex + 0); // corner

    // +z Face
    eBuff.push_back(vertIndex + 7); // +x+y+z
    eBuff.push_back(vertIndex + 4); // +z
    eBuff.push_back(vertIndex + 5); // +x+z
    
    eBuff.push_back(vertIndex + 7); // +x+y+z
    eBuff.push_back(vertIndex + 6); // +y+z
    eBuff.push_back(vertIndex + 4); // +z

    // -z Face
    eBuff.push_back(vertIndex + 2); // +y 
    eBuff.push_back(vertIndex + 1); // +x
    eBuff.push_back(vertIndex + 0); // corner
    
    eBuff.push_back(vertIndex + 2); // +y
    eBuff.push_back(vertIndex + 3); // +x+y
    eBuff.push_back(vertIndex + 1); // +x
}
