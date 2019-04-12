#include <GL\glew.h>
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <cstdio>
#include <cassert>

#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <vector>
#include <iostream>

#include "GL_framework.h"
#include "Texture.h"

namespace ResourcesManager {
	extern std::string ReadFile(const char* filePath);
}
float Random(float min, float max) {

	return min + (rand() - 0) * (max - min) / (RAND_MAX - 0);
}
float MapValue(float val, float inMin, float inMax, float outMin, float outMax) {
	return outMin + (val - inMin) * (outMax - outMin) / (inMax - inMin);
}

#define POINTS_COUNT 10

#define SPACE_WIDTH 10
#define SPACE_HEIGHT 10
#define SPACE_DEPTH 10

#define PI 3.1415926535897932384626433832795
#define TAU PI*2

namespace GlobalVars {
	static unsigned int CURRENT_SCENE = 1;
	static float CUBE_WIDTH = 1.2f;
	static float MUTATOR = 0.1f;

}

///////// fw decl
namespace ImGui {
	void Render();
}
namespace Axis {
void setupAxis();
void cleanupAxis();
void drawAxis();
}
////////////////

namespace RenderVars {
	const float FOV = glm::radians(65.f);
	const float zNear = 1.f;
	const float zFar = 50.f;

	glm::mat4 _projection;
	glm::mat4 _modelView;
	glm::mat4 _MVP;
	glm::mat4 _inv_modelview;
	glm::vec4 _cameraPoint;

	struct prevMouse {
		float lastx, lasty;
		MouseEvent::Button button = MouseEvent::Button::None;
		bool waspressed = false;
	} prevMouse;

	float panv[3] = { 0.f, -5.f, -15.f };
	float rota[2] = { 0.f, 0.f };
}
namespace RV = RenderVars;

void GLResize(int width, int height) {
	glViewport(0, 0, width, height);
	if(height != 0) RV::_projection = glm::perspective(RV::FOV, (float)width / (float)height, RV::zNear, RV::zFar);
	else RV::_projection = glm::perspective(RV::FOV, 0.f, RV::zNear, RV::zFar);
}

void GLmousecb(MouseEvent ev) {
	if(RV::prevMouse.waspressed && RV::prevMouse.button == ev.button) {
		float diffx = ev.posx - RV::prevMouse.lastx;
		float diffy = ev.posy - RV::prevMouse.lasty;
		switch(ev.button) {
		case MouseEvent::Button::Left: // ROTATE
			RV::rota[0] += diffx * 0.005f;
			RV::rota[1] += diffy * 0.005f;
			break;
		case MouseEvent::Button::Right: // MOVE XY
			RV::panv[0] += diffx * 0.03f;
			RV::panv[1] -= diffy * 0.03f;
			break;
		case MouseEvent::Button::Middle: // MOVE Z
			RV::panv[2] += diffy * 0.05f;
			break;
		default: break;
		}
	} else {
		RV::prevMouse.button = ev.button;
		RV::prevMouse.waspressed = true;
	}
	RV::prevMouse.lastx = ev.posx;
	RV::prevMouse.lasty = ev.posy;
}

//////////////////////////////////////////////////
GLuint compileShader(const char* shaderStr, GLenum shaderType, const char* name="") {
	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &shaderStr, NULL);
	glCompileShader(shader);
	GLint res;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
	if (res == GL_FALSE) {
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &res);
		char *buff = new char[res];
		glGetShaderInfoLog(shader, res, &res, buff);
		fprintf(stderr, "Error Shader %s: %s", name, buff);
		delete[] buff;
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}
void linkProgram(GLuint program) {
	glLinkProgram(program);
	GLint res;
	glGetProgramiv(program, GL_LINK_STATUS, &res);
	if (res == GL_FALSE) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &res);
		char *buff = new char[res];
		glGetProgramInfoLog(program, res, &res, buff);
		fprintf(stderr, "Error Link: %s", buff);
		delete[] buff;
	}
}

