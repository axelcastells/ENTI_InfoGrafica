#include <GL\glew.h>
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <cstdio>
#include <cassert>
#include <chrono>

#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <vector>
#include <iostream>

#include "GL_framework.h"

#pragma region Externs
namespace ModelLoader {
	extern bool LoadOBJ(const char* path, std::vector< glm::vec3> & out_vertices, std::vector< glm::vec2> & out_uvs, std::vector< glm::vec3> & out_normals);
}
namespace ResourcesManager {
	extern std::string ReadFile(const char* filePath);
}
#pragma endregion
float Random(float min, float max) {

	return min + (rand() - 0) * (max - min) / (RAND_MAX - 0);
}
float MapValue(float val, float inMin, float inMax, float outMin, float outMax) {
	return outMin + (val - inMin) * (outMax - outMin) / (inMax - inMin);
}

#pragma region Global Vars
static float WHEEL_DISTANCE = 56;
static float WHEEL_FREQUENCE = 0.01f;
static unsigned int CURRENT_SCENE = 0;
static float CUBE_WIDTH = 1.2f;
static float MUTATOR = 0.1f;

static float CAMERA_DISTANCE = 30;

static float CURRENT_TIME = 0;
static GLuint PROGRAM[2];
static GLuint SHADERS[4];

static float AMBIENT_VALUE = 0;
static float SPECULAR_VALUE = 0;
static float DIFFUSE_VALUE = 0;

static glm::vec3 col(.3f, .5f, .2f);
static glm::vec3 lightCol(1);
static glm::vec3 lightPos(5, 10, 0);
static float LIGHT_SPECULAR_INTENSITY = 1;
//glm::vec3 camPos(0, 0, -10);

static glm::vec3 POINT_LIGHT_POS = { 0,0,0 };
static float POINT_LIGHT_OFFSET = 1;
static glm::vec3 POINT_LIGHT_COL = { 0,0,0 };
static float POINT_LIGHT_SPECULAR = 0;
static float POINT_LIGHT_FREQ = 1.f;
static float POINT_LIGHT_SPECULAR_INTENSITY = 1;

static float OSCILATION_FREQUENCY = 1;

static float TIME_SCALE = 1.f;

#pragma endregion

#define POINTS_COUNT 10

#define SPACE_WIDTH 10
#define SPACE_HEIGHT 10
#define SPACE_DEPTH 10

#define MIN_SPEED 0
#define MAX_SPEED 10

#define PI 3.1415926535897932384626433832795
#define TAU PI*2

#define VERT_SHADER_CABINS "vertexShader"
#define FRAG_SHADER_CABINS "fragmentShader"
#define VERT_SHADER_STENCIL "vertexShaderStencil"
#define FRAG_SHADER_STENCIL "fragmentShaderStencil"


#define OBJ_PATH_CABIN "Cabin.obj"
#define OBJ_PATH_WHEEL "Wheel.obj"
#define OBJ_PATH_BASE "Base.obj"
#define OBJ_PATH_TRUMP "Trump.obj"
#define OBJ_PATH_CHICKEN "Chicken.obj"

