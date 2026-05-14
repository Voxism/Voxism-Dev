#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <glad/glad.h>

#include "Program.h"
#include "Shape.h"
#include "Texture.h"

class ToolView {
public:
	ToolView() = default;
	~ToolView() = default;

	bool init(const std::string &resourceDirectory,
          const std::shared_ptr<Program> &litProgram,
          GLuint textureID);

    void draw(int width,
          int height,
          const glm::mat4 &sceneView,
          const glm::vec3 &cameraPos,
          const glm::vec3 &cameraForward,
          const glm::vec3 &cameraRight,
          const glm::vec3 &cameraUp,
          const glm::vec3 &lightPos,
          const glm::vec3 &lightColor);

	void setVisible(bool visible) { visible_ = visible; }
	bool isVisible() const { return visible_; }

	void setOffset(const glm::vec3 &offset) { offset_ = offset; }
	void setRotationDeg(const glm::vec3 &rotationDeg) { rotationDeg_ = rotationDeg; }
	void setScale(glm::vec3 scale) { scale_ = scale; }
	void setFov(float fov) { fovDeg_ = fov; }

	void setAnimTime(float t) { animTime_ = t; }
	void setMoveBlend(float b) { moveBlend_ = b; }
	void setUseBob(bool useBob) { useBob_ = useBob; }

	void triggerUse();
	void update(float dt);

private:
	static void computeNormals(tinyobj::mesh_t &mesh);
	static void ensureTexcoordsXZ(tinyobj::shape_t &sh);
	static std::vector<float> loadVertexColors(const std::string &path, size_t expectedVertexCount);
	static std::shared_ptr<Shape> loadMesh(const std::string &resourceDirectory,
	                                       const char *primaryName,
	                                       const char *fallbackName);

private:
	std::shared_ptr<Program> prog_;
	std::shared_ptr<Shape> mesh_;

	GLuint textureID_ = 0;

	bool visible_ = true;
	bool useBob_ = true;

	float fovDeg_ = 55.0f;
	glm::vec3 scale_ = glm::vec3(0.1f);
	float animTime_ = 0.0f;
	float moveBlend_ = 0.0f;

	bool useAnimating_ = false;
	float useAnimTime_ = 0.0f;
	float useAnimDuration_ = 0.18f;

	glm::vec3 offset_ = glm::vec3(-100.0f, 0.0f, 0.0f);
	glm::vec3 rotationDeg_ = glm::vec3(0.0f, 0.0f, 0.0f);
};
