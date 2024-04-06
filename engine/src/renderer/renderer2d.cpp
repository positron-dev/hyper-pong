#include <renderer/renderer2d.hpp>
#include <renderer/vertex_array.hpp>
#include <renderer/vertex_buffer.hpp>
#include <renderer/shader.hpp>
#include "uniform_buffer.hpp"
#include "renderer/element_buffer.hpp"
#include "renderer/render_command.hpp"
#include <array>

using namespace hyp;

const uint32_t maxLight = 32;

namespace utils {
	static void initQuad();
	static void flushQuad();
	static void nextQuadBatch();

	static void initLine();
	static void flushLine();
	static void nextLineBatch();

	static void initCircle();
	static void flushCircle();
}

struct QuadVertex
{
	glm::vec3 pos = glm::vec3(0.0);
	glm::vec4 color = glm::vec4(1.0);
	glm::vec2 uv{};
	int transformIndex = 0;
	float textureIndex = 0;
};

struct LineVertex {
	glm::vec3 position = { 0.f, 0.f, 0.f };
	glm::vec4 color = glm::vec4(1.f);
};

struct CircleVertex {
	glm::vec3 worldPosition;
	glm::vec3 localPosition;
	glm::vec4 color;
	float thickness;
	float fade;
};


struct RenderEntity {
	hyp::Ref<hyp::VertexBuffer> vbo;
	hyp::Ref<hyp::VertexArray> vao;
	hyp::Ref<hyp::ShaderProgram> program;
};

struct QuadData : public RenderEntity {
	std::vector<QuadVertex> vertices;
	std::vector<glm::mat4> transforms;
	hyp::Shared<hyp::UniformBuffer> uniformBuffer;

	glm::vec4 vertexPos[4] = {};
	glm::vec2 uvCoords[4] = {};
	int transformIndexCount = 0;

	uint32_t indexCount = 0;

	void reset() {
		vertices.clear();
		transforms.clear();
		indexCount = 0;
		transformIndexCount = 0;
	}
};

struct LineData : public RenderEntity {
	std::vector<LineVertex> vertices;
	virtual void reset() {
		vertices.clear();
	}
};

struct CircleData : public RenderEntity {
	std::vector<CircleVertex> vertices;
	uint32_t indexCount = 0;

	virtual void reset() {
		vertices.clear();
		indexCount = 0;
	}
};

///TODO: support for other lighting settings such as:
/// 1. the constant, linear and quadratic value when calculation attenuation
/// 2. providing ambient, diffuse and specular color for each light
struct LightingData {
	std::vector<hyp::Light> lights;
	hyp::Shared<hyp::UniformBuffer> uniformBuffer;
	int lightCount = 0;
	bool enabled = false;
};

struct RendererData {
	//

	static const uint32_t maxQuad = 1000;
	static const uint32_t maxVertices = maxQuad * 4;
	static const uint32_t maxIndices = maxQuad * 6;

	static const uint32_t maxTextureSlots = 32;

	std::array<hyp::Ref<hyp::Texture2D>, maxTextureSlots> textureSlots;

	hyp::Ref<hyp::Texture2D> defaultTexture;
	uint32_t textureSlotIndex = 1; // 1 instead of 0 because there's already an existing texture -- defaultTexture --

	// entity
	QuadData quad;
	LineData line;
	CircleData circle;
	LightingData lighting;

	struct CameraData {
		glm::mat4 viewProjection;
	};


	CameraData cameraBuffer{};
	hyp::Shared<hyp::UniformBuffer> cameraUniformBuffer;

	Renderer2D::Stats stats;
};

static RendererData s_renderer;

void hyp::Renderer2D::init() {
	HYP_INFO("Initialize 2D Renderer");

	utils::initQuad();
	utils::initLine();
	utils::initCircle();

	s_renderer.defaultTexture = hyp::Texture2D::create(TextureSpecification());

	uint32_t whiteColor = 0xFFffFFff;
	s_renderer.defaultTexture->setData(&whiteColor, sizeof(uint32_t));

	s_renderer.textureSlots[0] = s_renderer.defaultTexture;

	s_renderer.cameraUniformBuffer = hyp::UniformBuffer::create(sizeof(RendererData::CameraData), 0);
	s_renderer.lighting.uniformBuffer = hyp::UniformBuffer::create(sizeof(Light) * maxLight, 2);
}

