#include "game_layer.hpp"
#include <chrono>
#include <random>

static const int row = 50;
static const int col = 50;

int board[row][col] = { 0 };

static const float sizeX = WIDTH / row;
static const float sizeY = HEIGHT / col;
static const float space = 0.f;

static float timeToUpdate = 0.f;

static unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
std::default_random_engine generator(seed);

static void init_board() {
	std::uniform_int_distribution<int> distribution(0, 1);
	int i, j;
	for (i = 0; i < col; i++)
	{
		for (j = 0; j < row; j++)
		{
			board[j][i] = distribution(generator);
		}
	}
}

static bool is_cell_alive(int i, int j) {
	HYP_ASSERT(i < row && j < col);

	int live = 0;

	for (int x = -1; x <= 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			// skip the cell itself
			if (x == 0 && y == 0)
			{
				continue;
			}

			int ni = x + i;
			int nj = y + j;

			if (ni >= 0 && ni < row && nj >= 0 && nj < col && board[ni][nj] == 1)
			{
				live++;
			}
		}
	}

	// against the odds

	// Rules:
	// 1. Any live cell with fewer than two live neighbors dies, as if by underpopulation.
	// 2. Any live cell with two or three live neighbors lives on to the next generation.
	// 3. Any live cell with more than three live neighbors dies, as if by overpopulation.
	// 4. Any dead cell with exactly three live neighbors becomes a live cell, as if by reproduction.
	//

	bool is_alive = bool(board[i][j]);

	if (is_alive)
	{
		if (live < 2)
			return false;
		else if (live == 2 || live == 3)
			return true;
		else
			return false;
	}
	else if (live == 3)
	{
		return true;
	}

	return false;
}

static void update_board() {
	for (int j = 0; j < col; j++)
	{
		for (int i = 0; i < row; i++)
		{
			board[i][j] = is_cell_alive(i, j);
		}
	}
}

static void draw_board() {
	for (int j = 0; j < col; j++)
	{
		for (int i = 0; i < row; i++)
		{
			glm::vec3 pos {};
			pos.x = ((sizeX + space) * i);
			pos.y = ((sizeY + space) * j);

			glm::mat4 model(1.0);

			model = glm::translate(model, pos);
			model = glm::scale(model, glm::vec3(sizeX, sizeY, 1.f));

			int val = board[i][j];
			if (val == 1)
			{
				hyp::Renderer2D::drawQuad(pos, { sizeX, sizeY }, glm::vec4(1.f));
			}
			else
			{
				hyp::Renderer2D::drawQuad(pos, { sizeX, sizeY }, glm::vec4(0.f, 0.f, 0.f, 1.f));
			}
		}
	}
}

void GameLayer::onAttach() {
	init_board();

	m_cameraController = hyp::CreateRef<hyp::OrthoGraphicCameraController>(600.f, 600.f);
}

void GameLayer::onUpdate(float dt) {
	timeToUpdate += dt;

	if (timeToUpdate >= 0.5f)
	{
		update_board();
		timeToUpdate = 0.f;
	}

	hyp::RenderCommand::setClearColor(0.3, 0.4, 0.1, 1.f);
	hyp::RenderCommand::clear();

	hyp::Renderer2D::beginScene(m_cameraController->getCamera().getViewProjectionMatrix());
	draw_board();

	hyp::Renderer2D::endScene();
}