
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
    // basic Stone
    addMaterial(
        glm::vec4(0.05, 0.05, 0.05, 1.0), // amb
        glm::vec4(0.2, 0.2, 0.3, 1.0), // diff
        glm::vec4(0.05, 0.05, 0.05, 1.0), // spec
        3 // shine
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