
#include "Materials.h"

Materials::Materials():
    materials(){}

void Materials::init(GLuint bindingPoint){
    // lambda to help add materials.
    auto addMaterial  = [&](
        glm::vec4 ambient, 
        glm::vec4 diffuse,
        glm::vec4 specular,
        float shininess
    ) {
        materials.push_back(
            Material{ambient, diffuse, specular, shininess});
    };
    auto addDb16Material = [&](float r, float g, float b) {
        const glm::vec4 diffuse(r, g, b, 1.0f);
        const glm::vec4 ambient(
            glm::max(0.02f, r * 0.22f),
            glm::max(0.02f, g * 0.22f),
            glm::max(0.02f, b * 0.22f),
            1.0f);
        const glm::vec4 specular(
            glm::min(1.0f, r * 0.35f + 0.04f),
            glm::min(1.0f, g * 0.35f + 0.04f),
            glm::min(1.0f, b * 0.35f + 0.04f),
            1.0f);
        addMaterial(ambient, diffuse, specular, 18.0f);
    };
    // basic grass
    addMaterial(
        glm::vec4(0.03, 0.14, 0.04, 1.0), // ambient (dark green)
        glm::vec4(0.1, 0.3, 0.1, 1.0), // diffuse (rich green)
        glm::vec4(0.24, 0.41, 0.24, 1.0), // specular (very subtle, slightly green)
        15.0f // shininess (broad highlight)
    );
    // basic Stone (gray)
    addMaterial(
        glm::vec4(0.07f, 0.07f, 0.07f, 1.0f),  // amb
        glm::vec4(0.38f, 0.38f, 0.38f, 1.0f),  // diff (darker medium gray)
        glm::vec4(0.14f, 0.14f, 0.14f, 1.0f),  // spec
        4.0f                                    // shine
    );
    // brick
    addMaterial(
        glm::vec4(0.16f, 0.03f, 0.03f, 1.0f),
        glm::vec4(0.62f, 0.18f, 0.16f, 1.0f),
        glm::vec4(0.18f, 0.08f, 0.08f, 1.0f),
        8.0f
    );
    // sand
    addMaterial(
        glm::vec4(0.18f, 0.15f, 0.08f, 1.0f),
        glm::vec4(0.72f, 0.63f, 0.30f, 1.0f),
        glm::vec4(0.22f, 0.20f, 0.12f, 1.0f),
        10.0f
    );
    // dirt — earthy brown subsoil layer.
    addMaterial(
        glm::vec4(0.10f, 0.06f, 0.03f, 1.0f),  // amb (deep brown)
        glm::vec4(0.42f, 0.26f, 0.12f, 1.0f),  // diff (saturated warm brown)
        glm::vec4(0.08f, 0.05f, 0.03f, 1.0f),  // spec (very low — dirt is matte)
        4.0f                                    // shine
    );
    // gold — strong yellow-gold with a tight, bright specular highlight.
    addMaterial(
        glm::vec4(0.20f, 0.14f, 0.02f, 1.0f),  // amb (warm)
        glm::vec4(0.95f, 0.78f, 0.18f, 1.0f),  // diff (rich gold)
        glm::vec4(1.00f, 0.92f, 0.50f, 1.0f),  // spec (bright, slightly warm white)
        128.0f                                  // shine (very tight highlight)
    );
    // DB16 palette for authored voxel flora and props.
    addDb16Material(0x14 / 255.0f, 0x0c / 255.0f, 0x1c / 255.0f);
    addDb16Material(0x44 / 255.0f, 0x24 / 255.0f, 0x34 / 255.0f);
    addDb16Material(0x30 / 255.0f, 0x34 / 255.0f, 0x6d / 255.0f);
    addDb16Material(0x4e / 255.0f, 0x4a / 255.0f, 0x4e / 255.0f);
    addDb16Material(0x85 / 255.0f, 0x4c / 255.0f, 0x30 / 255.0f);
    addDb16Material(0x34 / 255.0f, 0x65 / 255.0f, 0x24 / 255.0f);
    addDb16Material(0xd0 / 255.0f, 0x46 / 255.0f, 0x48 / 255.0f);
    addDb16Material(0x75 / 255.0f, 0x71 / 255.0f, 0x61 / 255.0f);
    addDb16Material(0x59 / 255.0f, 0x7d / 255.0f, 0xce / 255.0f);
    addDb16Material(0xd2 / 255.0f, 0x7d / 255.0f, 0x2c / 255.0f);
    addDb16Material(0x85 / 255.0f, 0x95 / 255.0f, 0xa1 / 255.0f);
    addDb16Material(0x6d / 255.0f, 0xaa / 255.0f, 0x2c / 255.0f);
    addDb16Material(0xd2 / 255.0f, 0xaa / 255.0f, 0x99 / 255.0f);
    addDb16Material(0x6d / 255.0f, 0xc2 / 255.0f, 0xca / 255.0f);
    addDb16Material(0xda / 255.0f, 0xd4 / 255.0f, 0x5e / 255.0f);
    addDb16Material(0xde / 255.0f, 0xee / 255.0f, 0xd6 / 255.0f);

    
    
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