void Renderer2D::deinit()
{
	s_renderer.quad.vertices.clear();
	s_renderer.line.vertices.clear();
	s_renderer.circle.vertices.clear();
	HYP_INFO("Destroyed 2D Renderer");

}

/*
* flushes all the entity batch (e.g quad, line etc.) data
*/

void Renderer2D::flush() {
	utils::flushQuad();
	utils::flushLine();
	utils::flushCircle();
}


void Renderer2D::startBatch()
{
	s_renderer.quad.reset();
	s_renderer.line.reset();
	s_renderer.circle.reset();

	s_renderer.stats.drawCalls = 0;
	s_renderer.stats.lineCount = 0;
	s_renderer.stats.quadCount = 0;

	// lightings
	s_renderer.lighting.lights.clear();
	s_renderer.lighting.lightCount = 0;

	// textures

	s_renderer.textureSlotIndex = 1;
}

/*
* flushes all the entity batch (e.g quad, line etc.) data, then starts a new one.
*/
void Renderer2D::nextBatch()
{
	flush();
	startBatch();
}

void Renderer2D::beginScene(const glm::mat4& viewProjectionMatrix)
{
	startBatch();

	s_renderer.cameraBuffer.viewProjection = viewProjectionMatrix;
	s_renderer.cameraUniformBuffer->setData(&s_renderer.cameraBuffer, sizeof(RendererData::CameraData));
}

void Renderer2D::endScene() {
	flush();
}

void Renderer2D::enableLighting(bool value)
{
	s_renderer.lighting.enabled = value;
};

void Renderer2D::addLight(const Light& light)
{
	auto& lighting = s_renderer.lighting;

	if (lighting.lightCount >= maxLight) return;

	lighting.lights.push_back(light);
	lighting.lightCount++;
}

void Renderer2D::drawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
{
	auto& quad = s_renderer.quad;

	if (quad.transforms.size() == RendererData::maxQuad) {
		// we've exceeded the maximum batch for a quad at the point,
		// so we have to push it..
		utils::nextQuadBatch();
	}

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, position + glm::vec3(size / 2.f, 0.f));
	model = glm::scale(model, glm::vec3(size, 0.f));

	int quadVertexCount = 4;

	for (int i = 0; i < quadVertexCount; i++) {
		QuadVertex vertex;
		vertex.pos = quad.vertexPos[i];
		vertex.color = color;
		vertex.uv = quad.uvCoords[i];
		vertex.textureIndex = 0.0; // use default texture
		vertex.transformIndex = quad.transformIndexCount;

		quad.vertices.push_back(vertex);
	}

	quad.transforms.push_back(model);
	quad.transformIndexCount++;
	s_renderer.quad.indexCount += 6;
}

/*
* @brief for rendering textured quads
*/
void Renderer2D::drawQuad(const glm::vec3& position, const glm::vec2& size, hyp::Ref<hyp::Texture2D>& texture, const glm::vec4& color)
{
	float textureIndex = 0.0;

	// find texture in slots
	for (int i = 1; i < s_renderer.textureSlotIndex; i++) {
		if (*s_renderer.textureSlots[i] == *texture) {
			textureIndex = (float)i;
			break;
		}
	}

	if (textureIndex == 0.0) {
		// texture is a new texture
		if (s_renderer.textureSlotIndex == RendererData::maxTextureSlots)
			utils::nextQuadBatch(); // dispatch the current batch

		/// the texture slot index will never be = to maxTextureSlots
		/// this logic above avoids this scenario, and is presumed to reset the slot index
		HYP_ASSERT_CORE(s_renderer.textureSlotIndex != RendererData::maxTextureSlots, "texture slot limits exceeded");
		s_renderer.textureSlots[s_renderer.textureSlotIndex] = texture;
		textureIndex = s_renderer.textureSlotIndex++;
	}

	int quadVertexCount = 4;

	auto& quad = s_renderer.quad;

	for (int i = 0; i < quadVertexCount; i++) {
		QuadVertex vertex;
		vertex.pos = quad.vertexPos[i];
		vertex.color = color;
		vertex.uv = quad.uvCoords[i];
		vertex.textureIndex = textureIndex;
		vertex.transformIndex = quad.transformIndexCount;

		quad.vertices.push_back(vertex);
	}

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, position + glm::vec3(size / 2.f, 0.f));
	model = glm::scale(model, glm::vec3(size, 0.f));

	quad.transforms.push_back(model);
	quad.transformIndexCount++;
	s_renderer.quad.indexCount += 6;
}

