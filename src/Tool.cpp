#include "Tool.h"

#include <iostream>
#include <vector>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;
using namespace glm;

static void computeNormalsImpl(tinyobj::mesh_t &mesh)
{
	vector<float> nor;
	nor.resize(mesh.positions.size(), 0.0f);

	for (size_t i = 0; i < mesh.indices.size() / 3; i++) {
		unsigned int idx0 = mesh.indices[3 * i + 0];
		unsigned int idx1 = mesh.indices[3 * i + 1];
		unsigned int idx2 = mesh.indices[3 * i + 2];

		vec3 v0(mesh.positions[3 * idx0 + 0], mesh.positions[3 * idx0 + 1], mesh.positions[3 * idx0 + 2]);
		vec3 v1(mesh.positions[3 * idx1 + 0], mesh.positions[3 * idx1 + 1], mesh.positions[3 * idx1 + 2]);
		vec3 v2(mesh.positions[3 * idx2 + 0], mesh.positions[3 * idx2 + 1], mesh.positions[3 * idx2 + 2]);

		vec3 normal = cross(v1 - v0, v2 - v0);
		nor[3 * idx0 + 0] += normal.x;
		nor[3 * idx0 + 1] += normal.y;
		nor[3 * idx0 + 2] += normal.z;
		nor[3 * idx1 + 0] += normal.x;
		nor[3 * idx1 + 1] += normal.y;
		nor[3 * idx1 + 2] += normal.z;
		nor[3 * idx2 + 0] += normal.x;
		nor[3 * idx2 + 1] += normal.y;
		nor[3 * idx2 + 2] += normal.z;
	}

	for (size_t i = 0; i < nor.size() / 3; i++) {
		vec3 n(nor[3 * i + 0], nor[3 * i + 1], nor[3 * i + 2]);
		if (length(n) > 0.0f)
			n = normalize(n);
		nor[3 * i + 0] = n.x;
		nor[3 * i + 1] = n.y;
		nor[3 * i + 2] = n.z;
	}
	mesh.normals = nor;
}

static void ensureTexcoordsXZImpl(tinyobj::shape_t &sh)
{
	size_t nv = sh.mesh.positions.size() / 3;
	if (nv == 0 || !sh.mesh.texcoords.empty())
		return;
	sh.mesh.texcoords.resize(nv * 2);
	for (size_t i = 0; i < nv; i++) {
		float x = sh.mesh.positions[3 * i + 0];
		float z = sh.mesh.positions[3 * i + 2];
		sh.mesh.texcoords[2 * i + 0] = x * 0.12f + 0.5f;
		sh.mesh.texcoords[2 * i + 1] = z * 0.12f + 0.5f;
	}
}

void ToolView::computeNormals(tinyobj::mesh_t &mesh)
{
	computeNormalsImpl(mesh);
}

void ToolView::ensureTexcoordsXZ(tinyobj::shape_t &sh)
{
	ensureTexcoordsXZImpl(sh);
}

shared_ptr<Shape> ToolView::loadMesh(const string &resourceDirectory,
                                     const char *primaryName,
                                     const char *fallbackName)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	string err;
	string path = resourceDirectory + "/" + primaryName;

	if (!tinyobj::LoadObj(shapes, materials, err, path.c_str())) {
		cerr << "LoadObj failed (" << path << "): " << err << endl;
		shapes.clear();
		materials.clear();
		err.clear();

		path = resourceDirectory + "/" + fallbackName;
		if (!tinyobj::LoadObj(shapes, materials, err, path.c_str())) {
			cerr << "LoadObj failed (" << path << "): " << err << endl;
			return nullptr;
		}
	}

	if (shapes.empty())
		return nullptr;

	tinyobj::shape_t &sh = shapes[0];
	if (sh.mesh.normals.empty())
		computeNormalsImpl(sh.mesh);
	ensureTexcoordsXZImpl(sh);

	auto mesh = make_shared<Shape>();
	mesh->createShape(sh);
	mesh->measure();
	mesh->init();
	return mesh;
}

bool ToolView::init(const string &resourceDirectory,
                    const shared_ptr<Program> &litProgram)
{
	prog_ = litProgram;
	if (!prog_) {
		cerr << "ToolView: missing lit texture shader program" << endl;
		return false;
	}

	texture_ = make_shared<Texture>();
	texture_->setFilename(resourceDirectory + "/finalguntex.png");
	texture_->setUnit(0);
	texture_->init();
	if (texture_->getID() == 0) {
		cerr << "ToolView: failed to load finalguntex.png" << endl;
		return false;
	}

	mesh_ = loadMesh(resourceDirectory, "gun1painted_colored.obj", "cube.obj");
	if (!mesh_) {
		cerr << "ToolView: failed to load gun1painted.obj and fallback cube.obj" << endl;
		return false;
	}
	return true;
}
void ToolView::triggerUse(){
	useAnimating_ = true;
	useAnimTime_ = 0.0f;
}


