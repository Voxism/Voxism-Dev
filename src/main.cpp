/*
 * CSC 476 Lab 1 — Simple 3D collection game
 * Third-person orbit cam, skybox, glTF character (bind pose + procedural motion), post FX.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "CharacterController.h"
#include "BirdFlock.h"
#include "GameWorld.h"
#include "GltfMesh.h"
#include "Skybox.h"
#include "camera/FirstPersonCamera.h"
#include "camera/FreeCamera.h"
#include "GLSL.h"
#include "MatrixStack.h"
#include "Program.h"
#include "Shape.h"
#include "Texture.h"
#include "WindowManager.h"
#include "world/ChunkManager.h"
#include "world/Materials.h"
#include "Tool.h"
#include "Crosshair.h"
#include "tools/ToolManager.h"
#include "tools/ToolPreviewRenderer.h"
#include "ui/MaterialRadialMenu.h"
#include "ui/RadialToolMenu.h"
#include "audio/SoundtrackPlayer.h"
#include "particles/BreakParticleSystem.h"
#include "rendering/CascadedShadowMap.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include "camera/Frustum.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;
using namespace glm;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void computeNormals(tinyobj::mesh_t &mesh)
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

/** Ensure mesh has UVs for textured shading (planar XZ, course framework meshes often omit coords). */
static void ensureTexcoordsXZ(tinyobj::shape_t &sh)
{
	size_t nv = sh.mesh.positions.size() / 3;
	if (nv == 0)
		return;
	sh.mesh.texcoords.resize(nv * 2);
	for (size_t i = 0; i < nv; i++) {
		float x = sh.mesh.positions[3 * i + 0];
		float z = sh.mesh.positions[3 * i + 2];
		sh.mesh.texcoords[2 * i + 0] = x * 0.12f + 0.5f;
		sh.mesh.texcoords[2 * i + 1] = z * 0.12f + 0.5f;
	}
}

namespace {
ImU32 uiRgba(int r, int g, int b, float alpha)
{
	return IM_COL32(r, g, b, static_cast<int>(255.0f * glm::clamp(alpha, 0.0f, 1.0f)));
}

ImU32 uiColorFromVec3(const glm::vec3 &color, float alpha)
{
	return IM_COL32(
		static_cast<int>(255.0f * glm::clamp(color.r, 0.0f, 1.0f)),
		static_cast<int>(255.0f * glm::clamp(color.g, 0.0f, 1.0f)),
		static_cast<int>(255.0f * glm::clamp(color.b, 0.0f, 1.0f)),
		static_cast<int>(255.0f * glm::clamp(alpha, 0.0f, 1.0f)));
}

glm::vec3 shadeHudColor(const glm::vec3 &color, float factor, float lift = 0.0f)
{
	return glm::clamp(color * factor + glm::vec3(lift), glm::vec3(0.0f), glm::vec3(1.0f));
}
}

/** Tiled procedural ground ( RGB8, power-of-2 ), style similar to tiled terrain textures. */
static GLuint makeGroundCheckerTexture(int size)
{
	vector<uint8_t> px(static_cast<size_t>(size * size * 3));
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			bool c = (((x / 32) + (y / 32)) & 1) == 0;
			int i = (y * size + x) * 3;
			if (c) {
				px[static_cast<size_t>(i)] = 52;
				px[static_cast<size_t>(i + 1)] = 72;
				px[static_cast<size_t>(i + 2)] = 48;
			} else {
				px[static_cast<size_t>(i)] = 32;
				px[static_cast<size_t>(i + 1)] = 48;
				px[static_cast<size_t>(i + 2)] = 30;
			}
		}
	}
	GLuint tid = 0;
	glGenTextures(1, &tid);
	glBindTexture(GL_TEXTURE_2D, tid);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, px.data());
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tid;
}

static std::string shaderPath(const std::string& resourceDir,
                               const std::string& category,
                               const std::string& filename) {
    return resourceDir + "/shaders/" + category + "/" + filename;
}

// ---------------------------------------------------------------------------
// CPU-only helper: project the world-space sun position into screen UV and
// classify visibility. Pure CPU code (no GL types, no GL calls) so it can be
// unit-tested without an OpenGL context.
//
// Returns:
//   sunScreen      — UV in [0, 1] when sunVisible; sentinel (0.5, 0.55) when not
//   sunVisible     — true iff sunClip.w > 0.001f AND sunVisibleFade > 0
//   sunVisibleFade — 0..1 multiplier applied to godrayStrength at composite time
//
// kMargin defines the smooth-fade window (in UV space) around [0, 1] so the
// godray contribution does not pop on/off as the sun crosses the frustum edge.
// ---------------------------------------------------------------------------
namespace {

struct SunProjection {
    glm::vec2 sunScreen;      // UV in [0,1]; valid only when sunVisible
    bool      sunVisible;     // sunClip.w > 0.001f AND sunVisibleFade > 0
    float     sunVisibleFade; // 0..1; multiplies godrayStrength
};

SunProjection projectSunToScreen(const glm::mat4& P, const glm::mat4& V,
                                 const glm::vec3& sunWorld,
                                 float kMargin = 0.15f)
{
    SunProjection out;
    out.sunScreen      = glm::vec2(0.5f, 0.55f); // sentinel matches pre-fix default
    out.sunVisible     = false;
    out.sunVisibleFade = 0.0f;

    const glm::vec4 sunClip = P * V * glm::vec4(sunWorld, 1.0f);
    if (sunClip.w > 0.001f) {
        const glm::vec2 uv((sunClip.x / sunClip.w) * 0.5f + 0.5f,
                           (sunClip.y / sunClip.w) * 0.5f + 0.5f);

        // Per-axis margin-clamp fade. Equals 1.0 strictly inside [0,1], decays
        // linearly to 0 across the kMargin window past each boundary.
        const float kSafeMargin = (kMargin > 0.0f) ? kMargin : 1e-6f;
        const float fx = glm::clamp(
            1.0f - std::max(0.0f, std::max(-uv.x, uv.x - 1.0f)) / kSafeMargin,
            0.0f, 1.0f);
        const float fy = glm::clamp(
            1.0f - std::max(0.0f, std::max(-uv.y, uv.y - 1.0f)) / kSafeMargin,
            0.0f, 1.0f);

        const float fade = fx * fy;
        if (fade > 0.0f) {
            out.sunScreen      = uv;
            out.sunVisibleFade = fade;
            out.sunVisible     = true;
        }
    }

    return out;
}

} // namespace

struct PostProcessToggle {
    bool godRaysEnabled  = true;
    bool bloomEnabled    = false;
    bool ssaoEnabled     = true;
    float godrayStrength = 0.8f;
    float bloomStrength  = 1.6f;
    float ssaoRadius     = 0.75f;
    float ssaoBias       = 0.025f;
    float ssaoIntensity  = 1.0f;
    bool  taaEnabled     = false;
    float taaBlend       = 0.1f;
};

class Application : public EventCallbacks {
public:
	WindowManager *windowManager = nullptr;