#define SINGLE_FRAME_ANGLE(_dt) (float)(TAU * WHEEL_FREQUENCE * _dt)

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
	const float zFar = 150.f;

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
void linkProgram(GLuint PROGRAM) {
	glLinkProgram(PROGRAM);
	GLint res;
	glGetProgramiv(PROGRAM, GL_LINK_STATUS, &res);
	if (res == GL_FALSE) {
		glGetProgramiv(PROGRAM, GL_INFO_LOG_LENGTH, &res);
		char *buff = new char[res];
		glGetProgramInfoLog(PROGRAM, res, &res, buff);
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

/////////////////////////////////////////////////
// CABINS
namespace Cabins {
	int COUNT = 20; //20
	GLuint vao;
	GLuint vbo[3];

	float outline = 1.1f;
	glm::vec3 color = { 1,0,0 };

	glm::mat4 objMat = glm::mat4(1.f);

	std::vector<glm::vec3> dataVerts;
	std::vector<glm::vec3> dataNorms;
	std::vector<glm::vec2> dataUvs;

	void setup() {
		ModelLoader::LoadOBJ(OBJ_PATH_CABIN, dataVerts, dataUvs, dataNorms);

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(3, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataVerts.size(), dataVerts.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataNorms.size(), dataNorms.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * dataUvs.size(), dataUvs.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)2, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

	}
	void GetPositionInWheel(glm::mat4 &_objMat, int _index, float _t) {

		float axisA = WHEEL_DISTANCE * glm::cos(TAU * WHEEL_FREQUENCE*_t + (TAU*_index) / COUNT);
		float axisB = WHEEL_DISTANCE * glm::sin(TAU * WHEEL_FREQUENCE*_t + (TAU*_index) / COUNT);

		glm::vec3 traslation(0, axisA, axisB);

		_objMat = glm::translate(_objMat, traslation);
	}

	void Update(float _dt) {
		CURRENT_TIME += _dt;
	}

	void draw() {
		glBindVertexArray(vao);

		glEnable(GL_STENCIL_TEST);

		// Draw floor


		glStencilFunc(GL_ALWAYS, 1, 0xFF); // Set any stencil to 1
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF); // Write to stencil buffer
		glDepthMask(GL_FALSE); // Don't write to depth buffer
		glClear(GL_STENCIL_BUFFER_BIT); // Clear stencil buffer (0 by default)



		glUseProgram(PROGRAM[0]);

		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));

		glUniform1f(glGetUniformLocation(PROGRAM[0], "ambientValue"), AMBIENT_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularValue"), SPECULAR_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "diffuseValue"), DIFFUSE_VALUE);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularIntensity"), LIGHT_SPECULAR_INTENSITY);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointSpecularIntensity"), POINT_LIGHT_SPECULAR_INTENSITY);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointLightSpecular"), POINT_LIGHT_SPECULAR);
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightPos"), 1, glm::value_ptr(POINT_LIGHT_POS));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightColor"), 1, glm::value_ptr(POINT_LIGHT_COL));

		glUniform3fv(glGetUniformLocation(PROGRAM[0], "objectColor"), 1, glm::value_ptr(col));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightColor"), 1, glm::value_ptr(lightCol));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightPos"), 1, glm::value_ptr(lightPos));

		for (int i = 0; i < COUNT; i++) {
			objMat = glm::mat4(1);

			GetPositionInWheel(objMat, i, CURRENT_TIME);
			glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
			glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());


		}




		glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // Pass test if stencil value is 1
		glStencilMask(0x00); // Don't write anything to stencil buffer
		glDepthMask(GL_TRUE); // Write to depth buffer



		glUseProgram(PROGRAM[1]);

		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));

		for (int i = 0; i < COUNT; i++) {
			objMat = glm::mat4(1);

			GetPositionInWheel(objMat, i, CURRENT_TIME);

			//objMat = glm::scale(glm::translate(objMat, glm::vec3(0, 0, -1)), glm::vec3(1, 1, -1));

			glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
			glUniform1f(glGetUniformLocation(PROGRAM[1], "outline"), outline);
			glUniform3fv(glGetUniformLocation(PROGRAM[1], "color"), 1, glm::value_ptr(color));
			glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());


		}
		
		glDisable(GL_STENCIL_TEST);



		glUseProgram(0);
		glBindVertexArray(0);
	}
}
// WHEEL
namespace Wheel {
	GLuint vao;
	GLuint vbo[3];
	glm::mat4 objMat = glm::mat4(1.f);

	float outline = 1.1f;
	glm::vec3 color = { 1,0,0 };

	std::vector<glm::vec3> dataVerts;
	std::vector<glm::vec3> dataNorms;
	std::vector<glm::vec2> dataUvs;