void ToolView::update(float dt){
	if (continuousUseActive_) {
		continuousUseTime_ += dt;
	} else {
		continuousUseTime_ = 0.0f;
	}

	if (useAnimating_) {
		useAnimTime_ += dt;
		if (useAnimTime_ >= useAnimDuration_) {
			useAnimTime_ = useAnimDuration_;
			useAnimating_ = false;
		}
	}
}


void ToolView::draw(int width,
                    int height,
                    const glm::mat4 &sceneView,
                    const glm::vec3 &cameraPos,
                    const glm::vec3 &cameraForward,
                    const glm::vec3 &cameraRight,
                    const glm::vec3 &cameraUp,
                    const glm::vec3 &lightPos,
                    const glm::vec3 &lightColor){
	if (!visible_ || !mesh_ || !prog_ || width <= 0 || height <= 0)
		return;

	float aspect = width / (float)height;
	glm::mat4 P = glm::perspective(glm::radians(fovDeg_), aspect, 0.01f, 10.0f);
	glm::mat4 V = sceneView;

	glm::vec3 localOffset = offset_;
	if (useBob_) {
		float swayX = 0.02f * sin(animTime_ * 4.0f) * moveBlend_;
		float swayY = 0.03f * sin(animTime_ * 10.0f) * moveBlend_;
		localOffset += glm::vec3(swayX, swayY, 0.0f);
	}

	float useT = 0.0f;
	if (useAnimDuration_ > 0.0f)
		useT = glm::clamp(useAnimTime_ / useAnimDuration_, 0.0f, 1.0f);

	float dip = sin(useT * 3.14159265f);

	glm::vec3 animOffset(0.0f);

	if (continuousUseActive_) {
		const float swing = sin(continuousUseTime_ * 12.0f);
		animOffset += cameraForward * (-0.06f * swing);
	} else {
		animOffset += cameraForward * (-0.10f * dip);
	}

	glm::vec3 toolWorldPos =
		cameraPos +
		cameraRight * localOffset.x +
		cameraUp * localOffset.y +
		cameraForward * localOffset.z + animOffset;

	glm::mat4 M(1.0f);
	M = glm::translate(M, toolWorldPos);

	glm::mat4 camBasis(1.0f);
	camBasis[0] = glm::vec4(cameraRight, 0.0f);
	camBasis[1] = glm::vec4(cameraUp, 0.0f);
	camBasis[2] = glm::vec4(-cameraForward, 0.0f);

	glm::mat4 localRot(1.0f);
	localRot = glm::rotate(localRot, glm::radians(rotationDeg_.x), glm::vec3(1, 0, 0));
	localRot = glm::rotate(localRot, glm::radians(rotationDeg_.y), glm::vec3(0, 1, 0));
	localRot = glm::rotate(localRot, glm::radians(rotationDeg_.z), glm::vec3(0, 0, 1));

	M = M * camBasis * localRot * glm::scale(glm::mat4(1.0f), scale_);

	prog_->bind();
	glUniformMatrix4fv(prog_->getUniform("P"), 1, GL_FALSE, glm::value_ptr(P));
	glUniformMatrix4fv(prog_->getUniform("V"), 1, GL_FALSE, glm::value_ptr(V));
	glUniformMatrix4fv(prog_->getUniform("M"), 1, GL_FALSE, glm::value_ptr(M));
	glUniform3fv(prog_->getUniform("lightPos"), 1, glm::value_ptr(lightPos));
	glUniform3fv(prog_->getUniform("camPos"), 1, glm::value_ptr(cameraPos));
	glUniform3fv(prog_->getUniform("lightColor"), 1, glm::value_ptr(lightColor));

	glUniform3f(prog_->getUniform("matAmbient"), 0.35f, 0.35f, 0.35f);
	glUniform3f(prog_->getUniform("matDiffuse"), 0.95f, 0.95f, 0.95f);
	glUniform3f(prog_->getUniform("matSpecular"), 0.35f, 0.35f, 0.35f);
	glUniform1f(prog_->getUniform("shininess"), 20.0f);
	glUniform3f(prog_->getUniform("tintColor"), 1.0f, 1.0f, 1.0f);
	glUniform1f(prog_->getUniform("emissiveStrength"), 0.0f);
	glUniform3f(prog_->getUniform("emissiveColor"), 0.0f, 0.0f, 0.0f);
	glUniform1i(prog_->getUniform("useEmissiveMap"), 0);
	texture_->bind(prog_->getUniform("Texture0"));

	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);
	mesh_->draw(prog_);
	texture_->unbind();
	prog_->unbind();
}