////////////////////////////////////////////////// AXIS
namespace Axis {
GLuint AxisVao;
GLuint AxisVbo[3];
GLuint AxisShader[2];
GLuint AxisProgram;

float AxisVerts[] = {
	0.0, 0.0, 0.0,
	1.0, 0.0, 0.0,
	0.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	0.0, 0.0, 0.0,
	0.0, 0.0, 1.0
};
float AxisColors[] = {
	1.0, 0.0, 0.0, 1.0,
	1.0, 0.0, 0.0, 1.0,
	0.0, 1.0, 0.0, 1.0,
	0.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 1.0,
	0.0, 0.0, 1.0, 1.0
};
GLubyte AxisIdx[] = {
	0, 1,
	2, 3,
	4, 5
};
const char* Axis_vertShader =
"#version 330\n\
in vec3 in_Position;\n\
in vec4 in_Color;\n\
out vec4 vert_color;\n\
uniform mat4 mvpMat;\n\
void main() {\n\
	vert_color = in_Color;\n\
	gl_Position = mvpMat * vec4(in_Position, 1.0);\n\
}";
const char* Axis_fragShader =
"#version 330\n\
in vec4 vert_color;\n\
out vec4 out_Color;\n\
void main() {\n\
	out_Color = vert_color;\n\
}";

void setupAxis() {
	glGenVertexArrays(1, &AxisVao);
	glBindVertexArray(AxisVao);
	glGenBuffers(3, AxisVbo);

	glBindBuffer(GL_ARRAY_BUFFER, AxisVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, AxisVerts, GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, AxisVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, AxisColors, GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)1, 4, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, AxisVbo[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte) * 6, AxisIdx, GL_STATIC_DRAW);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	AxisShader[0] = compileShader(Axis_vertShader, GL_VERTEX_SHADER, "AxisVert");
	AxisShader[1] = compileShader(Axis_fragShader, GL_FRAGMENT_SHADER, "AxisFrag");

	AxisProgram = glCreateProgram();
	glAttachShader(AxisProgram, AxisShader[0]);
	glAttachShader(AxisProgram, AxisShader[1]);
	glBindAttribLocation(AxisProgram, 0, "in_Position");
	glBindAttribLocation(AxisProgram, 1, "in_Color");
	linkProgram(AxisProgram);
}
void cleanupAxis() {
	glDeleteBuffers(3, AxisVbo);
	glDeleteVertexArrays(1, &AxisVao);

	glDeleteProgram(AxisProgram);
	glDeleteShader(AxisShader[0]);
	glDeleteShader(AxisShader[1]);
}
void drawAxis() {
	glBindVertexArray(AxisVao);
	glUseProgram(AxisProgram);
	glUniformMatrix4fv(glGetUniformLocation(AxisProgram, "mvpMat"), 1, GL_FALSE, glm::value_ptr(RV::_MVP));
	glDrawElements(GL_LINES, 6, GL_UNSIGNED_BYTE, 0);

	glUseProgram(0);
	glBindVertexArray(0);
}
}

////////////////////////////////////////////////// CUBE
namespace Cube {
GLuint cubeVao;
GLuint cubeVbo[3];
GLuint cubeShaders[2];
GLuint cubeProgram;
glm::mat4 objMat = glm::mat4(1.f);

extern const float halfW = 0.5f;
int numVerts = 24 + 6; // 4 vertex/face * 6 faces + 6 PRIMITIVE RESTART