	void setup() {
		ModelLoader::LoadOBJ(OBJ_PATH_WHEEL, dataVerts, dataUvs, dataNorms);

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(3, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataVerts.size(), dataVerts.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataNorms.size(), dataNorms.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * dataUvs.size(), dataUvs.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)2, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void Update(float _dt) {
		objMat = glm::rotate(objMat, SINGLE_FRAME_ANGLE(_dt), glm::vec3(1, 0, 0));
	}

	void draw() {
		glEnable(GL_STENCIL_TEST);

		// Draw floor


		glStencilFunc(GL_ALWAYS, 1, 0xFF); // Set any stencil to 1
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF); // Write to stencil buffer
		glDepthMask(GL_FALSE); // Don't write to depth buffer
		glClear(GL_STENCIL_BUFFER_BIT); // Clear stencil buffer (0 by default)

		glBindVertexArray(vao);
		glUseProgram(PROGRAM[0]);

		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));

		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularIntensity"), LIGHT_SPECULAR_INTENSITY);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointSpecularIntensity"), POINT_LIGHT_SPECULAR_INTENSITY);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "ambientValue"), AMBIENT_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularValue"), SPECULAR_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "diffuseValue"), DIFFUSE_VALUE);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointLightSpecular"), POINT_LIGHT_SPECULAR);
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightPos"), 1, glm::value_ptr(POINT_LIGHT_POS));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightColor"), 1, glm::value_ptr(POINT_LIGHT_COL));

		glUniform3fv(glGetUniformLocation(PROGRAM[0], "objectColor"), 1, glm::value_ptr(col));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightColor"), 1, glm::value_ptr(lightCol));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightPos"), 1, glm::value_ptr(lightPos));
		glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());


		glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // Pass test if stencil value is 1
		glStencilMask(0x00); // Don't write anything to stencil buffer
		glDepthMask(GL_TRUE); // Write to depth buffer

		glUseProgram(PROGRAM[1]);
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
		glUniform1f(glGetUniformLocation(PROGRAM[1], "outline"), outline);
		glUniform3fv(glGetUniformLocation(PROGRAM[1], "color"), 1, glm::value_ptr(color));
		glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());

		glDisable(GL_STENCIL_TEST);

		glUseProgram(0);
		glBindVertexArray(0);
	}
}
// BASE
namespace Base {
	GLuint vao;
	GLuint vbo[3];
	glm::mat4 objMat = glm::mat4(1.f);

	float outline = 1.1f;
	glm::vec3 color = { 1,0,0 };

	std::vector<glm::vec3> dataVerts;
	std::vector<glm::vec3> dataNorms;
	std::vector<glm::vec2> dataUvs;

	void setup() {
		ModelLoader::LoadOBJ(OBJ_PATH_BASE, dataVerts, dataUvs, dataNorms);

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(3, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataVerts.size(), dataVerts.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataNorms.size(), dataNorms.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * dataUvs.size(), dataUvs.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)2, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void draw() {
		glEnable(GL_STENCIL_TEST);

		// Draw floor


		glStencilFunc(GL_ALWAYS, 1, 0xFF); // Set any stencil to 1
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF); // Write to stencil buffer
		glDepthMask(GL_FALSE); // Don't write to depth buffer
		glClear(GL_STENCIL_BUFFER_BIT); // Clear stencil buffer (0 by default)

		glBindVertexArray(vao);
		glUseProgram(PROGRAM[0]);

		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));

		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularIntensity"), LIGHT_SPECULAR_INTENSITY);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointSpecularIntensity"), POINT_LIGHT_SPECULAR_INTENSITY);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "ambientValue"), AMBIENT_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularValue"), SPECULAR_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "diffuseValue"), DIFFUSE_VALUE);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointLightSpecular"), POINT_LIGHT_SPECULAR);
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightPos"), 1, glm::value_ptr(POINT_LIGHT_POS));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightColor"), 1, glm::value_ptr(POINT_LIGHT_COL));

		glUniform3fv(glGetUniformLocation(PROGRAM[0], "objectColor"), 1, glm::value_ptr(col));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightColor"), 1, glm::value_ptr(lightCol));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightPos"), 1, glm::value_ptr(lightPos));
		glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());


		glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // Pass test if stencil value is 1
		glStencilMask(0x00); // Don't write anything to stencil buffer
		glDepthMask(GL_TRUE); // Write to depth buffer

		glUseProgram(PROGRAM[1]);
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
		glUniform1f(glGetUniformLocation(PROGRAM[1], "outline"), outline);
		glUniform3fv(glGetUniformLocation(PROGRAM[1], "color"), 1, glm::value_ptr(color));
		glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());

		glDisable(GL_STENCIL_TEST);

		glUseProgram(0);
		glBindVertexArray(0);
	}
}
// CHARACTERS
namespace Trump {
	GLuint vao;
	GLuint vbo[3];
	glm::mat4 objMat = glm::mat4(1.f);
	glm::mat4 worldMat(1);