	~Application()
	{
		teardownPostProcess();
		shadowMap_.destroy();
		if (groundTexGl_)
			glDeleteTextures(1, &groundTexGl_);
		radialToolMenu_.destroy();
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void init(const string &resourceDirectory)
	{
		resourceDir_ = resourceDirectory;
		GLSL::checkVersion();
		cout << "OpenGL Version:" << glGetString(GL_VERSION) << endl;
		glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_PROGRAM_POINT_SIZE);

		glEnable(GL_CULL_FACE);

		sunWorld_ = vec3(12.0f, 30.0f, 20.0f);

		chunkManager->loadFloraAssets(resourceDirectory);
		if (!birdFlock_.init(resourceDirectory)) {
			cerr << "Bird asset load failed." << endl;
		}

		chunkManager->generateChunks(vec3(0,0,0));

		// Materials initialization
		GLuint materialsBindingPoint = 0;
		materials->init(materialsBindingPoint);

		// ChunkProg definition
		chunkProg_ = make_shared<Program>();
		chunkProg_->setVerbose(true);
		chunkProg_->setShaderNames(
			shaderPath(resourceDirectory, "chunk", "chunk_vert.glsl"),
			shaderPath(resourceDirectory, "chunk", "chunk_frag.glsl")
		);
		if (!chunkProg_->init())
			cerr << "texProg failed to link" << endl;
		chunkProg_->addUniform("P");
		chunkProg_->addUniform("V");
		chunkProg_->addUniform("M");
		chunkProg_->addUniform("chunkWorldPos");
		chunkProg_->addUniform("chunkSizeMeters");
		chunkProg_->addUniform("voxelSizeMeters");
		chunkProg_->addUniformBuffObj("materials", materialsBindingPoint);
		chunkProg_->addUniform("matIDTex");
		chunkProg_->addUniform("lightPos");
		chunkProg_->addUniform("lightDir");
		chunkProg_->addUniform("camPos");
		chunkProg_->addUniform("lightColor");
		chunkProg_->addUniform("shadowMap");
		chunkProg_->addUniform("lightSpaceMatrices[0]");
		chunkProg_->addUniform("cascadeEnds[0]");
		chunkProg_->addUniform("cascadeCount");
		chunkProg_->addUniform("shadowEnabled");
		chunkProg_->addUniform("shadowMapSize");
		chunkProg_->addUniform("shadowStrength");
		chunkProg_->addUniform("minShadowVisibility");
		chunkProg_->addUniform("shadowBlurTexels");
		chunkProg_->addUniform("normalBiasVoxels");
		chunkProg_->addAttribute("vertPos");
		chunkProg_->addAttribute("normalID");

		birdProg_ = make_shared<Program>();
		birdProg_->setVerbose(true);
		birdProg_->setShaderNames(
			shaderPath(resourceDirectory, "scene", "tool_vertcolor_vert.glsl"),
			shaderPath(resourceDirectory, "scene", "tool_vertcolor_frag.glsl")
		);
		if (!birdProg_->init())
			cerr << "birdProg failed to link" << endl;
		birdProg_->addUniform("P");
		birdProg_->addUniform("V");
		birdProg_->addUniform("M");
		birdProg_->addUniform("lightPos");
		birdProg_->addUniform("camPos");
		birdProg_->addUniform("lightColor");
		birdProg_->addUniform("matAmbient");
		birdProg_->addUniform("matDiffuse");
		birdProg_->addUniform("matSpecular");
		birdProg_->addUniform("shininess");
		birdProg_->addUniform("tintColor");
		birdProg_->addAttribute("vertPos");
		birdProg_->addAttribute("vertNor");
		birdProg_->addAttribute("vertCol");

		// Lit texture pass (world-space Blinn-Phong, 471-style texture sampling)
		texProg_ = make_shared<Program>();
		texProg_->setVerbose(true);
		texProg_->setShaderNames(shaderPath(resourceDirectory, "scene", "tex_lit_world_vert.glsl"),
		                         shaderPath(resourceDirectory, "scene", "tex_lit_world_frag.glsl"));
		if (!texProg_->init())
			cerr << "texProg failed to link" << endl;
		texProg_->addUniform("P");
		texProg_->addUniform("V");
		texProg_->addUniform("M");
		texProg_->addUniform("Texture0");
		texProg_->addUniform("lightPos");
		texProg_->addUniform("camPos");
		texProg_->addUniform("lightColor");
	texProg_->addUniform("matAmbient");
	texProg_->addUniform("matDiffuse");
	texProg_->addUniform("matSpecular");
	texProg_->addUniform("shininess");
	texProg_->addUniform("tintColor");
		texProg_->addUniform("emissiveStrength");
		texProg_->addUniform("emissiveColor");
		texProg_->addUniform("useEmissiveMap");
	texProg_->addAttribute("vertPos");
	texProg_->addAttribute("vertNor");
	texProg_->addAttribute("vertTex");

		groundTexGl_ = makeGroundCheckerTexture(256);

		skybox_.init(resourceDirectory, "skybox", "png");

		world_.reset();
		fpvCamera.SetChunkManager(chunkManager.get());

		initPostProcessShaders(resourceDirectory);
		initShadowResources(resourceDirectory);

		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(windowManager->getHandle(), &fbW, &fbH);
		setupPostProcess(fbW, fbH);

		lastStatsPrint_ = 0.0;

		toolView_.init(resourceDirectory, texProg_);
		toolView_.setOffset(vec3(1.0f, -2.5f, 1.5f));
		toolView_.setRotationDeg(vec3(4.0f, 180.0f, 0.0f));
		toolView_.setScale(vec3(0.05f, 0.05f, 0.05f));
		toolView_.setFov(55.0f);
		if (!previewRenderer_.init(resourceDirectory))
			cerr << "previewRenderer init failed" << endl;
		if (!breakParticles_.init(resourceDirectory))
			cerr << "breakParticles init failed" << endl;

		crosshair_.init(resourceDirectory);
		crosshair_.setSize(8.0f);
		crosshair_.setThickness(2.0f);
		crosshair_.setGap(5.0f);
		crosshair_.setColor(vec3(1.0f, 1.0f, 1.0f));

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		(void)io;

		ImGui::StyleColorsDark();
		loadHudFont();

		const char *glsl_version = "#version 330";
		ImGui_ImplGlfw_InitForOpenGL(windowManager->getHandle(), true);
		ImGui_ImplOpenGL3_Init(glsl_version);
		if (!radialToolMenu_.init(resourceDirectory))
			cerr << "radialToolMenu init failed" << endl;
		initMaterialRadialMenu();

		// --- Soundtrack: start looping background music. Non-fatal on failure. ---
		if (soundtrack_.init()) {
			soundtrack_.setVolume(musicVolume_);
			const string trackPath =
				resourceDirectory + "/soundtrack/Porter Robinson - Lifelike (Official Audio).mp3";
			if (!soundtrack_.playLoop(trackPath)) {
				cerr << "Soundtrack: failed to start '" << trackPath << "'" << endl;
			}

			// --- SFX: pre-decode the 4 break/place stone clips into voice pools. ---
			for (int i = 1; i <= 4; ++i) {
				const string idB = "break" + std::to_string(i);
				const string idP = "place" + std::to_string(i);
				const string pathB = resourceDirectory + "/sfx/break/stone" + std::to_string(i) + ".ogg";
				const string pathP = resourceDirectory + "/sfx/place/stone" + std::to_string(i) + ".ogg";
				if (soundtrack_.loadSfx(idB, pathB)) breakSfxIds_.push_back(idB);
				if (soundtrack_.loadSfx(idP, pathP)) placeSfxIds_.push_back(idP);
			}
		}
	}

	/** Play a random break sfx (from sfx/break/stone1..4.ogg). Non-fatal. */
	void playBreakSfx()
	{
		if (breakSfxIds_.empty()) return;
		std::uniform_int_distribution<size_t> dist(0, breakSfxIds_.size() - 1);
		soundtrack_.playSfx(breakSfxIds_[dist(sfxRng_)]);
	}

	/** Play a random place sfx (from sfx/place/stone1..4.ogg). Non-fatal. */
	void playPlaceSfx()
	{
		if (placeSfxIds_.empty()) return;
		std::uniform_int_distribution<size_t> dist(0, placeSfxIds_.size() - 1);
		soundtrack_.playSfx(placeSfxIds_[dist(sfxRng_)]);
	}

	void spawnBreakParticles(const ChunkEditSummary &editSummary)
	{
		if (editSummary.valid &&
			editSummary.action == ChunkEditAction::Delete &&
			editSummary.affectedVoxelCount > 0) {
			breakParticles_.spawnDeleteBurst(editSummary, chunkManager->voxSizeMeters);
		}
	}

	void spawnLandingParticles()
	{
		FirstPersonCamera::LandingEvent landing = fpvCamera.ConsumeLandingEvent();
		if (!landing.triggered) {
			return;
		}

		const glm::vec3 feetPos = fpvCamera.GetFeetPos();
		uint8_t materialID = Materials::Stone;
		const glm::vec3 probeStart = feetPos - glm::vec3(0.0f, chunkManager->voxSizeMeters * 0.25f, 0.0f);
		const glm::ivec3 startVoxel = chunkManager->worldToVoxel(probeStart);
		for (int offset = 0; offset < 8; ++offset) {
			const glm::ivec3 probeVoxel = startVoxel - glm::ivec3(0, offset, 0);
			if (chunkManager->isVoxelOccupied(probeVoxel)) {
				materialID = chunkManager->voxelMaterial(probeVoxel);
				break;
			}
		}
		breakParticles_.spawnLandingBurst(feetPos, landing.fallHeight, chunkManager->voxSizeMeters, materialID);
	}

	glm::vec3 materialColorVariant(int materialIndex, float offset) const
	{
		return glm::clamp(
			Materials::paletteColor(materialIndex) + glm::vec3(offset),
			glm::vec3(0.0f),
			glm::vec3(1.0f));
	}

	MaterialMenuOption makeMaterialMenuOption(int materialIndex) const
	{
		MaterialMenuOption option;
		option.materialIndex = materialIndex;
		option.label = Materials::paletteName(materialIndex);
		option.swatchColors = {{
			materialColorVariant(materialIndex, -0.028f),
			materialColorVariant(materialIndex, -0.010f),
			materialColorVariant(materialIndex,  0.012f),
			materialColorVariant(materialIndex,  0.028f),
		}};
		return option;
	}

	void initMaterialRadialMenu()
	{
		std::vector<MaterialMenuOption> options;
		options.push_back(makeMaterialMenuOption(Materials::Grass));
		options.push_back(makeMaterialMenuOption(Materials::Dirt));
		options.push_back(makeMaterialMenuOption(Materials::Stone));
		options.push_back(makeMaterialMenuOption(Materials::Brick));
		options.push_back(makeMaterialMenuOption(Materials::Sand));
		options.push_back(makeMaterialMenuOption(Materials::Gold));
		materialRadialMenu_.init(options);
	}

	void loadHudFont()
	{
		struct FontCandidate {
			const char *path;
			float size;
		};

		const FontCandidate candidates[] = {
			{"C:/Windows/Fonts/GIL_____.TTF", 24.0f},
			{"C:/Windows/Fonts/segoeui.ttf", 23.0f},
			{"C:/Windows/Fonts/arial.ttf", 23.0f},
			{"/System/Library/Fonts/Supplemental/Gill Sans.ttc", 24.0f},
			{"/System/Library/Fonts/Supplemental/Avenir Next.ttc", 23.0f},
		};

		ImGuiIO &io = ImGui::GetIO();
		for (const FontCandidate &candidate : candidates) {
			std::ifstream file(candidate.path);
			if (!file.good()) {
				continue;
			}

			hudFont_ = io.Fonts->AddFontFromFileTTF(candidate.path, candidate.size);
			if (hudFont_) {
				return;
			}
		}
		hudFont_ = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
	}

	glm::vec3 currentSunDirection() const
	{
		if (glm::dot(sunWorld_, sunWorld_) > 0.0001f) {
			return glm::normalize(sunWorld_);
		}
		return glm::normalize(glm::vec3(0.35f, 0.85f, 0.25f));
	}

	void rotateSunAroundWorldY(float radians)
	{
		const float c = std::cos(radians);
		const float s = std::sin(radians);
		const float x = sunWorld_.x;
		const float z = sunWorld_.z;
		sunWorld_.x = x * c - z * s;
		sunWorld_.z = x * s + z * c;
	}

