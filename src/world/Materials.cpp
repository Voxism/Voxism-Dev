
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
        float metallic
    ) {
        materials.push_back(
            Material{diffuse, roughness, metallic});
    };

    // Grass
    addMaterial(
        glm::vec4(0.11f, 0.40f, 0.11f, 1.0f),
        0.55f,
        0.0f
    );
    
    // Stone
    addMaterial(
        glm::vec4(0.20f, 0.20f, 0.20f, 1.0f),
        0.8f,
        0.0f
    );

    // Brick
    addMaterial(
        glm::vec4(0.60f, 0.30f, 0.26f, 1.0f),
        0.88f,
        0.0f
    );

    // Sand
    addMaterial(
        glm::vec4(1.0f, 0.89f, 0.49f, 1.0f),
        0.25f,
        0.00f
    );

    // Dirt
    addMaterial(
        glm::vec4(0.29f, 0.22f, 0.08f, 1.0f),
        0.80f,
        0.08f
    );
    
    // Gold
    addMaterial(
        glm::vec4(0.85f, 0.73f, 0.09f, 1.0f),
        0.40f,
        0.85f
    );
    
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
        "Gold"
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
        glm::vec3(0.95f, 0.78f, 0.18f)
    };

    if (index < 0 || index >= paletteCount) {
        return glm::vec3(0.38f, 0.38f, 0.38f);
    }
    return kColors[index];
}
