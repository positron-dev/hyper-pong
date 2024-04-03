#include <renderer/renderer2d.hpp>
#include <renderer/vertex_array.hpp>
#include <renderer/vertex_buffer.hpp>
#include <renderer/shader.hpp>
#include "uniform_buffer.hpp"

const uint32_t maxQuad = 1000;
const uint32_t maxVertices = maxQuad * 6;
const uint32_t maxLight = 32;

namespace hyp {
	namespace utils {
		static void init_quad();
		static void flush_quad();
		static void reset_quad();

		static void init_line();
		static void flush_line();
		static void reset_line();
	}

	struct QuadVertex
	{
		glm::vec3 pos = glm::vec3(0.0);
		glm::vec4 color = glm::vec4(1.0);
		glm::vec2 uv{};
		int index = 0;
	};

	struct LineVertex {
		glm::vec3 position = { 0.f, 0.f, 0.f };
		glm::vec4 color = glm::vec4(1.f);
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

		glm::vec4 vertexPos[6] = {};
		glm::vec2 uvCoords[6] = {};
		int indexCount = 0;
	};

	struct LineData : public RenderEntity {
		std::vector<LineVertex> vertices;
		float lineWidth = 2.f;
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

		// entity
		QuadData quad;
		LineData line;
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
		utils::init_quad();
		utils::init_line();

		HYP_INFO("Initialize 2D Renderer");

		s_renderer.cameraUniformBuffer = hyp::UniformBuffer::create(sizeof(RendererData::CameraData), 0);
		s_renderer.lighting.uniformBuffer = hyp::UniformBuffer::create(sizeof(Light) * maxLight, 2);
		glEnable(GL_LINE_SMOOTH);
	}

	void Renderer2D::deinit()
	{
		s_renderer.quad.vertices.clear();
		s_renderer.line.vertices.clear();
		HYP_INFO("Destroyed 2D Renderer");

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

	void Renderer2D::startBatch()
	{
		utils::reset_quad();
		utils::reset_line();

		s_renderer.stats.drawCalls = 0;
		s_renderer.stats.lineCount = 0;
		s_renderer.stats.quadCount = 0;

		// lightings
		s_renderer.lighting.lights.clear();
		s_renderer.lighting.lightCount = 0;
	}

	void Renderer2D::drawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		auto& quad = s_renderer.quad;
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, position + glm::vec3(size / 2.f, 0.f));
		model = glm::scale(model, glm::vec3(size, 0.f));


		for (int i = 0; i < 6; i++) {
			QuadVertex vertex;
			vertex.pos = quad.vertexPos[i];
			vertex.color = color;
			vertex.uv = quad.uvCoords[i];
			vertex.index = quad.indexCount;

			quad.vertices.push_back(vertex);
		}

		quad.transforms.push_back(model);
		quad.indexCount++;

		if (quad.transforms.size() == maxQuad) {
			utils::flush_quad();
			utils::reset_quad();
		}
	}

	void Renderer2D::drawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color) {
		LineVertex v0, v1;

		v0.position = p1;
		v1.position = p2;

		v0.color = v1.color = color;

		s_renderer.line.vertices.push_back(v0);
		s_renderer.line.vertices.push_back(v1);
	}

	void Renderer2D::flush() {
		utils::flush_quad();
		utils::flush_line();
	}

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
}

/*Quad Data*/