	float outline = 1.1f;
	glm::vec3 color = { 1,0,0 };

	std::vector<glm::vec3> dataVerts;
	std::vector<glm::vec3> dataNorms;
	std::vector<glm::vec2> dataUvs;

	void setup() {
		ModelLoader::LoadOBJ(OBJ_PATH_TRUMP, dataVerts, dataUvs, dataNorms);

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(3, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataVerts.size(), dataVerts.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataNorms.size(), dataNorms.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * dataUvs.size(), dataUvs.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)2, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void Update(float _dt) {
		objMat = glm::mat4(1);
		Cabins::GetPositionInWheel(objMat, 0, CURRENT_TIME);
		objMat = glm::translate(objMat, glm::vec3(0,-5.5f,-1.5f));
		objMat = glm::scale(objMat, glm::vec3(2));
		worldMat = objMat;
	}

	void draw() {
		glEnable(GL_STENCIL_TEST);

		// Draw floor


		glStencilFunc(GL_ALWAYS, 1, 0xFF); // Set any stencil to 1
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF); // Write to stencil buffer
		glDepthMask(GL_FALSE); // Don't write to depth buffer
		glClear(GL_STENCIL_BUFFER_BIT); // Clear stencil buffer (0 by default)

		glBindVertexArray(vao);
		glUseProgram(PROGRAM[0]);

		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));

		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularIntensity"), LIGHT_SPECULAR_INTENSITY);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointSpecularIntensity"), POINT_LIGHT_SPECULAR_INTENSITY);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "ambientValue"), AMBIENT_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularValue"), SPECULAR_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "diffuseValue"), DIFFUSE_VALUE);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointLightSpecular"), POINT_LIGHT_SPECULAR);
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightPos"), 1, glm::value_ptr(POINT_LIGHT_POS));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightColor"), 1, glm::value_ptr(POINT_LIGHT_COL));

		glUniform3fv(glGetUniformLocation(PROGRAM[0], "objectColor"), 1, glm::value_ptr(col));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightColor"), 1, glm::value_ptr(lightCol));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightPos"), 1, glm::value_ptr(lightPos));
		glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());


		glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // Pass test if stencil value is 1
		glStencilMask(0x00); // Don't write anything to stencil buffer
		glDepthMask(GL_TRUE); // Write to depth buffer

		glUseProgram(PROGRAM[1]);
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
		glUniform1f(glGetUniformLocation(PROGRAM[1], "outline"), outline);
		glUniform3fv(glGetUniformLocation(PROGRAM[1], "color"), 1, glm::value_ptr(color));
		glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());

		glDisable(GL_STENCIL_TEST);

		glUseProgram(0);
		glBindVertexArray(0);
	}
}
namespace Chicken {
	GLuint vao;
	GLuint vbo[3];
	glm::mat4 objMat(1);
	glm::mat4 worldMat(1);

	float outline = 1.1f;
	glm::vec3 color = { 1,0,0 };

	std::vector<glm::vec3> dataVerts;
	std::vector<glm::vec3> dataNorms;
	std::vector<glm::vec2> dataUvs;

