#pragma once
#ifndef HYPER_MOUSE_CODE_HPP
#define HYPER_MOUSE_CODE_HPP

#include <GLFW/glfw3.h>
#include <cstdint>

namespace hyp {
	using MouseCode = uint8_t;

	enum Mouse : MouseCode {
		// From glfw3.h
		BUTTON0 = GLFW_MOUSE_BUTTON_1,
		BUTTON2 = GLFW_MOUSE_BUTTON_2,
		BUTTON3 = GLFW_MOUSE_BUTTON_3,
		BUTTON4 = GLFW_MOUSE_BUTTON_4,
		BUTTON5 = GLFW_MOUSE_BUTTON_5,
		BUTTON6 = GLFW_MOUSE_BUTTON_6,
		BUTTON7 = GLFW_MOUSE_BUTTON_7,

		BUTTON_LAST = GLFW_MOUSE_BUTTON_LAST,
		BUTTON_LEFT = GLFW_MOUSE_BUTTON_LEFT,
		BUTTON_RIGHT = GLFW_MOUSE_BUTTON_RIGHT,
		BUTTON_MIDDLE = GLFW_MOUSE_BUTTON_MIDDLE
	};
}

#endif // !HYPER_MOUSE_CODE_HPP