#include "Skybox.h"

#include "Program.h"

#include <iostream>

#include "stb_image.h"

Skybox::Skybox() = default;

Skybox::~Skybox()
{
	if (vbo_)
		glDeleteBuffers(1, &vbo_);
	if (vao_)
		glDeleteVertexArrays(1, &vao_);
	if (tex_)
		glDeleteTextures(1, &tex_);
}

bool Skybox::init(const std::string &resourceDir, const std::string &faceDir, const std::string &ext)
{
	// Load 6 faces. Order must match GL_TEXTURE_CUBE_MAP_POSITIVE_X .. NEGATIVE_Z.
	const char *faceNames[6] = { "px", "nx", "py", "ny", "pz", "nz" };

	glGenTextures(1, &tex_);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex_);

	// Cubemap faces should NOT be flipped vertically (different convention vs 2D textures).
	stbi_set_flip_vertically_on_load(false);

	for (int i = 0; i < 6; ++i) {
		std::string path = resourceDir + "/" + faceDir + "/" + faceNames[i] + "." + ext;
		int w = 0, h = 0, n = 0;
		unsigned char *data = stbi_load(path.c_str(), &w, &h, &n, 3);
		if (!data) {
			std::cerr << "Skybox: failed to load cubemap face " << path << std::endl;
			glDeleteTextures(1, &tex_);
			tex_ = 0;
			return false;
		}
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB8, w, h, 0,
					 GL_RGB, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	prog_ = std::make_shared<Program>();
	prog_->setVerbose(true);
	prog_->setShaderNames(resourceDir + "/shaders/skybox/skybox_vert.glsl",
						  resourceDir + "/shaders/skybox/skybox_frag.glsl");
	if (!prog_->init())
		return false;
	prog_->addUniform("P");
	prog_->addUniform("V");
	prog_->addUniform("skybox");

	float skyboxVertices[] = {
		-1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,
		-1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,
		1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,
		-1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,
		-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
	};

	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);
	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}

void Skybox::draw(const glm::mat4 &P, const glm::mat4 &viewRotationOnly)
{
	if (!tex_ || !prog_)
		return;

	// Skybox is drawn from inside the cube; disable culling so inward-facing
	// triangles aren't discarded.
	GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);

	prog_->bind();
	glUniformMatrix4fv(prog_->getUniform("P"), 1, GL_FALSE, &P[0][0]);
	glUniformMatrix4fv(prog_->getUniform("V"), 1, GL_FALSE, &viewRotationOnly[0][0]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex_);
	glUniform1i(prog_->getUniform("skybox"), 0);
	glBindVertexArray(vao_);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	prog_->unbind();

	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	if (cullWasEnabled)
		glEnable(GL_CULL_FACE);
}
