
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