void hyp::utils::init_quad() {
	auto& quad = s_renderer.quad;

	quad.vao = hyp::VertexArray::create();
	quad.vbo = hyp::VertexBuffer::create(maxVertices * sizeof(QuadVertex));
	quad.vbo->setLayout({
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec3, "aPos", false),
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec4, "aColor", false),
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec2, "aUV", false),
			hyp::VertexAttribDescriptor(hyp::ShaderDataType::Int, "aIndex", false)
		});

	quad.vao->addVertexBuffer(s_renderer.quad.vbo);
	quad.vertices.clear();
	quad.vertices.resize(maxVertices);

	quad.program = hyp::CreateRef<hyp::ShaderProgram>("assets/shaders/quad.vert",
		"assets/shaders/quad.frag");

	quad.program->link();
	quad.program->setBlockBinding("Camera", 0);
	quad.program->setBlockBinding("Transform", 1);
	quad.program->setBlockBinding("Lights", 2);

	quad.uniformBuffer = hyp::UniformBuffer::create(sizeof(glm::mat4) * maxQuad, 1);

	quad.vertexPos[0] = { +0.5f, +0.5f, 0.0, 1.f };
	quad.vertexPos[1] = { -0.5f, +0.5f, 0.0, 1.f };
	quad.vertexPos[2] = { -0.5f, -0.5f, 0.0, 1.f };
	quad.vertexPos[3] = { -0.5f, -0.5f, 0.0, 1.f };
	quad.vertexPos[4] = { +0.5f, -0.5f, 0.0, 1.f };
	quad.vertexPos[5] = { +0.5f, +0.5f, 0.0, 1.f };

	quad.uvCoords[0] = { 1, 1 };
	quad.uvCoords[1] = { 0, 1 };
	quad.uvCoords[2] = { 0, 0 };
	quad.uvCoords[3] = { 0, 0 };
	quad.uvCoords[4] = { 1, 0 };
	quad.uvCoords[5] = { 1, 1 };
}

void hyp::utils::flush_quad()
{
	auto& quad = s_renderer.quad;
	size_t size = quad.vertices.size();
	if (size == 0) {
		return;
	}

	quad.vao->bind();
	quad.vbo->setData(quad.vertices.data(), size * sizeof(QuadVertex));

	auto& lighting = s_renderer.lighting;

	quad.program->use();
	quad.uniformBuffer->setData(quad.transforms.data(), quad.transforms.size() * sizeof(glm::mat4));

	quad.program->setBool("enableLighting", lighting.enabled);
	if (lighting.enabled) {
		quad.program->setInt("noLights", lighting.lightCount);
		lighting.uniformBuffer->setData(lighting.lights.data(), lighting.lights.size() * sizeof(Light));
	}
	else {
		quad.program->setInt("noLights", 0);
	}

	glDrawArrays(GL_TRIANGLES, 0, size);

	s_renderer.stats.quadCount += size / 6;
	s_renderer.stats.drawCalls++;
}

void hyp::utils::reset_quad() {
	s_renderer.quad.vertices.clear();
	s_renderer.quad.transforms.clear();
	s_renderer.quad.indexCount = 0;
}


/*Line Data*/
void hyp::utils::init_line() {
	auto& line = s_renderer.line;


	line.vao = hyp::VertexArray::create();
	line.vbo = hyp::VertexBuffer::create(maxVertices * sizeof(LineVertex));

	line.vbo->setLayout({
		hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec3, "aPos", false),
		hyp::VertexAttribDescriptor(hyp::ShaderDataType::Vec4, "aColor", false),
		});

	line.vao->addVertexBuffer(s_renderer.line.vbo);
	line.vertices.clear();
	line.vertices.resize(maxVertices);

	line.program = hyp::CreateRef<hyp::ShaderProgram>(
		"assets/shaders/line.vert", "assets/shaders/line.frag");
	line.program->setBlockBinding("Camera", 0);


	line.program->link();
}

void hyp::utils::flush_line() {
	auto& line = s_renderer.line;
	size_t size = line.vertices.size();
	if (size == 0) {
		return;
	}

	line.vao->bind();
	line.vbo->setData(&line.vertices[0], size * sizeof(LineVertex));
	line.program->use();

	glLineWidth(line.lineWidth);
	glDrawArrays(GL_LINES, 0, size);

	s_renderer.stats.lineCount += size / 2;
	s_renderer.stats.drawCalls++;
}

void hyp::utils::reset_line()
{
	s_renderer.line.vertices.clear();
}

hyp::Renderer2D::Stats hyp::Renderer2D::getStats() {
	return s_renderer.stats;
}