	void initShadowResources(const string &resourceDirectory)
	{
		shadowDepthProg_ = make_shared<Program>();
		shadowDepthProg_->setVerbose(true);
		shadowDepthProg_->setShaderNames(shaderPath(resourceDirectory, "shadow", "chunk_depth_vert.glsl"),
		                                 shaderPath(resourceDirectory, "shadow", "chunk_depth_frag.glsl"));
		if (!shadowDepthProg_->init()) {
			cerr << "shadowDepthProg failed — shadows disabled" << endl;
			shadowSettings_.enabled = false;
		} else {
			shadowDepthProg_->addUniform("lightSpaceMatrix");
			shadowDepthProg_->addAttribute("vertPos");
		}

		if (!shadowMap_.init(shadowSettings_.resolution, shadowSettings_.cascadeCount)) {
			cerr << "shadowMap init failed — shadows disabled" << endl;
			shadowSettings_.enabled = false;
		} else {
			chunkManager->markShadowMapsDirty();
		}
	}

	void initPostProcessShaders(const string &resourceDirectory)
	{
		godrayProg_ = make_shared<Program>();
		godrayProg_->setVerbose(true);
		godrayProg_->setShaderNames(shaderPath(resourceDirectory, "postprocess", "screen_vert.glsl"),
		                           shaderPath(resourceDirectory, "postprocess", "godray_frag.glsl"));
		if (!godrayProg_->init())
			cerr << "godrayProg failed" << endl;
		godrayProg_->addUniform("sceneTex");
		godrayProg_->addUniform("sunPos");
		godrayProg_->addUniform("time");

		bloomBrightProg_ = make_shared<Program>();
		bloomBrightProg_->setVerbose(true);
		bloomBrightProg_->setShaderNames(shaderPath(resourceDirectory, "postprocess", "screen_vert.glsl"),
		                                 shaderPath(resourceDirectory, "postprocess", "bloom_bright_frag.glsl"));
		if (!bloomBrightProg_->init())
			cerr << "bloomBrightProg failed" << endl;
		bloomBrightProg_->addUniform("sceneTex");

		blurProg_ = make_shared<Program>();
		blurProg_->setVerbose(true);
		blurProg_->setShaderNames(shaderPath(resourceDirectory, "postprocess", "screen_vert.glsl"),
		                          shaderPath(resourceDirectory, "postprocess", "blur_frag.glsl"));
		if (!blurProg_->init())
			cerr << "blurProg failed" << endl;
		blurProg_->addUniform("image");
		blurProg_->addUniform("horizontal");
		blurProg_->addUniform("texelSize");

		compositeProg_ = make_shared<Program>();
		compositeProg_->setVerbose(true);
		compositeProg_->setShaderNames(shaderPath(resourceDirectory, "postprocess", "screen_vert.glsl"),
		                               shaderPath(resourceDirectory, "postprocess", "composite_frag.glsl"));
		if (!compositeProg_->init())
			cerr << "compositeProg failed" << endl;
		compositeProg_->addUniform("sceneTex");
		compositeProg_->addUniform("godrayTex");
		compositeProg_->addUniform("bloomTex");
		compositeProg_->addUniform("godrayStrength");
		compositeProg_->addUniform("bloomStrength");
		compositeProg_->addUniform("ssaoTex");
		compositeProg_->addUniform("ssaoIntensity");
		compositeProg_->addUniform("ssaoEnabled");

		// SSAO pass shader
		ssaoProg_ = make_shared<Program>();
		ssaoProg_->setVerbose(true);
		ssaoProg_->setShaderNames(shaderPath(resourceDirectory, "postprocess", "screen_vert.glsl"),
		                          shaderPath(resourceDirectory, "postprocess", "ssao_frag.glsl"));
		if (!ssaoProg_->init()) {
			cerr << "ssaoProg failed — SSAO disabled" << endl;
			postToggles_.ssaoEnabled = false;
		} else {
			ssaoProg_->addUniform("depthTex");
			ssaoProg_->addUniform("noiseTex");
			for (int i = 0; i < 64; ++i)
				ssaoProg_->addUniform(("samples[" + to_string(i) + "]").c_str());
			ssaoProg_->addUniform("projection");
			ssaoProg_->addUniform("invProjection");
			ssaoProg_->addUniform("noiseScale");
			ssaoProg_->addUniform("radius");
			ssaoProg_->addUniform("bias");
			ssaoProg_->addUniform("texelSize");
		}

		// SSAO blur shader
		ssaoBlurProg_ = make_shared<Program>();
		ssaoBlurProg_->setVerbose(true);
		ssaoBlurProg_->setShaderNames(shaderPath(resourceDirectory, "postprocess", "screen_vert.glsl"),
		                               shaderPath(resourceDirectory, "postprocess", "ssao_blur_frag.glsl"));
		if (!ssaoBlurProg_->init()) {
			cerr << "ssaoBlurProg failed — SSAO disabled" << endl;
			postToggles_.ssaoEnabled = false;
		} else {
			ssaoBlurProg_->addUniform("ssaoInput");
			ssaoBlurProg_->addUniform("depthTex");
			ssaoBlurProg_->addUniform("texelSize");
			ssaoBlurProg_->addUniform("invProjection");
		}

		// TAA resolve shader
		taaProg_ = make_shared<Program>();
		taaProg_->setVerbose(true);
		taaProg_->setShaderNames(shaderPath(resourceDirectory, "postprocess", "screen_vert.glsl"),
		                         shaderPath(resourceDirectory, "postprocess", "taa_resolve_frag.glsl"));
		if (!taaProg_->init()) {
			cerr << "taaProg failed — TAA disabled" << endl;
			postToggles_.taaEnabled = false;
		} else {
			taaProg_->addUniform("currentTex");
			taaProg_->addUniform("historyTex");
			taaProg_->addUniform("depthTex");
			taaProg_->addUniform("invViewProj");
			taaProg_->addUniform("prevViewProj");
			taaProg_->addUniform("texelSize");
			taaProg_->addUniform("historyValid");
			taaProg_->addUniform("blendFactor");
		}
	}

	void teardownPostProcess()
	{
		if (quadVao_) {
			glDeleteVertexArrays(1, &quadVao_);
			quadVao_ = 0;
		}
		if (quadVbo_) {
			glDeleteBuffers(1, &quadVbo_);
			quadVbo_ = 0;
		}
		if (sceneFBO_) {
			glDeleteFramebuffers(1, &sceneFBO_);
			sceneFBO_ = 0;
		}
		if (sceneColorTex_) {
			glDeleteTextures(1, &sceneColorTex_);
			sceneColorTex_ = 0;
		}
		if (sceneDepthTex_) {
			glDeleteTextures(1, &sceneDepthTex_);
			sceneDepthTex_ = 0;
		}
		if (sceneDepthRBO_) {
			glDeleteRenderbuffers(1, &sceneDepthRBO_);
			sceneDepthRBO_ = 0;
		}
		if (godrayFBO_) {
			glDeleteFramebuffers(1, &godrayFBO_);
			godrayFBO_ = 0;
		}
		if (godrayTex_) {
			glDeleteTextures(1, &godrayTex_);
			godrayTex_ = 0;
		}
		for (int i = 0; i < 2; i++) {
			if (pingpongFBO_[i]) {
				glDeleteFramebuffers(1, &pingpongFBO_[i]);
				pingpongFBO_[i] = 0;
			}
			if (pingpongTex_[i]) {
				glDeleteTextures(1, &pingpongTex_[i]);
				pingpongTex_[i] = 0;
			}
		}
		if (ssaoFBO_) { glDeleteFramebuffers(1, &ssaoFBO_); ssaoFBO_ = 0; }
		if (ssaoTex_) { glDeleteTextures(1, &ssaoTex_); ssaoTex_ = 0; }
		if (ssaoBlurFBO_) { glDeleteFramebuffers(1, &ssaoBlurFBO_); ssaoBlurFBO_ = 0; }
		if (ssaoBlurTex_) { glDeleteTextures(1, &ssaoBlurTex_); ssaoBlurTex_ = 0; }
		if (noiseTex_) { glDeleteTextures(1, &noiseTex_); noiseTex_ = 0; }
		if (taaFBO_) { glDeleteFramebuffers(1, &taaFBO_); taaFBO_ = 0; }
		for (int i = 0; i < 2; i++) {
			if (taaHistoryTex_[i]) { glDeleteTextures(1, &taaHistoryTex_[i]); taaHistoryTex_[i] = 0; }
		}
		taaHistoryValid_ = false;
		postW_ = postH_ = 0;
	}

	void setupPostProcess(int w, int h)
	{
		if (w <= 0 || h <= 0)
			return;
		if (w == postW_ && h == postH_ && quadVao_)
			return;

		teardownPostProcess();
		postW_ = w;
		postH_ = h;

		float quad[] = {
			-1.f, -1.f, 0.f, 0.f,
			1.f, -1.f, 1.f, 0.f,
			1.f, 1.f, 1.f, 1.f,
			-1.f, -1.f, 0.f, 0.f,
			1.f, 1.f, 1.f, 1.f,
			-1.f, 1.f, 0.f, 1.f,
		};
		glGenVertexArrays(1, &quadVao_);
		glGenBuffers(1, &quadVbo_);
		glBindVertexArray(quadVao_);
		glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
		glBindVertexArray(0);

		glGenFramebuffers(1, &sceneFBO_);
		glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO_);
		glGenTextures(1, &sceneColorTex_);
		glBindTexture(GL_TEXTURE_2D, sceneColorTex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTex_, 0);

		// Depth texture (required for SSAO depth reads)
		glGenTextures(1, &sceneDepthTex_);
		glBindTexture(GL_TEXTURE_2D, sceneDepthTex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTex_, 0);