					   //   4---------7
					   //  /|        /|
					   // / |       / |
					   //5---------6  |
					   //|  0------|--3
					   //| /       | /
					   //|/        |/
					   //1---------2
glm::vec3 verts[] = {
	glm::vec3(-halfW, -halfW, -halfW),
	glm::vec3(-halfW, -halfW,  halfW),
	glm::vec3(halfW, -halfW,  halfW),
	glm::vec3(halfW, -halfW, -halfW),
	glm::vec3(-halfW,  halfW, -halfW),
	glm::vec3(-halfW,  halfW,  halfW),
	glm::vec3(halfW,  halfW,  halfW),
	glm::vec3(halfW,  halfW, -halfW)
};
glm::vec3 norms[] = {
	glm::vec3(0.f, -1.f,  0.f),
	glm::vec3(0.f,  1.f,  0.f),
	glm::vec3(-1.f,  0.f,  0.f),
	glm::vec3(1.f,  0.f,  0.f),
	glm::vec3(0.f,  0.f, -1.f),
	glm::vec3(0.f,  0.f,  1.f)
};

glm::vec3 cubeVerts[] = {
	verts[1], verts[0], verts[2], verts[3],
	verts[5], verts[6], verts[4], verts[7],
	verts[1], verts[5], verts[0], verts[4],
	verts[2], verts[3], verts[6], verts[7],
	verts[0], verts[4], verts[3], verts[7],
	verts[1], verts[2], verts[5], verts[6]
};
glm::vec3 cubeNorms[] = {
	norms[0], norms[0], norms[0], norms[0],
	norms[1], norms[1], norms[1], norms[1],
	norms[2], norms[2], norms[2], norms[2],
	norms[3], norms[3], norms[3], norms[3],
	norms[4], norms[4], norms[4], norms[4],
	norms[5], norms[5], norms[5], norms[5]
};
GLubyte cubeIdx[] = {
	0, 1, 2, 3, UCHAR_MAX,
	4, 5, 6, 7, UCHAR_MAX,
	8, 9, 10, 11, UCHAR_MAX,
	12, 13, 14, 15, UCHAR_MAX,
	16, 17, 18, 19, UCHAR_MAX,
	20, 21, 22, 23, UCHAR_MAX
};

const char* cube_vertShader =
"#version 330\n\
in vec3 in_Position;\n\
in vec3 in_Normal;\n\
out vec4 vert_Normal;\n\
uniform mat4 objMat;\n\
uniform mat4 mv_Mat;\n\
uniform mat4 mvpMat;\n\
void main() {\n\
	gl_Position = mvpMat * objMat * vec4(in_Position, 1.0);\n\
	vert_Normal = mv_Mat * objMat * vec4(in_Normal, 0.0);\n\
}";
const char* cube_fragShader =
"#version 330\n\
in vec4 vert_Normal;\n\
out vec4 out_Color;\n\
uniform mat4 mv_Mat;\n\
uniform vec4 color;\n\
void main() {\n\
	out_Color = vec4(color.xyz * dot(vert_Normal, mv_Mat*vec4(0.0, 1.0, 0.0, 0.0)) + color.xyz * 0.3, 1.0 );\n\
}";
void setupCube() {
	glGenVertexArrays(1, &cubeVao);
	glBindVertexArray(cubeVao);
	glGenBuffers(3, cubeVbo);

	glBindBuffer(GL_ARRAY_BUFFER, cubeVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, cubeVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeNorms), cubeNorms, GL_STATIC_DRAW);
	glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	glPrimitiveRestartIndex(UCHAR_MAX);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeVbo[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIdx), cubeIdx, GL_STATIC_DRAW);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	cubeShaders[0] = compileShader(cube_vertShader, GL_VERTEX_SHADER, "cubeVert");
	cubeShaders[1] = compileShader(cube_fragShader, GL_FRAGMENT_SHADER, "cubeFrag");

	cubeProgram = glCreateProgram();
	glAttachShader(cubeProgram, cubeShaders[0]);
	glAttachShader(cubeProgram, cubeShaders[1]);
	glBindAttribLocation(cubeProgram, 0, "in_Position");
	glBindAttribLocation(cubeProgram, 1, "in_Normal");
	linkProgram(cubeProgram);
}
void cleanupCube() {
	glDeleteBuffers(3, cubeVbo);
	glDeleteVertexArrays(1, &cubeVao);

	glDeleteProgram(cubeProgram);
	glDeleteShader(cubeShaders[0]);
	glDeleteShader(cubeShaders[1]);
}
void updateCube(const glm::mat4& transform) {
	objMat = transform;
}
void drawCube() {
	glEnable(GL_PRIMITIVE_RESTART);
	glBindVertexArray(cubeVao);
	glUseProgram(cubeProgram);
	glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
	glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
	glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
	glUniform4f(glGetUniformLocation(cubeProgram, "color"), 0.1f, 1.f, 1.f, 0.f);
	glDrawElements(GL_TRIANGLE_STRIP, numVerts, GL_UNSIGNED_BYTE, 0);

	glUseProgram(0);
	glBindVertexArray(0);
	glDisable(GL_PRIMITIVE_RESTART);
}
}

