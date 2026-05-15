#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

#include <glad/glad.h>

class Program;

/** Cubemap skybox loaded from 6 face images (px, nx, py, ny, pz, nz). */
class Skybox {
public:
	Skybox();
	~Skybox();

	/**
	 * Load 6 cubemap faces from <resourceDir>/<faceDir>/{px,nx,py,ny,pz,nz}.<ext>
	 * Pass faceDir e.g. "skybox", and ext e.g. "png".
	 */
	bool init(const std::string &resourceDir, const std::string &faceDir, const std::string &ext);

	void draw(const glm::mat4 &P, const glm::mat4 &viewRotationOnly);

private:
	std::shared_ptr<Program> prog_;
	GLuint tex_ = 0;
	GLuint vao_ = 0;
	GLuint vbo_ = 0;
};