		// Check FBO completeness; fall back to renderbuffer if depth texture unsupported
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			cerr << "[SSAO] Scene FBO incomplete with depth texture — falling back to renderbuffer, SSAO disabled" << endl;
			glDeleteTextures(1, &sceneDepthTex_);
			sceneDepthTex_ = 0;
			glGenRenderbuffers(1, &sceneDepthRBO_);
			glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRBO_);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRBO_);
			postToggles_.ssaoEnabled = false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glGenFramebuffers(1, &godrayFBO_);
		glBindFramebuffer(GL_FRAMEBUFFER, godrayFBO_);
		glGenTextures(1, &godrayTex_);
		glBindTexture(GL_TEXTURE_2D, godrayTex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, godrayTex_, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		int hw = std::max(1, w / 2);
		int hh = std::max(1, h / 2);
		glGenFramebuffers(2, pingpongFBO_);
		glGenTextures(2, pingpongTex_);
		for (int i = 0; i < 2; i++) {
			glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO_[i]);
			glBindTexture(GL_TEXTURE_2D, pingpongTex_[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, hw, hh, 0, GL_RGB, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTex_[i], 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// SSAO raw output FBO
		glGenFramebuffers(1, &ssaoFBO_);
		glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO_);
		glGenTextures(1, &ssaoTex_);
		glBindTexture(GL_TEXTURE_2D, ssaoTex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoTex_, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			cerr << "[SSAO] SSAO FBO incomplete — SSAO disabled" << endl;
			postToggles_.ssaoEnabled = false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// SSAO blur FBO
		glGenFramebuffers(1, &ssaoBlurFBO_);
		glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO_);
		glGenTextures(1, &ssaoBlurTex_);
		glBindTexture(GL_TEXTURE_2D, ssaoBlurTex_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBlurTex_, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			cerr << "[SSAO] SSAO blur FBO incomplete — SSAO disabled" << endl;
			postToggles_.ssaoEnabled = false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// TAA FBO + ping-pong history textures (full res, bilinear for history fetch)
		glGenFramebuffers(1, &taaFBO_);
		glGenTextures(2, taaHistoryTex_);
		for (int i = 0; i < 2; i++) {
			glBindTexture(GL_TEXTURE_2D, taaHistoryTex_[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, taaFBO_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, taaHistoryTex_[0], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			cerr << "[TAA] TAA FBO incomplete — TAA disabled" << endl;
			postToggles_.taaEnabled = false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		taaHistoryIdx_ = 0;
		taaHistoryValid_ = false;

		// Generate SSAO kernel and noise texture (once)
		if (ssaoKernel_.empty())
			ssaoKernel_ = generateSSAOKernel(64);
		if (noiseTex_ == 0)
			noiseTex_ = generateNoiseTexture();
	}

	std::vector<glm::vec3> generateSSAOKernel(int sampleCount = 64)
	{
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		std::default_random_engine gen;
		std::vector<glm::vec3> kernel;
		kernel.reserve(sampleCount);

		for (int i = 0; i < sampleCount; ++i) {
			glm::vec3 sample(
				dist(gen) * 2.0f - 1.0f,   // x: [-1, 1]
				dist(gen) * 2.0f - 1.0f,   // y: [-1, 1]
				dist(gen)                    // z: [0, 1] — hemisphere
			);
			sample = glm::normalize(sample);
			sample *= dist(gen);  // random length [0, 1]

			// Accelerating interpolation: bias samples toward origin
			float scale = (float)i / (float)sampleCount;
			scale = 0.1f + scale * scale * 0.9f;  // lerp(0.1, 1.0, scale^2)
			sample *= scale;

			kernel.push_back(sample);
		}
		return kernel;
	}

	GLuint generateNoiseTexture()
	{
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		std::default_random_engine gen;
		std::vector<glm::vec3> noiseData;
		noiseData.reserve(16);
		for (int i = 0; i < 16; ++i) {
			glm::vec3 noise(
				dist(gen) * 2.0f - 1.0f,
				dist(gen) * 2.0f - 1.0f,
				0.0f
			);
			noise = glm::normalize(noise);
			noiseData.push_back(noise);
		}
		GLuint tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noiseData.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glBindTexture(GL_TEXTURE_2D, 0);
		return tex;
	}

	void drawFullscreenQuad()
	{
		glDisable(GL_DEPTH_TEST);
		glBindVertexArray(quadVao_);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
		glEnable(GL_DEPTH_TEST);
	}

	void renderSSAOPass(const glm::mat4& P, const glm::mat4& V)
	{
		(void)V;  // V not needed for SSAO (depth is already in view space)

		// --- SSAO raw pass ---
		glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO_);
		glViewport(0, 0, postW_, postH_);
		glClear(GL_COLOR_BUFFER_BIT);
		ssaoProg_->bind();

		// Depth texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sceneDepthTex_);
		glUniform1i(ssaoProg_->getUniform("depthTex"), 0);

		// Noise texture
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, noiseTex_);
		glUniform1i(ssaoProg_->getUniform("noiseTex"), 1);

		// Kernel samples
		for (int i = 0; i < 64 && i < (int)ssaoKernel_.size(); ++i) {
			string name = "samples[" + to_string(i) + "]";
			glUniform3fv(ssaoProg_->getUniform(name.c_str()), 1, glm::value_ptr(ssaoKernel_[i]));
		}

		// Matrices
		glUniformMatrix4fv(ssaoProg_->getUniform("projection"), 1, GL_FALSE, glm::value_ptr(P));
		glm::mat4 invP = glm::inverse(P);
		glUniformMatrix4fv(ssaoProg_->getUniform("invProjection"), 1, GL_FALSE, glm::value_ptr(invP));

		// Scale and tuning
		glUniform2f(ssaoProg_->getUniform("noiseScale"), (float)postW_ / 4.0f, (float)postH_ / 4.0f);
		glUniform1f(ssaoProg_->getUniform("radius"), postToggles_.ssaoRadius);
		glUniform1f(ssaoProg_->getUniform("bias"), postToggles_.ssaoBias);
		glUniform2f(ssaoProg_->getUniform("texelSize"), 1.0f / (float)postW_, 1.0f / (float)postH_);

		drawFullscreenQuad();
		ssaoProg_->unbind();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// --- SSAO blur pass ---
		glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO_);
		glViewport(0, 0, postW_, postH_);
		glClear(GL_COLOR_BUFFER_BIT);
		ssaoBlurProg_->bind();

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, ssaoTex_);
		glUniform1i(ssaoBlurProg_->getUniform("ssaoInput"), 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, sceneDepthTex_);
		glUniform1i(ssaoBlurProg_->getUniform("depthTex"), 1);
		glUniform2f(ssaoBlurProg_->getUniform("texelSize"), 1.0f / (float)postW_, 1.0f / (float)postH_);
		glUniformMatrix4fv(ssaoBlurProg_->getUniform("invProjection"), 1, GL_FALSE, glm::value_ptr(invP));

		drawFullscreenQuad();
		ssaoBlurProg_->unbind();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void setMovementKeyState(int key, bool down)
	{
		if (key == GLFW_KEY_W)
			keyW_ = down;
		else if (key == GLFW_KEY_S)
			keyS_ = down;
		else if (key == GLFW_KEY_A)
			keyA_ = down;
		else if (key == GLFW_KEY_D)
			keyD_ = down;
		else if (key == GLFW_KEY_Q)
			keySunLeft_ = down;
		else if (key == GLFW_KEY_E)
			keySunRight_ = down;
	}

	void applyCameraKeyState(int key, int action)
	{
		const bool sunTestKey = (key == GLFW_KEY_Q || key == GLFW_KEY_E);
		if (!sunTestKey) {
			camera->ProcessKeypress(key, action);
		}
	}

	bool anySelectorOpen() const
	{
		return radialToolMenu_.isOpen() || materialRadialMenu_.isOpen();
	}

	void openRadialToolMenu(GLFWwindow *window)
	{
		if (anySelectorOpen()) {
			return;
		}

		radialMenuRestoreMouseLocked_ = mouseLocked_;
		leftMouseDown_ = false;
		rightMouseDown_ = false;
		toolManager_.endAction(ToolMode::Build);
		toolManager_.endAction(ToolMode::Delete);
		toolView_.setContinuousUseActive(false);

		mouseLocked_ = false;
		firstMouse_ = true;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
		}

		int windowWidth = 0, windowHeight = 0;
		glfwGetWindowSize(window, &windowWidth, &windowHeight);
		const ImVec2 center(static_cast<float>(windowWidth) * 0.5f, static_cast<float>(windowHeight) * 0.5f);
		lastMouseX_ = center.x;
		lastMouseY_ = center.y;
		glfwSetCursorPos(window, center.x, center.y);
		radialToolMenu_.open(center, toolManager_.activeToolKind());
		radialToolMenu_.updateMouse(center);
	}

	void openMaterialRadialMenu(GLFWwindow *window)
	{
		if (anySelectorOpen()) {
			return;
		}

		radialMenuRestoreMouseLocked_ = mouseLocked_;
		leftMouseDown_ = false;
		rightMouseDown_ = false;
		toolManager_.endAction(ToolMode::Build);
		toolManager_.endAction(ToolMode::Delete);
		toolView_.setContinuousUseActive(false);

		mouseLocked_ = false;
		firstMouse_ = true;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
		}

		int windowWidth = 0, windowHeight = 0;
		glfwGetWindowSize(window, &windowWidth, &windowHeight);
		const ImVec2 center(static_cast<float>(windowWidth) * 0.5f, static_cast<float>(windowHeight) * 0.5f);
		lastMouseX_ = center.x;
		lastMouseY_ = center.y;
		glfwSetCursorPos(window, center.x, center.y);
		materialRadialMenu_.open(center, toolManager_.activeMaterialIndex());
		materialRadialMenu_.updateMouse(center);
	}

	void closeRadialToolMenu(GLFWwindow *window)
	{
		if (!radialToolMenu_.isOpen()) {
			return;
		}

		ToolKind selectedTool = toolManager_.activeToolKind();
		if (radialToolMenu_.close(&selectedTool)) {
			toolManager_.setActiveTool(selectedTool);
		}

		leftMouseDown_ = false;
		rightMouseDown_ = false;
		firstMouse_ = true;
		if (radialMenuRestoreMouseLocked_) {
			mouseLocked_ = true;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			}
		} else {
			mouseLocked_ = false;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
			}
		}
	}

	void closeMaterialRadialMenu(GLFWwindow *window)
	{
		if (!materialRadialMenu_.isOpen()) {
			return;
		}

		int selectedMaterial = toolManager_.activeMaterialIndex();
		if (materialRadialMenu_.close(&selectedMaterial)) {
			toolManager_.setActiveMaterialIndex(selectedMaterial);
		}

		leftMouseDown_ = false;
		rightMouseDown_ = false;
		firstMouse_ = true;
		if (radialMenuRestoreMouseLocked_) {
			mouseLocked_ = true;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			}
		} else {
			mouseLocked_ = false;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
			}
		}
	}

	bool pickLookedAtMaterial()
	{
		ChunkManager::VoxelRaycastHit hit;
		const glm::vec3 eye = camera->GetCameraPos();
		const glm::vec3 forward = glm::normalize(camera->GetForward());
		if (!chunkManager->raycastVoxels(eye, forward, 8.0f, hit)) {
			return false;
		}

		toolManager_.setActiveMaterialIndex(static_cast<int>(chunkManager->voxelMaterial(hit.voxel)));
		return true;
	}

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) override
	{
		(void)scancode;
		(void)mods;
		if (key == GLFW_KEY_TAB) {
			if (action == GLFW_PRESS) {
				openRadialToolMenu(window);
			} else if (action == GLFW_RELEASE) {
				closeRadialToolMenu(window);
			}
			return;
		}
		if (key == GLFW_KEY_1) {
			if (action == GLFW_PRESS) {
				openMaterialRadialMenu(window);
			} else if (action == GLFW_RELEASE) {
				closeMaterialRadialMenu(window);
			}
			return;
		}

		if (anySelectorOpen()) {
			if (action == GLFW_RELEASE) {
				applyCameraKeyState(key, action);
				setMovementKeyState(key, false);
				if (key == GLFW_KEY_Z) {
					wireframe_ = false;
				}
			}
			return;
		}

		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			mouseLocked_ = false;
			firstMouse_ = true;
			leftMouseDown_ = false;
			rightMouseDown_ = false;
			toolManager_.endAction(ToolMode::Build);
			toolManager_.endAction(ToolMode::Delete);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
			}
		}

		if (key == GLFW_KEY_X && action == GLFW_PRESS) {
			if (camera == &fpvCamera) {
				freeCamera.SetState(
					fpvCamera.GetCameraPos(),
					fpvCamera.GetYaw(),
					fpvCamera.GetPitch(),
					fpvCamera.GetFOV()
				);

				camera = &freeCamera;
			} else {
				camera = &fpvCamera;
			}
		}
		if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
			showDebugWindow_ = !showDebugWindow_;
		}

		if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
			pickLookedAtMaterial();
		}

		applyCameraKeyState(key, action);

		// if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		// 	player_.tryJump();

		if (action == GLFW_PRESS)
			setMovementKeyState(key, true);
		else if (action == GLFW_RELEASE)
			setMovementKeyState(key, false);

		if (key == GLFW_KEY_Z && action == GLFW_PRESS)
			wireframe_ = true;
		if (key == GLFW_KEY_Z && action == GLFW_RELEASE)
			wireframe_ = false;

		if (key == GLFW_KEY_T && action == GLFW_PRESS) {
			idleYawHoldEnabled_ = !idleYawHoldEnabled_;
			cout << "[Camera yaw on idle] " << (idleYawHoldEnabled_ ? "hold after stillness (T)" : "always face camera (T)") << endl;
		}

		if (key == GLFW_KEY_B && action == GLFW_PRESS) {
			postToggles_.bloomEnabled = !postToggles_.bloomEnabled;
			cout << "[PostFX] Bloom: " << (postToggles_.bloomEnabled ? "ON" : "OFF") << endl;
		}

		if (key == GLFW_KEY_MINUS && action == GLFW_PRESS) {
			postToggles_.taaEnabled = !postToggles_.taaEnabled;
			taaHistoryValid_ = false;
			cout << "[PostFX] TAA: " << (postToggles_.taaEnabled ? "ON" : "OFF") << endl;
		}

		if (action == GLFW_PRESS || action == GLFW_REPEAT) {
			if (key == GLFW_KEY_LEFT_BRACKET) {
				toolManager_.cycleSize(-1);
				markSizeChanged();
			} else if (key == GLFW_KEY_RIGHT_BRACKET) {
				toolManager_.cycleSize(1);
				markSizeChanged();
			}
		}
	}

	void mouseCallback(GLFWwindow *window, int button, int action, int mods) override
	{
		(void)window;
		(void)mods;

		if (anySelectorOpen()) {
			if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
				leftMouseDown_ = false;
			} else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
				rightMouseDown_ = false;
			}
			return;
		}

		if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
			if (!mouseLocked_) {
				mouseLocked_ = true;
				firstMouse_ = true;
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				if (glfwRawMouseMotionSupported()) {
					glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
				}
				return;
			}

			pickLookedAtMaterial();
			return;
		}

		if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_PRESS) {
			if (!mouseLocked_) {
				mouseLocked_ = true;
				firstMouse_ = true;
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				if (glfwRawMouseMotionSupported()) {
					glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
				}
				return;
			}

			const ToolMode mode = (button == GLFW_MOUSE_BUTTON_LEFT) ? ToolMode::Build : ToolMode::Delete;
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				leftMouseDown_ = true;
			} else {
				rightMouseDown_ = true;
			}
			glm::vec3 eye = camera->GetCameraPos();
			glm::vec3 forward = glm::normalize(camera->GetForward());
			ChunkEditSummary editSummary;
			if (toolManager_.beginAction(*chunkManager, eye, forward, mode, &editSummary)) {
				if (!toolManager_.supportsContinuousAction(mode)) {
					toolView_.triggerUse();
				}
				spawnBreakParticles(editSummary);
				if (mode == ToolMode::Build)  playPlaceSfx();
				else                          playBreakSfx();
			}
		}
		if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_RELEASE) {
			const ToolMode mode = (button == GLFW_MOUSE_BUTTON_LEFT) ? ToolMode::Build : ToolMode::Delete;
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				leftMouseDown_ = false;
			} else {
				rightMouseDown_ = false;
			}
			toolManager_.endAction(mode);
		}
	}

	void resizeCallback(GLFWwindow *window, int width, int height) override
	{
		(void)window;
		glViewport(0, 0, width, height);
		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(windowManager->getHandle(), &fbW, &fbH);
		setupPostProcess(fbW, fbH);
	}

	void cursorPosCallback(GLFWwindow *window, double xpos, double ypos) override
	{
		(void)window;
		if (radialToolMenu_.isOpen()) {
			lastMouseX_ = xpos;
			lastMouseY_ = ypos;
			radialToolMenu_.updateMouse(ImVec2(static_cast<float>(xpos), static_cast<float>(ypos)));
			return;
		}
		if (materialRadialMenu_.isOpen()) {
			lastMouseX_ = xpos;
			lastMouseY_ = ypos;
			materialRadialMenu_.updateMouse(ImVec2(static_cast<float>(xpos), static_cast<float>(ypos)));
			return;
		}
		if (!mouseLocked_){
			return;
		}
		if (firstMouse_) {
			lastMouseX_ = xpos;
			lastMouseY_ = ypos;
			firstMouse_ = false;
			return;
		}
		double dx = xpos - lastMouseX_;
		double dy = lastMouseY_ - ypos;
		lastMouseX_ = xpos;
		lastMouseY_ = ypos;
		camera->ProcessMouseMovement(dx, dy);
	}

	void scrollCallback(GLFWwindow *window, double xoffset, double yoffset) override
	{
		(void)window;
		(void)xoffset;
		if (anySelectorOpen()) {
			return;
		}
		camera->ProcessScroll(yoffset);
	}

	void updateFixedStep(float dt)
	{
		const bool radialMenuOpen = anySelectorOpen();
		vec2 wish = radialMenuOpen
			? vec2(0.0f)
			: vec2(
				static_cast<float>(keyD_) - static_cast<float>(keyA_),
				static_cast<float>(keyW_) - static_cast<float>(keyS_));
		if (!radialMenuOpen) {
			camera->UpdateCamera(dt);
		}
		animTime_ += dt;
		const float sunTurnInput = static_cast<float>(keySunRight_) - static_cast<float>(keySunLeft_);
		if (!radialMenuOpen && std::abs(sunTurnInput) > 0.0f) {
			rotateSunAroundWorldY(sunTurnInput * sunRotateSpeedRadPerSec_ * dt);
			chunkManager->markShadowMapsDirty();
		}
		const bool organicInUse =
			!radialMenuOpen &&
			toolManager_.supportsContinuousAction(ToolMode::Build) &&
			(mouseLocked_ && (leftMouseDown_ || rightMouseDown_));
		toolView_.setContinuousUseActive(organicInUse);

		if (!radialMenuOpen && mouseLocked_ && leftMouseDown_ && toolManager_.supportsContinuousAction(ToolMode::Build)) {
			const glm::vec3 eye = camera->GetCameraPos();
			const glm::vec3 forward = glm::normalize(camera->GetForward());
			ChunkEditSummary editSummary;
			if (toolManager_.updateAction(*chunkManager, eye, forward, ToolMode::Build, &editSummary)) {
				playPlaceSfx();
			}
		}
		if (!radialMenuOpen && mouseLocked_ && rightMouseDown_ && toolManager_.supportsContinuousAction(ToolMode::Delete)) {
			const glm::vec3 eye = camera->GetCameraPos();
			const glm::vec3 forward = glm::normalize(camera->GetForward());
			ChunkEditSummary editSummary;
			if (toolManager_.updateAction(*chunkManager, eye, forward, ToolMode::Delete, &editSummary)) {
				spawnBreakParticles(editSummary);
				playBreakSfx();
			}
		}
		
		birdFlock_.update(dt, fpvCamera.GetCameraPos());
		toolView_.update(dt);
		breakParticles_.update(dt);
		spawnLandingParticles();

		toolView_.setAnimTime(animTime_);
		toolView_.setMoveBlend(glm::clamp(glm::length(wish), 0.0f, 1.0f));
	}

	void renderShadowPass()
	{
		if (!shadowSettings_.enabled || !shadowMap_.isReady() || !shadowDepthProg_) {
			return;
		}

		shadowDepthProg_->bind();
		shadowMap_.renderDepthPass([&](int cascadeIndex) {
			glUniformMatrix4fv(shadowDepthProg_->getUniform("lightSpaceMatrix"),
			                   1,
			                   GL_FALSE,
			                   value_ptr(shadowMap_.lightSpaceMatrix(cascadeIndex)));
			Frustum lightFrustum(shadowMap_.lightProjection(cascadeIndex),
			                     shadowMap_.lightView(cascadeIndex));
			chunkManager->drawChunksForShadow(*shadowDepthProg_,
			                                  shadowMap_.cascadeCenter(cascadeIndex),
			                                  lightFrustum,
			                                  shadowMap_.cascadeRadius(cascadeIndex) + chunkManager->chunkSizeMeters);
		});
		shadowDepthProg_->unbind();
	}

	void bindChunkShadowUniforms()
	{
		const bool shadowsActive = shadowSettings_.enabled && shadowMap_.isReady();
		const int activeCascadeCount = shadowsActive ? shadowMap_.cascadeCount() : 0;

		glUniform1i(chunkProg_->getUniform("shadowEnabled"), shadowsActive ? 1 : 0);
		glUniform1i(chunkProg_->getUniform("cascadeCount"), activeCascadeCount);
		glUniform1f(chunkProg_->getUniform("shadowMapSize"), static_cast<float>(std::max(1, shadowMap_.resolution())));
		glUniform1f(chunkProg_->getUniform("shadowStrength"), shadowSettings_.shadowStrength);
		glUniform1f(chunkProg_->getUniform("minShadowVisibility"), shadowSettings_.minShadowVisibility);
		glUniform1f(chunkProg_->getUniform("shadowBlurTexels"), shadowSettings_.blurTexels);
		glUniform1f(chunkProg_->getUniform("normalBiasVoxels"), shadowSettings_.normalBiasVoxels);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D_ARRAY, shadowsActive ? shadowMap_.depthTexture() : 0);
		glUniform1i(chunkProg_->getUniform("shadowMap"), 4);

		if (shadowsActive && activeCascadeCount > 0) {
			glUniformMatrix4fv(chunkProg_->getUniform("lightSpaceMatrices[0]"),
			                   activeCascadeCount,
			                   GL_FALSE,
			                   value_ptr(shadowMap_.lightSpaceMatrix(0)));
			glUniform1fv(chunkProg_->getUniform("cascadeEnds[0]"),
			             activeCascadeCount,
			             shadowMap_.cascadeEnds().data());
		}
	}

	void drawScene3D(const mat4 &P, const mat4 &V, const glm::vec3 &sunDir)
	{
		vec3 eye = camera->GetCameraPos();
		mat4 Vsky = glm::mat4(glm::mat3(V));
		// Frustum frustum = Frustum(P, V);
		Frustum frustum = Frustum(P, fpvCamera.GetViewMatrix()); 

		// vec3 lightColor(1.4f, 1.35f, 1.25f);
		vec3 lightColor(3.0);

		// --- Chunk Drawing ---
		chunkProg_->bind();
		mat4 M = mat4(1.0);
		glUniformMatrix4fv(chunkProg_->getUniform("P"), 1, GL_FALSE, value_ptr(P));
		glUniformMatrix4fv(chunkProg_->getUniform("V"), 1, GL_FALSE, value_ptr(V));
		glUniformMatrix4fv(chunkProg_->getUniform("M"), 1, GL_FALSE, value_ptr(M));
		glUniform3fv(chunkProg_->getUniform("lightPos"), 1, value_ptr(sunWorld_));
		glUniform3fv(chunkProg_->getUniform("lightDir"), 1, value_ptr(sunDir));
		glUniform3fv(chunkProg_->getUniform("camPos"), 1, value_ptr(eye));
		glUniform3fv(chunkProg_->getUniform("lightColor"), 1, value_ptr(lightColor));
		bindChunkShadowUniforms();
		chunkManager->drawChunks(*chunkProg_, fpvCamera, frustum, frameNumber++);
		chunkProg_->unbind();

		birdFlock_.draw(birdProg_, P, V, sunWorld_, lightColor, eye);

		ToolPreview preview = toolManager_.getPreview(*chunkManager, eye, glm::normalize(camera->GetForward()), ToolMode::Build);
		previewRenderer_.draw(preview, chunkManager->voxSizeMeters, P, V);
		breakParticles_.draw(P, V, sunWorld_ - eye, chunkManager->voxSizeMeters);

		skybox_.draw(P, Vsky);
	}

	void chunkrender(double deltaTime) {
		chunkManager->updateChunks();
	}

	void drawHudVoxelCube(ImDrawList *drawList, const glm::vec3 &baseColor, const ImVec2 &center, float size, float alpha) const
	{
		const float w = size * 0.48f;
		const float h = size * 0.28f;
		const float drop = size * 0.42f;
		const float yOffset = size * 0.12f;

		const ImVec2 top(center.x, center.y - h - yOffset);
		const ImVec2 right(center.x + w, center.y - yOffset);
		const ImVec2 bottom(center.x, center.y + h - yOffset);
		const ImVec2 left(center.x - w, center.y - yOffset);
		const ImVec2 bottomDrop(center.x, center.y + h + drop - yOffset);
		const ImVec2 leftDrop(center.x - w, center.y + drop - yOffset);
		const ImVec2 rightDrop(center.x + w, center.y + drop - yOffset);

		const ImVec2 topFace[] = {top, right, bottom, left};
		const ImVec2 leftFace[] = {left, bottom, bottomDrop, leftDrop};
		const ImVec2 rightFace[] = {bottom, right, rightDrop, bottomDrop};

		drawList->AddConvexPolyFilled(leftFace, 4, uiColorFromVec3(shadeHudColor(baseColor, 0.68f), alpha));
		drawList->AddConvexPolyFilled(rightFace, 4, uiColorFromVec3(shadeHudColor(baseColor, 0.86f, 0.010f), alpha));
		drawList->AddConvexPolyFilled(topFace, 4, uiColorFromVec3(shadeHudColor(baseColor, 1.10f, 0.045f), alpha));
	}

	std::string activeSizeText() const
	{
		char buffer[32];
		if (toolManager_.activeToolUsesMeterRadius()) {
			snprintf(buffer, sizeof(buffer), "%.2f m", toolManager_.activeToolRadiusMeters());
		} else {
			snprintf(buffer, sizeof(buffer), "%d vox", toolManager_.activeToolSize());
		}
		return std::string(buffer);
	}

	int activeDiscreteSizeIndex() const
	{
		const int sizes[] = {2, 4, 8, 16, 32, 64};
		const int activeSize = toolManager_.activeToolSize();
		for (int i = 0; i < 6; ++i) {
			if (sizes[i] == activeSize) {
				return i;
			}
		}
		return 0;
	}

	void markSizeChanged()
	{
		sizePulseStartSeconds_ = glfwGetTime();
	}

	void drawGameplayHud(int width, int height)
	{
		(void)height;
		ImDrawList *drawList = ImGui::GetForegroundDrawList();
		ImFont *font = hudFont_ ? hudFont_ : ImGui::GetFont();
		const float panelWidth = std::min(640.0f, std::max(430.0f, static_cast<float>(width) - 36.0f));
		const float panelHeight = 64.0f;
		const ImVec2 panelMin(18.0f, 18.0f);
		const ImVec2 panelMax(panelMin.x + panelWidth, panelMin.y + panelHeight);

		drawList->AddRectFilled(panelMin, panelMax, uiRgba(10, 14, 20, 0.48f), 6.0f);

		const float col0 = panelMin.x + 18.0f;
		const float col1 = panelMin.x + panelWidth * 0.31f;
		const float col2 = panelMin.x + panelWidth * 0.66f;
		const float labelY = panelMin.y + 11.0f;
		const float valueY = panelMin.y + 30.0f;
		const ImU32 labelColor = uiRgba(202, 212, 224, 0.72f);
		const ImU32 valueColor = uiRgba(244, 249, 249, 0.94f);

		drawList->AddLine(ImVec2(col1 - 14.0f, panelMin.y + 12.0f), ImVec2(col1 - 14.0f, panelMax.y - 12.0f), uiRgba(255, 255, 255, 0.10f), 1.0f);
		drawList->AddLine(ImVec2(col2 - 14.0f, panelMin.y + 12.0f), ImVec2(col2 - 14.0f, panelMax.y - 12.0f), uiRgba(255, 255, 255, 0.10f), 1.0f);

		drawList->AddText(font, 15.0f, ImVec2(col0, labelY), labelColor, "Tool");
		drawList->AddText(font, 24.0f, ImVec2(col0, valueY), valueColor, toolManager_.activeToolName());

		drawList->AddText(font, 15.0f, ImVec2(col1, labelY), labelColor, "Material");
		const glm::vec3 materialColor = Materials::paletteColor(toolManager_.activeMaterialIndex());
		drawHudVoxelCube(drawList, materialColor, ImVec2(col1 + 18.0f, valueY + 16.0f), 28.0f, 0.94f);
		drawList->AddText(font, 22.0f, ImVec2(col1 + 44.0f, valueY + 2.0f), valueColor, toolManager_.activeMaterialName());

		const double sizePulseAge = glfwGetTime() - sizePulseStartSeconds_;
		const float sizePulse = (sizePulseAge >= 0.0 && sizePulseAge < 0.75)
			? static_cast<float>(1.0 - sizePulseAge / 0.75)
			: 0.0f;
		if (sizePulse > 0.0f) {
			drawList->AddRectFilled(ImVec2(col2 - 4.0f, panelMin.y + 8.0f), ImVec2(panelMax.x - 12.0f, panelMax.y - 8.0f), uiRgba(255, 255, 255, 0.12f * sizePulse), 5.0f);
		}

		drawList->AddText(font, 15.0f, ImVec2(col2, labelY), labelColor, "Size");
		const std::string sizeText = activeSizeText();
		drawList->AddText(font, 24.0f, ImVec2(col2, valueY), valueColor, sizeText.c_str());

		const float sizeTextWidth = font->CalcTextSizeA(24.0f, FLT_MAX, 0.0f, sizeText.c_str()).x;
		const float meterRight = panelMax.x - 20.0f;
		const float meterX = std::min(meterRight - 92.0f, col2 + sizeTextWidth + 28.0f);
		const float meterWidth = std::max(56.0f, meterRight - meterX);
		const float meterY = valueY + 14.0f;
		if (toolManager_.activeToolUsesMeterRadius()) {
			const float t = glm::clamp((toolManager_.activeToolRadiusMeters() - 0.25f) / (8.0f - 0.25f), 0.0f, 1.0f);
			drawList->AddRectFilled(ImVec2(meterX, meterY), ImVec2(meterX + meterWidth, meterY + 7.0f), uiRgba(255, 255, 255, 0.16f), 3.0f);
			drawList->AddRectFilled(ImVec2(meterX, meterY), ImVec2(meterX + meterWidth * t, meterY + 7.0f), uiRgba(244, 249, 249, 0.78f), 3.0f);
		} else {
			const int activeIndex = activeDiscreteSizeIndex();
			for (int i = 0; i < 6; ++i) {
				const float step = (meterWidth - 8.0f) / 5.0f;
				const float x = meterX + static_cast<float>(i) * step;
				const float alpha = (i <= activeIndex) ? 0.82f : 0.20f;
				drawList->AddRectFilled(ImVec2(x, meterY - 1.0f), ImVec2(x + 8.0f, meterY + 7.0f), uiRgba(244, 249, 249, alpha), 2.0f);
			}
		}
	}

	void render()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		if (width <= 0 || height <= 0 || sceneFBO_ == 0)
			return;

		float aspect = width / (float)height;
		MatrixStack Pstack;
		Pstack.pushMatrix();
		Pstack.perspective(glm::radians(camera->GetFOV()), aspect, 0.1f, 2000.0f);
		mat4 P = Pstack.topMatrix();
		mat4 V = camera->GetViewMatrix();
		Pstack.popMatrix();
		const vec3 sunDir = currentSunDirection();

		// Sub-pixel jitter for TAA — applied to the scene-render projection only.
		// Shadows, sun projection, and SSAO keep the unjittered P.
		mat4 Pjit = P;
		if (postToggles_.taaEnabled && postW_ > 0 && postH_ > 0) {
			glm::vec2 jitter = (haltonJitter(taaFrame_ % 8) - glm::vec2(0.5f)) * 2.0f
			                   / glm::vec2((float)postW_, (float)postH_);
			Pjit[2][0] += jitter.x;
			Pjit[2][1] += jitter.y;
		}

		if (shadowSettings_.enabled && shadowMap_.isReady()) {
			if (chunkManager->isShadowMapsDirty() || chunkManager->hasPendingBufferUpdates()) {
				shadowMap_.updateMatrices(P,
				                          V,
				                          camera->GetCameraPos(),
				                          sunDir,
				                          shadowSettings_.shadowDistance,
				                          chunkManager->voxSizeMeters);
				renderShadowPass();
				if (!chunkManager->hasPendingBufferUpdates()) {
					chunkManager->clearShadowMapsDirty();
				}
			}
		}

		// --- Scene HDR framebuffer (brighter clear helps god-ray mask) ---
		glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO_);
		glViewport(0, 0, postW_, postH_);
		glClearColor(0.24f, 0.3f, 0.42f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (wireframe_)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		drawScene3D(Pjit, V, sunDir);
		if (wireframe_)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// --- TAA resolve: reproject + blend history, output feeds the rest of post ---
		GLuint sceneInputTex = sceneColorTex_;
		if (postToggles_.taaEnabled && taaFBO_ != 0) {
			int writeIdx = 1 - taaHistoryIdx_;
			glBindFramebuffer(GL_FRAMEBUFFER, taaFBO_);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, taaHistoryTex_[writeIdx], 0);
			glViewport(0, 0, postW_, postH_);
			glClear(GL_COLOR_BUFFER_BIT);
			taaProg_->bind();
			mat4 invVP = glm::inverse(Pjit * V);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sceneColorTex_);
			glUniform1i(taaProg_->getUniform("currentTex"), 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, taaHistoryTex_[taaHistoryIdx_]);
			glUniform1i(taaProg_->getUniform("historyTex"), 1);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, sceneDepthTex_);
			glUniform1i(taaProg_->getUniform("depthTex"), 2);
			glUniformMatrix4fv(taaProg_->getUniform("invViewProj"), 1, GL_FALSE, value_ptr(invVP));
			glUniformMatrix4fv(taaProg_->getUniform("prevViewProj"), 1, GL_FALSE, value_ptr(prevViewProj_));
			glUniform2f(taaProg_->getUniform("texelSize"), 1.0f / (float)postW_, 1.0f / (float)postH_);
			glUniform1i(taaProg_->getUniform("historyValid"), (taaHistoryValid_ && sceneDepthTex_ != 0) ? 1 : 0);
			glUniform1f(taaProg_->getUniform("blendFactor"), postToggles_.taaBlend);
			drawFullscreenQuad();
			taaProg_->unbind();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			sceneInputTex = taaHistoryTex_[writeIdx];
			taaHistoryIdx_ = writeIdx;
			taaHistoryValid_ = true;
		}

		const SunProjection sunProj = projectSunToScreen(P, V, sunWorld_);
		const vec2  sunScreen      = sunProj.sunScreen;
		const bool  sunVisible     = sunProj.sunVisible;
		const float sunVisibleFade = sunProj.sunVisibleFade;

		// --- SSAO pass (conditional) ---
		if (postToggles_.ssaoEnabled) {
			renderSSAOPass(P, V);
		}

		// --- God rays (conditional) ---
		if (postToggles_.godRaysEnabled && sunVisible) {
			// Source the radial blur from the real scene HDR buffer (matches CSC471 ref).
			glBindFramebuffer(GL_FRAMEBUFFER, godrayFBO_);
			glViewport(0, 0, postW_, postH_);
			glClear(GL_COLOR_BUFFER_BIT);
			godrayProg_->bind();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sceneInputTex);
			glUniform1i(godrayProg_->getUniform("sceneTex"), 0);
			glUniform2f(godrayProg_->getUniform("sunPos"), sunScreen.x, sunScreen.y);
			glUniform1f(godrayProg_->getUniform("time"), (float)glfwGetTime());
			drawFullscreenQuad();
			godrayProg_->unbind();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		// --- Bloom extract + blur ping-pong (conditional) ---
		int hw = std::max(1, postW_ / 2);
		int hh = std::max(1, postH_ / 2);
		int lastBuf = 0;
		if (postToggles_.bloomEnabled) {
			glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO_[0]);
			glViewport(0, 0, hw, hh);
			glClear(GL_COLOR_BUFFER_BIT);
			bloomBrightProg_->bind();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sceneInputTex);
			glUniform1i(bloomBrightProg_->getUniform("sceneTex"), 0);
			drawFullscreenQuad();
			bloomBrightProg_->unbind();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// --- Blur ping-pong (source: bloom in pingpongTex_[0]) ---
			blurProg_->bind();
			glUniform2f(blurProg_->getUniform("texelSize"), 1.0f / (float)hw, 1.0f / (float)hh);
			bool horizontal = true;
			for (int i = 0; i < 10; i++) {
				int target = 1 - lastBuf;
				glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO_[target]);
				glViewport(0, 0, hw, hh);
				glUniform1i(blurProg_->getUniform("horizontal"), horizontal ? 1 : 0);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pingpongTex_[lastBuf]);
				glUniform1i(blurProg_->getUniform("image"), 0);
				drawFullscreenQuad();
				lastBuf = target;
				horizontal = !horizontal;
			}
			blurProg_->unbind();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		// --- Composite to default framebuffer ---
		float godrayStrength = (postToggles_.godRaysEnabled && sunVisible)
		                       ? postToggles_.godrayStrength * sunVisibleFade
		                       : 0.0f;
		float bloomStrength  = postToggles_.bloomEnabled   ? postToggles_.bloomStrength  : 0.0f;
		glViewport(0, 0, width, height);
		glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		compositeProg_->bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sceneInputTex);
		glUniform1i(compositeProg_->getUniform("sceneTex"), 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, godrayTex_);
		glUniform1i(compositeProg_->getUniform("godrayTex"), 1);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, pingpongTex_[lastBuf]);
		glUniform1i(compositeProg_->getUniform("bloomTex"), 2);
		glUniform1f(compositeProg_->getUniform("godrayStrength"), godrayStrength);
		glUniform1f(compositeProg_->getUniform("bloomStrength"), bloomStrength);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, ssaoBlurTex_);
		glUniform1i(compositeProg_->getUniform("ssaoTex"), 3);
		glUniform1f(compositeProg_->getUniform("ssaoIntensity"), postToggles_.ssaoIntensity);
		glUniform1i(compositeProg_->getUniform("ssaoEnabled"), postToggles_.ssaoEnabled ? 1 : 0);
		drawFullscreenQuad();
		compositeProg_->unbind();

		// tool
		glm::vec3 eye = camera->GetCameraPos();
		glm::vec3 lightColor(1.0f, 0.98f, 0.92f);

		toolView_.draw(width, height,
					V,
					eye,
					camera->GetForward(),
					camera->GetRight(),
					camera->GetUp(),
					sunWorld_,
					lightColor);
		
		// crosshair
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		crosshair_.draw(width, height);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		// ui
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		drawGameplayHud(width, height);

		if (showDebugWindow_) {
			ImGui::Begin("Debug");

			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			ImGui::Text("Camera Pos: %.2f %.2f %.2f",
						camera->GetCameraPos().x,
						camera->GetCameraPos().y,
						camera->GetCameraPos().z);
			ImGui::Text("Sun Pos: %.2f %.2f %.2f", sunWorld_.x, sunWorld_.y, sunWorld_.z);
			ImGui::Text("Tool: %s", toolManager_.activeToolName());
			ImGui::Text("Material: %s", toolManager_.activeMaterialName());
			ImGui::Text("Birds: %zu", birdFlock_.birdCount());
			if (toolManager_.activeToolUsesMeterRadius()) {
				ImGui::Text("Tool Size: %.2f m", toolManager_.activeToolRadiusMeters());
			} else {
				ImGui::Text("Tool Size: %d", toolManager_.activeToolSize());
			}

			ImGui::Checkbox("God Rays", &postToggles_.godRaysEnabled);
			ImGui::Checkbox("Bloom", &postToggles_.bloomEnabled);
			ImGui::Checkbox("SSAO", &postToggles_.ssaoEnabled);
			{
				bool taaEnabled = postToggles_.taaEnabled;
				if (ImGui::Checkbox("TAA (-)", &taaEnabled)) {
					postToggles_.taaEnabled = taaEnabled;
					taaHistoryValid_ = false;
				}
			}
			ImGui::SliderFloat("TAA Blend", &postToggles_.taaBlend, 0.5f, 0.97f, "%.2f");
			{
				bool shadowsEnabled = shadowSettings_.enabled;
				if (ImGui::Checkbox("Shadows", &shadowsEnabled)) {
					if (shadowsEnabled) {
						chunkManager->markShadowMapsDirty();
					}
					shadowSettings_.enabled = shadowsEnabled;
				}
			}
			ImGui::SliderFloat("Shadow Strength", &shadowSettings_.shadowStrength, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Shadow Softness", &shadowSettings_.blurTexels, 0.25f, 3.0f, "%.2f");
			ImGui::SliderFloat("Shadow Min Light", &shadowSettings_.minShadowVisibility, 0.2f, 1.0f, "%.2f");

			if (ImGui::SliderFloat("Music", &musicVolume_, 0.0f, 1.0f, "%.2f")) {
				soundtrack_.setVolume(musicVolume_);
			}

			ImGui::End();
		}
		radialToolMenu_.draw();
		materialRadialMenu_.draw();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Store unjittered view-projection for next frame's TAA reprojection.
		prevViewProj_ = P * V;
		taaFrame_++;
	}