namespace Points {
	GLuint vao, vbo;
	GLuint shaders[3];
	GLuint program;
	static glm::vec3 positions[POINTS_COUNT];
	glm::mat4 objMat(1.f);

	int randomizer;

	void setupPoints() {

		for (int i = 0; i < POINTS_COUNT; i++) {
			positions[i] = glm::vec3(0);
			//Points::positions[i] = { rand() % SPACE_WIDTH - SPACE_WIDTH/2, 
			//						rand() % SPACE_HEIGHT, 
			//						rand() %  SPACE_DEPTH - SPACE_DEPTH/2 };
		}


		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		shaders[0] = compileShader(ResourcesManager::ReadFile(".\\vertexShader").c_str(), GL_VERTEX_SHADER);
		shaders[1] = compileShader(ResourcesManager::ReadFile(".\\geometryShader").c_str(), GL_GEOMETRY_SHADER);
		shaders[2] = compileShader(ResourcesManager::ReadFile(".\\fragmentShader").c_str(), GL_FRAGMENT_SHADER);

		program = glCreateProgram();
		glAttachShader(program, shaders[0]);
		glAttachShader(program, shaders[1]);
		glAttachShader(program, shaders[2]);
		
		glBindAttribLocation(program, 0, "in_Position");
		//glBindAttribLocation(program, 1, "in_Color");
		
		linkProgram(program);


	}

	void updatePoints() {
		for (int i = 0; i < POINTS_COUNT; i++) {
			
			//srand(randomizer+i);
			float speed = Random(0, 5.f);
			float dirX = Random(-1, 1);
			float dirY = Random(-1, 1);
			float dirZ = Random(-1, 1);
			glm::vec3 dir = glm::normalize(glm::vec3(dirX, dirY, dirZ));
			
			//std::cout << "RAND: " << rand() * .00001f << std::endl;
			positions[i] += dir * speed;
			
			// CLAMP
			positions[i].x = glm::clamp(positions[i].x, (float)-(SPACE_WIDTH / 2), (float)SPACE_WIDTH / 2);
			positions[i].y = glm::clamp(positions[i].y, .0f, (float)SPACE_WIDTH);
			positions[i].z = glm::clamp(positions[i].z, (float)-(SPACE_DEPTH / 2), (float)SPACE_DEPTH / 2);
		}
		randomizer++;
	}

