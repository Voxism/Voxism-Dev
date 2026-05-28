
#include "Materials.h"

Materials::Materials():
    materials(){
        assert(sizeof(Material)%16==0);
    }

void Materials::init(GLuint bindingPoint){
    // lambda to help add materials.
    auto addMaterial  = [&](
        glm::vec4 diffuse,
        float roughness,
        float metallic,
        int diffRandomFactor,
        int roughRandomFactor,
        int metallicRandomFactor,
        int pattern
    ) {
        materials.push_back(
            Material{diffuse, roughness, metallic, diffRandomFactor, roughRandomFactor, metallicRandomFactor, pattern});
    };

    auto addDb16Material = [&](
        float r,
        float g,
        float b,
        float roughness = 0.78f,
        float metallic = 0.0f
    ) {
        const glm::vec4 diffuse(r, g, b, 1.0f);
        addMaterial(diffuse, roughness, metallic, 0,0,0,0);
    };

    // Grass
    addMaterial(
        glm::vec4(0.11f, 0.40f, 0.11f, 1.0f), // diffuse
        0.50f, //roughness
        0.0f, //metallic
        15, //diffRandomFactor [0-200] 
        80, //roughRandomFactor [0-200] 
        0, //metallicRandomFactor [0-200]
        2 //pattern
    );
    
    // Stone
    addMaterial(
        glm::vec4(0.30f, 0.30f, 0.30f, 1.0f),
        0.7f,
        0.05f,
        15,
        30,
        30,
        0
    );

    // Brick
    addMaterial(
        glm::vec4(0.60f, 0.30f, 0.26f, 1.0f),
        0.88f,
        0.0f,
        0,
        0,
        0,
        0
    );

    // Sand
    addMaterial(
        glm::vec4(1.0f, 0.89f, 0.49f, 1.0f),
        0.25f,
        0.00f,
        0,
        0,
        0,
        0
    );

    // Dirt
    addMaterial(
        glm::vec4(0.29f, 0.22f, 0.08f, 1.0f),
        0.80f,
        0.08f,
        0,
        0,
        0,
        0
    );
    
    // Gold
    addMaterial(
        glm::vec4(0.85f, 0.73f, 0.09f, 1.0f),
        0.40f,
        0.85f,
        0,
        0,
        0,
        0
    );

    // DB16 palette for authored voxel flora and props.
    addDb16Material(0x08 / 255.0f, 0x08 / 255.0f, 0x09 / 255.0f, 0.95f, 0.00f); // DB16 Black
    addDb16Material(0x44 / 255.0f, 0x24 / 255.0f, 0x55 / 255.0f, 0.82f, 0.00f); // DB16 Purple
    addDb16Material(0x20 / 255.0f, 0x24 / 255.0f, 0x5f / 255.0f, 0.76f, 0.00f); // DB16 Navy
    addDb16Material(0x4e / 255.0f, 0x4a / 255.0f, 0x4e / 255.0f, 0.70f, 0.05f); // DB16 Slate
    addDb16Material(0x85 / 255.0f, 0x4c / 255.0f, 0x30 / 255.0f, 0.82f, 0.00f); // DB16 Brown
    addDb16Material(0x34 / 255.0f, 0x65 / 255.0f, 0x24 / 255.0f, 0.88f, 0.00f); // DB16 Dark Green
    addDb16Material(0xd0 / 255.0f, 0x46 / 255.0f, 0x48 / 255.0f, 0.68f, 0.00f); // DB16 Red
    addDb16Material(0x75 / 255.0f, 0x71 / 255.0f, 0x61 / 255.0f, 0.78f, 0.00f); // DB16 Gray
    addDb16Material(0x59 / 255.0f, 0x7d / 255.0f, 0xce / 255.0f, 0.56f, 0.00f); // DB16 Blue
    addDb16Material(0xd2 / 255.0f, 0x7d / 255.0f, 0x2c / 255.0f, 0.64f, 0.00f); // DB16 Orange
    addDb16Material(0x9a / 255.0f, 0xa5 / 255.0f, 0xad / 255.0f, 0.09f, 1.0f); // DB16 Silver
    addDb16Material(0x6d / 255.0f, 0xaa / 255.0f, 0x2c / 255.0f, 0.84f, 0.00f); // DB16 Green
    addDb16Material(0xd2 / 255.0f, 0xaa / 255.0f, 0x99 / 255.0f, 0.72f, 0.00f); // DB16 Peach
    addDb16Material(0x6d / 255.0f, 0xc2 / 255.0f, 0xca / 255.0f, 0.44f, 0.00f); // DB16 Cyan
    addDb16Material(0xda / 255.0f, 0xd4 / 255.0f, 0x5e / 255.0f, 0.58f, 0.00f); // DB16 Yellow
    addDb16Material(0xde / 255.0f, 0xee / 255.0f, 0xd6 / 255.0f, 0.66f, 0.00f); // DB16 White
        
    // generate buffer.
    glGenBuffers(1, &matBuffID);
    glBindBuffer(GL_UNIFORM_BUFFER, matBuffID);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Material)*materials.size(), materials.data(), GL_STATIC_DRAW);
    // remember index for later and bind buffer to the index.
    this->bindingPoint = bindingPoint;
    glBindBufferBase(GL_UNIFORM_BUFFER, this->bindingPoint, matBuffID);
}