private:
	string resourceDir_;

	float cameraPivotHeight_ = 1.15f;
	float charYaw_ = 0.0f;
	float animTime_ = 0.0f;
	float moveBlendDisplay_ = 0.0f;
	float characterScale_ = 1.0f;
	ImFont *hudFont_ = nullptr;
	bool showDebugWindow_ = false;
	double sizePulseStartSeconds_ = -100.0;

	FirstPersonCamera fpvCamera;
	FreeCamera freeCamera;
	Camera* camera = &fpvCamera;
	ToolView toolView_;
	ToolManager toolManager_;
	ToolPreviewRenderer previewRenderer_;
	RadialToolMenu radialToolMenu_;
	MaterialRadialMenu materialRadialMenu_;
	Crosshair crosshair_;
	Skybox skybox_;
	BreakParticleSystem breakParticles_;
	// GltfMesh characterMesh_;

	shared_ptr<Program> texProg_;
	shared_ptr<Program> godrayProg_, bloomBrightProg_, blurProg_, compositeProg_;
	shared_ptr<Program> chunkProg_;
	shared_ptr<Program> birdProg_;
	shared_ptr<Program> shadowDepthProg_;
	shared_ptr<Program> ssaoProg_;
	shared_ptr<Program> ssaoBlurProg_;
	shared_ptr<Program> taaProg_;
	shared_ptr<Materials> materials = make_shared<Materials>();
	shared_ptr<ChunkManager> chunkManager = make_shared<ChunkManager>(
	16,// voxPerMeter 
	8,// chunkSizeMeters
	48,// renderDistance (in meters)
	16,// renderHeight (int meters)
	32,// generationDistance (in meters)
	64 // generationHeight (in meters)
	);
	unsigned long frameNumber = 0;

	shared_ptr<Texture> collectibleTex_;
	GLuint groundTexGl_ = 0;

	vec3 sunWorld_;

	GLuint quadVao_ = 0, quadVbo_ = 0;
	GLuint sceneFBO_ = 0, sceneColorTex_ = 0;
	GLuint sceneDepthTex_ = 0;
	GLuint sceneDepthRBO_ = 0;
	GLuint godrayFBO_ = 0, godrayTex_ = 0;
	GLuint pingpongFBO_[2] = {0, 0};
	GLuint pingpongTex_[2] = {0, 0};
	GLuint ssaoFBO_ = 0, ssaoBlurFBO_ = 0;
	GLuint ssaoTex_ = 0, ssaoBlurTex_ = 0;
	GLuint noiseTex_ = 0;
	std::vector<glm::vec3> ssaoKernel_;
	int postW_ = 0, postH_ = 0;

	// TAA (temporal anti-aliasing)
	GLuint taaFBO_ = 0;
	GLuint taaHistoryTex_[2] = {0, 0};
	int taaHistoryIdx_ = 0;
	bool taaHistoryValid_ = false;
	glm::mat4 prevViewProj_ = glm::mat4(1.0f);
	unsigned int taaFrame_ = 0;

	// Halton(2,3) sub-pixel jitter sequence in [0,1).
	static glm::vec2 haltonJitter(unsigned int index) {
		auto halton = [](unsigned int i, unsigned int base) {
			float f = 1.0f, r = 0.0f;
			while (i > 0) {
				f /= (float)base;
				r += f * (float)(i % base);
				i /= base;
			}
			return r;
		};
		return glm::vec2(halton(index + 1, 2), halton(index + 1, 3));
	}

	GameWorld world_;
	BirdFlock birdFlock_;

	bool keyW_ = false, keyS_ = false, keyA_ = false, keyD_ = false;
	bool keySunLeft_ = false, keySunRight_ = false;
	bool wireframe_ = false;
	const float sunRotateSpeedRadPerSec_ = glm::radians(35.0f);

	PostProcessToggle postToggles_;
	ShadowSettings shadowSettings_;
	CascadedShadowMap shadowMap_;

	/** After this many seconds with no move input and low horizontal speed, idle avatar stops turning to face the camera (orbit to see front). Toggle with T. */
	static constexpr float kIdleYawHoldSeconds = 2.0f;
	bool idleYawHoldEnabled_ = true;
	float idleSecondsAccum_ = 0.0f;

	bool mouseLocked_ = false;
	bool firstMouse_ = true;
	bool leftMouseDown_ = false;
	bool rightMouseDown_ = false;
	bool radialMenuRestoreMouseLocked_ = false;
	double lastMouseX_ = 0.0, lastMouseY_ = 0.0;

	double lastStatsPrint_ = 0.0;

	SoundtrackPlayer soundtrack_;
	float musicVolume_ = 0.15f;                 // matches debug slider default
	std::vector<std::string> breakSfxIds_;
	std::vector<std::string> placeSfxIds_;
	std::mt19937 sfxRng_{std::random_device{}()};
};