	void setup() {
		ModelLoader::LoadOBJ(OBJ_PATH_CHICKEN, dataVerts, dataUvs, dataNorms);

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(3, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataVerts.size(), dataVerts.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * dataNorms.size(), dataNorms.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * dataUvs.size(), dataUvs.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)2, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void Update(float _dt) {
		objMat = glm::mat4(1);
		Cabins::GetPositionInWheel(objMat, 0, CURRENT_TIME);
		objMat = glm::translate(objMat, glm::vec3(0, -4.5f, 1.5f));
		objMat = glm::scale(objMat, glm::vec3(2,2,-2));
		worldMat = objMat;
	}

	void draw() {
		glEnable(GL_STENCIL_TEST);

		// Draw floor


		glStencilFunc(GL_ALWAYS, 1, 0xFF); // Set any stencil to 1
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glStencilMask(0xFF); // Write to stencil buffer
		glDepthMask(GL_FALSE); // Don't write to depth buffer
		glClear(GL_STENCIL_BUFFER_BIT); // Clear stencil buffer (0 by default)

		glBindVertexArray(vao);
		glUseProgram(PROGRAM[0]);

		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[0], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));

		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularIntensity"), LIGHT_SPECULAR_INTENSITY);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointSpecularIntensity"), POINT_LIGHT_SPECULAR_INTENSITY);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "ambientValue"), AMBIENT_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "specularValue"), SPECULAR_VALUE);
		glUniform1f(glGetUniformLocation(PROGRAM[0], "diffuseValue"), DIFFUSE_VALUE);

		glUniform1f(glGetUniformLocation(PROGRAM[0], "pointLightSpecular"), POINT_LIGHT_SPECULAR);
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightPos"), 1, glm::value_ptr(POINT_LIGHT_POS));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "pointLightColor"), 1, glm::value_ptr(POINT_LIGHT_COL));

		glUniform3fv(glGetUniformLocation(PROGRAM[0], "objectColor"), 1, glm::value_ptr(col));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightColor"), 1, glm::value_ptr(lightCol));
		glUniform3fv(glGetUniformLocation(PROGRAM[0], "lightPos"), 1, glm::value_ptr(lightPos));
		glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());


		glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // Pass test if stencil value is 1
		glStencilMask(0x00); // Don't write anything to stencil buffer
		glDepthMask(GL_TRUE); // Write to depth buffer

		glUseProgram(PROGRAM[1]);
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniformMatrix4fv(glGetUniformLocation(PROGRAM[1], "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
		glUniform1f(glGetUniformLocation(PROGRAM[1], "outline"), outline);
		glUniform3fv(glGetUniformLocation(PROGRAM[1], "color"), 1, glm::value_ptr(color));
		glDrawArrays(GL_TRIANGLES, 0, dataVerts.size());

		glDisable(GL_STENCIL_TEST);

		glUseProgram(0);
		glBindVertexArray(0);
	}
}


void ReloadShaders() {
	SHADERS[0] = compileShader(ResourcesManager::ReadFile(VERT_SHADER_CABINS).c_str(), GL_VERTEX_SHADER);
	SHADERS[1] = compileShader(ResourcesManager::ReadFile(FRAG_SHADER_CABINS).c_str(), GL_FRAGMENT_SHADER);
	SHADERS[2] = compileShader(ResourcesManager::ReadFile(VERT_SHADER_STENCIL).c_str(), GL_VERTEX_SHADER);
	SHADERS[3] = compileShader(ResourcesManager::ReadFile(FRAG_SHADER_STENCIL).c_str(), GL_FRAGMENT_SHADER);

	PROGRAM[0] = glCreateProgram();
	glAttachShader(PROGRAM[0], SHADERS[0]);
	glAttachShader(PROGRAM[0], SHADERS[1]);
	glBindAttribLocation(PROGRAM[0], 0, "in_Position");
	glBindAttribLocation(PROGRAM[0], 1, "in_Normal");
	linkProgram(PROGRAM[0]);	
	
	PROGRAM[1] = glCreateProgram();
	glAttachShader(PROGRAM[1], SHADERS[2]);
	glAttachShader(PROGRAM[1], SHADERS[3]);
	glBindAttribLocation(PROGRAM[1], 0, "in_Position");
	glBindAttribLocation(PROGRAM[1], 1, "in_Normal");
	linkProgram(PROGRAM[1]);
}