	void drawPoints() {
			glBindVertexArray(vao);
			glUseProgram(program);
			glUniformMatrix4fv(glGetUniformLocation(program, "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));

			glUniformMatrix4fv(glGetUniformLocation(program, "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));

			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glPointSize(10);

			for (int i = 0; i < POINTS_COUNT; i++) {

				objMat = glm::translate(glm::mat4(1), positions[i]);
				glUniformMatrix4fv(glGetUniformLocation(program, "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
				glDrawArrays(GL_POINTS, i, 1);
			}

	}
}
namespace Honeycomb {
	GLuint vao, vbo;
	GLuint shaders[3];
	GLuint program;
	static glm::vec3 positions[POINTS_COUNT];
	glm::mat4 objMat(1.f);

	void setupPoints() {

		for (int i = 0; i < POINTS_COUNT; i++) {
			if (i % 2 == 0) {
				positions[i] = glm::vec3(0, GlobalVars::CUBE_WIDTH * i, 0);

			}
			else {
				positions[i] = glm::vec3(GlobalVars::CUBE_WIDTH, GlobalVars::CUBE_WIDTH * i, GlobalVars::CUBE_WIDTH);
			}

		}


		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		shaders[0] = compileShader(ResourcesManager::ReadFile(".\\vertexShader").c_str(), GL_VERTEX_SHADER);
		shaders[1] = compileShader(ResourcesManager::ReadFile(".\\hcGeometryShader").c_str(), GL_GEOMETRY_SHADER);
		shaders[2] = compileShader(ResourcesManager::ReadFile(".\\fragmentShader").c_str(), GL_FRAGMENT_SHADER);

		program = glCreateProgram();
		glAttachShader(program, shaders[0]);
		glAttachShader(program, shaders[1]);
		glAttachShader(program, shaders[2]);

		glBindAttribLocation(program, 0, "in_Position");
		//glBindAttribLocation(program, 1, "in_Color");

		linkProgram(program);


	}

	void updatePoints(float dt) {

		static float s = 0;

		s += dt;

		if (dt >= TAU) {
			s = 0;
		}
		
		GlobalVars::MUTATOR = MapValue(cos(s), -1, 1, 0.1f, 0.8f);
	}

	void drawPoints() {
		glBindVertexArray(vao);
		glUseProgram(program);
		glUniformMatrix4fv(glGetUniformLocation(program, "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));

		glUniformMatrix4fv(glGetUniformLocation(program, "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));

		glUniformMatrix4fv(glGetUniformLocation(program, "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));

		glUniform1f(glGetUniformLocation(program, "mutationFactor"), GlobalVars::MUTATOR);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glPointSize(10);

		glDrawArrays(GL_POINTS, 0, POINTS_COUNT);
		//for (int i = 0; i < POINTS_COUNT; i++) {

		//	objMat = glm::translate(glm::mat4(1), positions[i]);
		//	glUniformMatrix4fv(glGetUniformLocation(program, "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
		//	glDrawArrays(GL_POINTS, i, 1);
		//}

	}
}

/////////////////////////////////////////////////



void GLinit(int width, int height) {
	glViewport(0, 0, width, height);
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);
	glClearDepth(1.f);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	RV::_projection = glm::perspective(RV::FOV, (float)width / (float)height, RV::zNear, RV::zFar);



	// Setup shaders & geometry
	Points::setupPoints();
	Honeycomb::setupPoints();
	Axis::setupAxis();
	Cube::setupCube();
	


	/////////////////////////////////////////////////////TODO
	// Do your init code here
	// ...
	// ...
	// ...
	/////////////////////////////////////////////////////////
}

void GLcleanup() {
	
	Axis::cleanupAxis();
	Cube::cleanupCube();

	/////////////////////////////////////////////////////TODO
	// Do your cleanup code here
	// ...
	// ...
	// ...
	/////////////////////////////////////////////////////////
}

void GLrender(float dt) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	RV::_modelView = glm::mat4(1.f);
	RV::_modelView = glm::translate(RV::_modelView, glm::vec3(RV::panv[0], RV::panv[1], RV::panv[2]));
	RV::_modelView = glm::rotate(RV::_modelView, RV::rota[1], glm::vec3(1.f, 0.f, 0.f));
	RV::_modelView = glm::rotate(RV::_modelView, RV::rota[0], glm::vec3(0.f, 1.f, 0.f));

	RV::_MVP = RV::_projection * RV::_modelView;

	//Points::updatePoints();
	switch (GlobalVars::CURRENT_SCENE)
	{
	case 0:
	{
		Points::updatePoints();
		Points::drawPoints();
	}break;
	case 1:
	{
		Honeycomb::updatePoints(dt);
		Honeycomb::drawPoints();

	}break;
	case 2:
	{

	}break;
	}
	Axis::drawAxis();
	//Cube::drawCube();
	/////////////////////////////////////////////////////TODO
	// Do your render code here
	// ...
	// ...
	// ...
	/////////////////////////////////////////////////////////

	ImGui::Render();
}

void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		/////////////////////////////////////////////////////TODO
		// Do your GUI code here....
		// ...
		// ...
		// ...
		/////////////////////////////////////////////////////////
	}
	// .........................

	ImGui::End();

	// Example code -- ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
	bool show_test_window = false;
	if (show_test_window) {
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
		ImGui::ShowTestWindow(&show_test_window);
	}
}