int main(int argc, char *argv[])
{
	string resourceDir = "../resources";
	if (argc >= 2)
		resourceDir = argv[1];

	auto *application = new Application();
	WindowManager *windowManager = new WindowManager();
	if (!windowManager->init(1280, 720)) {
		cerr << "Window / GL init failed." << endl;
		return -1;
	}
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;
	application->init(resourceDir);

	const double fixedDt = 1.0 / 60.0;
	double accumulator = 0.0;
	double elapsedStats = 0.0;

	double tPrev = glfwGetTime();

	std::cout << "Platform: " << glfwGetPlatform() << std::endl;
	while (!glfwWindowShouldClose(windowManager->getHandle())) {
		double tNow = glfwGetTime();
		glfwPollEvents();
		
		double tDelta = tNow - tPrev;
		double frameTime = tDelta;
		tPrev = tNow;
		accumulator += frameTime;
		elapsedStats += frameTime;

		while (accumulator >= fixedDt) {
			application->updateFixedStep(static_cast<float>(fixedDt));
			accumulator -= fixedDt;
		}
		
		application->chunkrender(tDelta);
		application->render();
		glfwSwapBuffers(windowManager->getHandle());
	}


	windowManager->shutdown();
	delete application;
	delete windowManager;
	return 0;
}