void Renderer2D::drawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color) {
	LineVertex v0, v1;

	v0.position = p1;
	v1.position = p2;

	v0.color = v1.color = color;

	s_renderer.line.vertices.push_back(v0);
	s_renderer.line.vertices.push_back(v1);

	if (s_renderer.line.vertices.size() == RendererData::maxQuad)
		utils::nextLineBatch();
}

void Renderer2D::drawCircle(const glm::mat4& transform, float thickness, float fade, const glm::vec4& color)
{

	//TODO: call next circle batch if the batch capacity is filled

	for (size_t i = 0; i < 4; i++)
	{
		CircleVertex vertex{};
		vertex.color = color;
		vertex.worldPosition = transform * s_renderer.quad.vertexPos[i]; // TODO: create circle's vertexPos to avoid coupled code
		vertex.localPosition = s_renderer.quad.vertexPos[i] * 2.f;
		vertex.thickness = thickness;
		vertex.fade = fade;

		s_renderer.circle.vertices.push_back(vertex);
	}

	s_renderer.circle.indexCount += 6;
}

/*Quad Data*/

void utils::initQuad() {
	auto& quad = s_renderer.quad;

	quad.vao = hyp::VertexArray::create();
	quad.vbo = hyp::VertexBuffer::create(RendererData::maxVertices * sizeof(QuadVertex));

	quad.vbo->setLayout({
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec3, "aPos", false),
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec4, "aColor", false),
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec2, "aUV", false),
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Int, "aTransformIndex", false),
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Float, "aTextureIndex", false),
		});

	quad.vao->addVertexBuffer(s_renderer.quad.vbo);
	quad.vertices.clear();
	quad.vertices.resize(RendererData::maxVertices);

	uint32_t* quadIndices = new uint32_t[RendererData::maxIndices];

	int offset = 0;
	int i = 0;
	for (i = 0; i < RendererData::maxIndices; i += 6) {
		quadIndices[i + 0] = offset + 0;
		quadIndices[i + 1] = offset + 1;
		quadIndices[i + 2] = offset + 2;

		quadIndices[i + 3] = offset + 2;
		quadIndices[i + 4] = offset + 3;
		quadIndices[i + 5] = offset + 0;

		offset += 4;
	}
	hyp::Ref<hyp::ElementBuffer> elementBuffer = hyp::CreateRef<hyp::ElementBuffer>(quadIndices, RendererData::maxIndices);
	quad.vao->setIndexBuffer(elementBuffer);

	delete[] quadIndices;

	quad.program = hyp::CreateRef<hyp::ShaderProgram>("assets/shaders/quad.vert",
		"assets/shaders/quad.frag");

	quad.program->link();
	quad.program->setBlockBinding("Camera", 0);
	quad.program->setBlockBinding("Transform", 1);
	quad.program->setBlockBinding("Lights", 2);


	// initialize the texture sampler slots
	quad.program->use();
	for (int i = 0; i < RendererData::maxTextureSlots; i++) {
		std::string name = "textures[" + std::to_string(i) + "]";
		quad.program->setInt(name, i);
	}

	quad.uniformBuffer = hyp::UniformBuffer::create(sizeof(glm::mat4) * RendererData::maxQuad, 1);

	quad.vertexPos[0] = { +0.5f, +0.5f, 0.0, 1.f };
	quad.vertexPos[1] = { -0.5f, +0.5f, 0.0, 1.f };
	quad.vertexPos[2] = { -0.5f, -0.5f, 0.0, 1.f };
	quad.vertexPos[3] = { +0.5f, -0.5f, 0.0, 1.f };

	quad.uvCoords[0] = { 1.f, 1.f };
	quad.uvCoords[1] = { 0.f, 1.f };
	quad.uvCoords[2] = { 0.f, 0.f };
	quad.uvCoords[3] = { 1.f, 0.f };
}