void GLinit(int width, int height) {
	glViewport(0, 0, width, height);
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);
	glClearDepth(1.f);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	RV::_projection = glm::perspective(RV::FOV, (float)width / (float)height, RV::zNear, RV::zFar);



	// Setup shaders & geometry
	ReloadShaders();
	//SHADERS[0] = compileShader(ResourcesManager::ReadFile(VERT_SHADER_CABINS).c_str(), GL_VERTEX_SHADER);
	//SHADERS[1] = compileShader(ResourcesManager::ReadFile(FRAG_SHADER_CABINS).c_str(), GL_FRAGMENT_SHADER);



	Axis::setupAxis();
	Cube::setupCube();

	Base::setup();
	Wheel::setup();
	Cabins::setup();
	Trump::setup();
	Chicken::setup();
	


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
// Cabina:	0
// Vaca:	1
// Pollo:	2
glm::mat4 CameraLookAtMatrix(glm::mat4 cameraMat, unsigned int targetId) {
		glm::vec4 point(0);
		point.w = 1;
		glm::mat4 cabinMat;
		Cabins::GetPositionInWheel(cabinMat, 0, CURRENT_TIME);
	if (targetId == 0) {
		glm::vec3 newTarget = cabinMat * point;
		glm::vec3 newEye = newTarget + glm::vec3(CAMERA_DISTANCE,0,0);
		cameraMat = glm::lookAt(newEye, newTarget, glm::vec3(0, 1, 0));
		return cameraMat;
	}
	else if (targetId == 1) {
		glm::vec3 newTarget = Trump::worldMat * point;
		glm::vec3 newEye = Chicken::worldMat * point;
		cameraMat = glm::lookAt(newEye + glm::vec3(0), newTarget, glm::vec3(0, 1, 0));
		return cameraMat;
	}
	else if (targetId == 2) {
		glm::vec3 newTarget = Chicken::worldMat * point;
		glm::vec3 newEye = Trump::worldMat * point;
		cameraMat = glm::lookAt(newEye + glm::vec3(0), newTarget, glm::vec3(0, 1, 0));
		return cameraMat;
	}
	else {
		std::cout << "FAILED TO USE CameraLookAt() invalid targetId" << std::endl;
		cameraMat = glm::mat4(1);
		return cameraMat;
	}
}