// typically a set once and forget.
void Materials::bind(){
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, matBuffID);
}

const char *Materials::paletteName(int index)
{
    static const char *kNames[] = {
        "Grass",
        "Stone",
        "Brick Red",
        "Sand",
        "Dirt",
        "Gold",
        "DB16 Black",
        "DB16 Purple",
        "DB16 Navy",
        "DB16 Slate",
        "DB16 Brown",
        "DB16 Dark Green",
        "DB16 Red",
        "DB16 Gray",
        "DB16 Blue",
        "DB16 Orange",
        "DB16 Silver",
        "DB16 Green",
        "DB16 Peach",
        "DB16 Cyan",
        "DB16 Yellow",
        "DB16 White"
    };

    if (index < 0 || index >= paletteCount) {
        return "Unknown";
    }
    return kNames[index];
}

glm::vec3 Materials::paletteColor(int index)
{
    static const glm::vec3 kColors[] = {
        glm::vec3(0.10f, 0.30f, 0.10f),
        glm::vec3(0.38f, 0.38f, 0.38f),
        glm::vec3(0.62f, 0.18f, 0.16f),
        glm::vec3(0.72f, 0.63f, 0.30f),
        glm::vec3(0.42f, 0.26f, 0.12f),
        glm::vec3(0.95f, 0.78f, 0.18f),
        glm::vec3(0x14 / 255.0f, 0x0c / 255.0f, 0x1c / 255.0f),
        glm::vec3(0x44 / 255.0f, 0x24 / 255.0f, 0x34 / 255.0f),
        glm::vec3(0x30 / 255.0f, 0x34 / 255.0f, 0x6d / 255.0f),
        glm::vec3(0x4e / 255.0f, 0x4a / 255.0f, 0x4e / 255.0f),
        glm::vec3(0x85 / 255.0f, 0x4c / 255.0f, 0x30 / 255.0f),
        glm::vec3(0x34 / 255.0f, 0x65 / 255.0f, 0x24 / 255.0f),
        glm::vec3(0xd0 / 255.0f, 0x46 / 255.0f, 0x48 / 255.0f),
        glm::vec3(0x75 / 255.0f, 0x71 / 255.0f, 0x61 / 255.0f),
        glm::vec3(0x59 / 255.0f, 0x7d / 255.0f, 0xce / 255.0f),
        glm::vec3(0xd2 / 255.0f, 0x7d / 255.0f, 0x2c / 255.0f),
        glm::vec3(0x85 / 255.0f, 0x95 / 255.0f, 0xa1 / 255.0f),
        glm::vec3(0x6d / 255.0f, 0xaa / 255.0f, 0x2c / 255.0f),
        glm::vec3(0xd2 / 255.0f, 0xaa / 255.0f, 0x99 / 255.0f),
        glm::vec3(0x6d / 255.0f, 0xc2 / 255.0f, 0xca / 255.0f),
        glm::vec3(0xda / 255.0f, 0xd4 / 255.0f, 0x5e / 255.0f),
        glm::vec3(0xde / 255.0f, 0xee / 255.0f, 0xd6 / 255.0f)
    };

    if (index < 0 || index >= paletteCount) {
        return glm::vec3(0.38f, 0.38f, 0.38f);
    }
    return kColors[index];
}