void utils::flushQuad()
{
	auto& quad = s_renderer.quad;
	size_t size = quad.vertices.size();
	if (size == 0) {
		return;
	}

	quad.vbo->setData(quad.vertices.data(), size * sizeof(QuadVertex));

	auto& lighting = s_renderer.lighting;
	quad.uniformBuffer->setData(quad.transforms.data(), quad.transforms.size() * sizeof(glm::mat4));

	quad.program->use();
	quad.program->setBool("enableLighting", lighting.enabled);
	if (lighting.enabled) {
		quad.program->setInt("noLights", lighting.lightCount);
		lighting.uniformBuffer->setData(lighting.lights.data(), lighting.lights.size() * sizeof(Light));
	}
	else {
		quad.program->setInt("noLights", 0);
	}

	for (uint32_t i = 0; i < s_renderer.textureSlotIndex; i++)
	{
		s_renderer.textureSlots[i]->bind(i);
	}
	hyp::RenderCommand::drawIndexed(quad.vao, quad.indexCount);

	s_renderer.stats.quadCount += quad.transforms.size(); // each transform count presents a quad (4 vertices)
	s_renderer.stats.drawCalls++;
}

void utils::nextQuadBatch()
{
	utils::flushQuad();
	s_renderer.quad.reset();
	s_renderer.textureSlotIndex = 1;
}

/*Line Data*/
void utils::initLine() {
	auto& line = s_renderer.line;

	line.vao = hyp::VertexArray::create();
	line.vbo = hyp::VertexBuffer::create(RendererData::maxVertices * sizeof(LineVertex));

	line.vbo->setLayout({
		hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec3, "aPos", false),
		hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec4, "aColor", false),
		});

	line.vao->addVertexBuffer(s_renderer.line.vbo);
	line.vertices.clear();
	line.vertices.resize(RendererData::maxVertices);

	line.program = hyp::CreateRef<hyp::ShaderProgram>(
		"assets/shaders/line.vert", "assets/shaders/line.frag");
	line.program->link();
	line.program->setBlockBinding("Camera", 0);
}

void utils::flushLine() {
	auto& line = s_renderer.line;
	size_t size = line.vertices.size();
	if (size == 0) {
		return;
	}

	line.vao->bind();
	line.vbo->setData(&line.vertices[0], size * sizeof(LineVertex));
	line.program->use();

	hyp::RenderCommand::drawLines(line.vao, size);

	s_renderer.stats.lineCount += float(size) / 2.f;
	s_renderer.stats.drawCalls++;
}

void utils::nextLineBatch()
{
	utils::flushLine();
	s_renderer.line.reset();
}

/* Circle Data */
void utils::initCircle()
{
	auto& circle = s_renderer.circle;

	circle.vertices.clear();
	circle.vertices.resize(RendererData::maxVertices);

	circle.vao = hyp::VertexArray::create();

	circle.vbo = hyp::VertexBuffer::create(RendererData::maxVertices * sizeof(CircleVertex));
	auto& layout = hyp::BufferLayout({
		{ hyp::ShaderDataType::Vec3, "aWorldPosition" },
		{ hyp::ShaderDataType::Vec3, "aLocalPosition" },
		{ hyp::ShaderDataType::Vec4, "aColor" },
		{ hyp::ShaderDataType::Float, "aThickness" },
		{ hyp::ShaderDataType::Float, "aFade" },
		});
	circle.vbo->setLayout(layout);


	uint32_t* indices = new uint32_t[RendererData::maxIndices];

	int offset = 0;
	int i = 0;
	for (i = 0; i < RendererData::maxIndices; i += 6) {
		indices[i + 0] = offset + 0;
		indices[i + 1] = offset + 1;
		indices[i + 2] = offset + 2;

		indices[i + 3] = offset + 2;
		indices[i + 4] = offset + 3;
		indices[i + 5] = offset + 0;

		offset += 4;
	}

	auto& circleIndexBuffer = hyp::CreateRef<hyp::ElementBuffer>(indices, RendererData::maxIndices);
	circle.vao->addVertexBuffer(circle.vbo);
	circle.vao->setIndexBuffer(circleIndexBuffer);

	delete[] indices;

	circle.program = hyp::ShaderProgram::create("assets/shaders/circle.vert",
		"assets/shaders/circle.frag");

	circle.program->link();
	circle.program->setBlockBinding("Camera", 0);
}

void utils::flushCircle()
{
	auto& circle = s_renderer.circle;
	size_t size = circle.vertices.size();

	if (!circle.indexCount) {
		return;
	}


	circle.vbo->setData(circle.vertices.data(), (uint32_t)size * sizeof(CircleVertex));

	circle.program->use();
	hyp::RenderCommand::drawIndexed(circle.vao, circle.indexCount);
}

hyp::Renderer2D::Stats hyp::Renderer2D::getStats() {
	return s_renderer.stats;
}