void GLrender(float dt) {
	dt *= TIME_SCALE;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



	//Points::updatePoints();
	switch (CURRENT_SCENE)
	{
	case 0:
	{
		static float counter = 0;
		counter += dt;
		if (counter >= 360) {
			counter = 0;
		}

		Wheel::Update(dt);
		Cabins::Update(dt);
		Trump::Update(dt);
		Chicken::Update(dt);

		glm::mat4 cabinTranslation(1);
		Cabins::GetPositionInWheel(cabinTranslation, 0, CURRENT_TIME);
		POINT_LIGHT_POS = RV::_modelView * cabinTranslation * glm::vec4(0,0,0,1);


		// OSCILATION
		glm::vec3 offset(0);
		offset.x = POINT_LIGHT_OFFSET * glm::cos(counter * OSCILATION_FREQUENCY);
		offset.y = -(POINT_LIGHT_OFFSET * glm::abs(glm::sin(counter * OSCILATION_FREQUENCY)));

		POINT_LIGHT_POS += offset;


		static int cameraMode = 0;

		// TIMER
#if(false)
		static std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::high_resolution_clock::now();
		std::chrono::time_point<std::chrono::steady_clock> currentTime = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> span = std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - startTime);

		if (cameraMode == 0 && span.count() >= 2) {
			cameraMode = 1;
			startTime = std::chrono::high_resolution_clock::now();
		}
		else if (cameraMode == 1 && span.count() >= 1) {
			cameraMode = 2;
			startTime = std::chrono::high_resolution_clock::now();
		}		
		else if (cameraMode == 2 && span.count() >= 1) {
			cameraMode = 1;
			startTime = std::chrono::high_resolution_clock::now();
		}
		// ----
#endif
		RV::_modelView = glm::mat4(1.f);
		RV::_modelView = CameraLookAtMatrix(RV::_modelView, cameraMode);

		//RV::_modelView is the camera position

		//RV::_modelView = glm::translate(RV::_modelView, glm::vec3(RV::panv[0], RV::panv[1], RV::panv[2]));
		//RV::_modelView = glm::rotate(RV::_modelView, RV::rota[1], glm::vec3(1.f, 0.f, 0.f));
		//RV::_modelView = glm::rotate(RV::_modelView, RV::rota[0], glm::vec3(0.f, 1.f, 0.f));

		RV::_MVP = RV::_projection * RV::_modelView;



		Wheel::draw();
		Cabins::draw();
		Base::draw();
		Trump::draw();
		Chicken::draw();
	}break;
	case 1:
	{

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

		ImGui::Text("Choose the exercise");
		if (ImGui::Button("Current Scene")) {
			if (CURRENT_SCENE == 0) CURRENT_SCENE = 1;
			else if (CURRENT_SCENE == 1) CURRENT_SCENE = 0;
		}

		if (ImGui::Button("Reload Shaders")) {
			ReloadShaders();
		}
		/*glm::vec3 col(.3f, .5f, .2f);
glm::vec3 lightCol(1);
glm::vec3 lightPos(5, 10, 0);
glm::vec3 camPos(0, 0, -10);*/
		
		ImGui::Text("[Lights]");
		ImGui::Text("Sky");
		ImGui::SliderFloat3("Sky Light Position", &lightPos.x, -100, 100);
		ImGui::SliderFloat3("Sky Light Color", &lightCol.x, 0, 1, "%.3f");

		ImGui::SliderFloat("Sky Diffuse", &DIFFUSE_VALUE, 0, 1, "%.3f");
		ImGui::SliderFloat("Sky Ambient", &AMBIENT_VALUE, 0, 1, "%.3f");
		ImGui::SliderFloat("Sky Specular", &SPECULAR_VALUE, 0, 1, "%.3f");
		ImGui::SliderFloat("Sky Specular Intensity", &LIGHT_SPECULAR_INTENSITY, 0, 100, "%.3f");


		ImGui::Text("Light Bulb");
		ImGui::SliderFloat("Point Light Position", &POINT_LIGHT_OFFSET, 0, 3);
		ImGui::SliderFloat("Point Light Oscilation", &OSCILATION_FREQUENCY, 0.1f, 3.f);
		ImGui::SliderFloat3("Point Light Color", &POINT_LIGHT_COL.x, 0, 1, "%.3f");
		ImGui::SliderFloat("Point Specular", &POINT_LIGHT_SPECULAR, 0, 1, "%.3f");
		ImGui::SliderFloat("Point Specular Intensity", &POINT_LIGHT_SPECULAR_INTENSITY, 0, 100, "%.3f");

		ImGui::Spacing();
		ImGui::SliderFloat3("Object Color", &col.x, 0, 1, "%.3f");
		ImGui::Spacing();
		ImGui::Text("[Outlines]");
		ImGui::SliderFloat3("Cabin Color", &Cabins::color.x, 0, 1);
		ImGui::SliderFloat("Cabin Outline", &Cabins::outline, 1, 2);
		ImGui::SliderFloat3("Base Color", &Base::color.x, 0, 1);
		ImGui::SliderFloat("Base Outline", &Base::outline, 1, 2);
		ImGui::SliderFloat3("Wheel Color", &Wheel::color.x, 0, 1);
		ImGui::SliderFloat("Wheel Outline", &Wheel::outline, 1, 2);
		ImGui::SliderFloat3("Chicken Color", &Chicken::color.x, 0, 1);
		ImGui::SliderFloat("Chicken Outline", &Chicken::outline, 1, 2);
		ImGui::SliderFloat3("Trump Color", &Trump::color.x, 0, 1);
		ImGui::SliderFloat("Trump Outline", &Trump::outline, 1, 2);
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