#pragma once

#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Orca
{
	class Quad 
	{
	public:
		Quad() = default;
		~Quad();

		void Init();
		void Render() const;

	private:
		GLuint m_VAO = 0;
		GLuint m_VBO = 0;
	};
}
