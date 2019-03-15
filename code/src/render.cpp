#include <GL\glew.h>
#include <glm\gtc\type_ptr.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <cstdio>
#include <cassert>
#include <iostream>

#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>

#include "GL_framework.h"
#include <vector>

bool zoombutton;
bool FOVbutton;

//mínim i màxim del Ambient Light
#define MIN_AMBIENT 0.0
#define MAX_AMBIENT 1.0

//mínim i màxim del Diffuse Light
#define MIN_DIFFUSE 0.0
#define MAX_DIFFUSE 1.0

//mínim i màxim del Specular Light
#define MIN_SPECULAR 0.0
#define MAX_SPECULAR 1.0

#pragma region Externs
namespace ModelLoader {
	extern bool LoadOBJ(const char* path, std::vector< glm::vec3> & out_vertices, std::vector< glm::vec2> & out_uvs, std::vector< glm::vec3> & out_normals);
}
namespace ResourcesManager {
	extern std::string ReadFile(const char* filePath);
}
#pragma endregion

namespace GlobalVars {
	static float SPECULAR_VALUE = .8f;
	static float AMBIENT_VALUE = .2f;
	static float DIFFUSE_VALUE = .3f;

	static int WIDTH;
	static int HEIGHT;
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
	float FOV = glm::radians(65.f);
	float zNear = 1.f;
	float zFar = 50.f;

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
	GlobalVars::WIDTH = width;
	GlobalVars::HEIGHT = height;
	glViewport(0, 0, width, height);
	if (height != 0) RV::_projection = glm::perspective(RV::FOV, (float)width / (float)height, RV::zNear, RV::zFar);
	else RV::_projection = glm::perspective(RV::FOV, 0.f, RV::zNear, RV::zFar);
}

void GLmousecb(MouseEvent ev) {
	if (RV::prevMouse.waspressed && RV::prevMouse.button == ev.button) {
		float diffx = ev.posx - RV::prevMouse.lastx;
		float diffy = ev.posy - RV::prevMouse.lasty;
		switch (ev.button) {
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
	}
	else {
		RV::prevMouse.button = ev.button;
		RV::prevMouse.waspressed = true;
	}
	RV::prevMouse.lastx = ev.posx;
	RV::prevMouse.lasty = ev.posy;
}

//////////////////////////////////////////////////
GLuint compileShader(const char* shaderStr, GLenum shaderType, const char* name = "") {
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
	void drawCube(float dt) {
		glEnable(GL_PRIMITIVE_RESTART);
		glBindVertexArray(cubeVao);
		glUseProgram(cubeProgram);

		static float accum = 0.f;
		accum += dt;

		if (accum > glm::two_pi<float>()) {
			accum = 0.f;

		}
		GLfloat col[] = {
			0.5f*glm::sin(accum) + 0.5,
			0.5f*glm::sin(5 * accum) + 0.5,
			0.5f*glm::sin(10 * accum) + 0.5,
			1.f
		};
		// Cube 1

		objMat = glm::translate(glm::mat4(1), glm::vec3(1, 0, 2.5f));

		glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
		glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "mv_Mat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_modelView));
		glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "mvpMat"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));
		glUniform4f(glGetUniformLocation(cubeProgram, "color"), 0.1f, 1.f, 1.f, 0.f);
		glDrawElements(GL_TRIANGLE_STRIP, numVerts, GL_UNSIGNED_BYTE, 0);

		//// Cube 2

		objMat = glm::translate(objMat, glm::vec3(1, 0, 2.5f));
		glUniformMatrix4fv(glGetUniformLocation(cubeProgram, "objMat"), 1, GL_FALSE, glm::value_ptr(objMat));
		glDrawElements(GL_TRIANGLE_STRIP, numVerts, GL_UNSIGNED_BYTE, 0);

		glUseProgram(0);
		glBindVertexArray(0);
		glDisable(GL_PRIMITIVE_RESTART);
	}

	
}

////////////////////////////////////////////////// MODEL
namespace Model {
	// MODEL
	std::vector< glm::vec3> out_vertices;
	std::vector< glm::vec2> out_uvs;
	std::vector< glm::vec3> out_normals;

	GLuint cubeVao;
	GLuint cubeVbo[3];
	GLuint shaders[2];
	GLuint program;
	glm::mat4 objMat = glm::mat4(1.f);

	const char* model_vertShader =
"#version 330 core\n\
in vec3 aPos;\n\
in vec3 aNormal;\n\
\n\
	out vec3 FragPos;\n\
	out vec3 Normal;\n\
\n\
	uniform mat4 model;\n\
	//uniform mat4 view;\n\
	uniform mat4 mvp;\n\
\n\
	void main()\n\
	{\n\
		FragPos = vec3(model * vec4(aPos, 1.0));\n\
		Normal = mat3(transpose(inverse(model))) * aNormal;\n\
		\n\
		gl_Position = mvp * vec4(FragPos, 1.0);\n\
	}\n\
";
	const char* model_fragShader =
		"#version 330 core\n\
		out vec4 FragColor;\n\
\n\
	in vec3 Normal;\n\
	in vec3 FragPos;\n\
\n\
	uniform float ambientValue;\n\
	uniform float specularValue;\n\
	uniform float diffuseValue;\n\
		\n\
	uniform vec3 lightPos;\n\
	uniform vec3 viewPos;\n\
	uniform vec3 lightColor;\n\
	uniform vec3 objectColor;\n\
\n\
	void main()\n\
	{\n\
		// ambient\n\
		vec3 ambient = ambientValue * lightColor;\n\
\n\
		// diffuse \n\
		vec3 norm = normalize(Normal);\n\
		vec3 lightDir = normalize(lightPos - FragPos);\n\
		float diff = max(dot(norm, lightDir), 0.0);\n\
		vec3 diffuse = diffuseValue * (diff * lightColor);\n\
\n\
		// specular\n\
		vec3 viewDir = normalize(viewPos - FragPos);\n\
		vec3 reflectDir = reflect(-lightDir, norm);\n\
		float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n\
		vec3 specular = specularValue * spec * lightColor;\n\
\n\
		vec3 result = (ambient + diffuse + specular) * objectColor;\n\
		FragColor = vec4(result, 1.0);\n\
	} ";

	void setupModel() {

		ModelLoader::LoadOBJ("deer.obj", out_vertices, out_uvs, out_normals);

		glGenVertexArrays(1, &cubeVao);
		glBindVertexArray(cubeVao);
		glGenBuffers(3, cubeVbo);

		glBindBuffer(GL_ARRAY_BUFFER, cubeVbo[0]);
		glBufferData(GL_ARRAY_BUFFER, out_vertices.size() * sizeof(glm::vec3), out_vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, cubeVbo[1]);
		glBufferData(GL_ARRAY_BUFFER, out_normals.size() * sizeof(glm::vec3), out_normals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer((GLuint)1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);


		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);


		shaders[0] = compileShader(model_vertShader, GL_VERTEX_SHADER, "model_vertShader");
		shaders[1] = compileShader(model_fragShader, GL_FRAGMENT_SHADER, "model_fragShader");

		program = glCreateProgram();
		glAttachShader(program, shaders[0]);
		glAttachShader(program, shaders[1]);
		glBindAttribLocation(program, 0, "aPos");
		glBindAttribLocation(program, 1, "aNormal");
		linkProgram(program);
	}
	void cleanupModel() {
		glDeleteBuffers(3, cubeVbo);
		glDeleteVertexArrays(1, &cubeVao);

		glDeleteProgram(program);
		glDeleteShader(shaders[0]);
		glDeleteShader(shaders[1]);
	}
	void updateModel(const glm::mat4& transform) {
		objMat = transform;
	}
	void drawModel(float dt) {
		glBindVertexArray(cubeVao);
		glUseProgram(program);

		glm::mat4 trans = glm::mat4(1.0f);
		trans = glm::scale(trans, glm::vec3(0.005));

		updateModel(trans);
		glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, glm::value_ptr(objMat));
		glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE, glm::value_ptr(RenderVars::_MVP));

		glm::vec3 col(.3f, .5f, .2f);
		glm::vec3 lightCol(1);
		glm::vec3 lightPos(5,10,0);
		glm::vec3 camPos(0,0,-10);

		glUniform1f(glGetUniformLocation(program, "ambientValue"), GlobalVars::AMBIENT_VALUE);
		glUniform1f(glGetUniformLocation(program, "specularValue"), GlobalVars::SPECULAR_VALUE);
		glUniform1f(glGetUniformLocation(program, "diffuseValue"), GlobalVars::DIFFUSE_VALUE);

		glUniform3fv(glGetUniformLocation(program, "objectColor"), 1, glm::value_ptr(col));
		glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, glm::value_ptr(lightCol));
		glUniform3fv(glGetUniformLocation(program, "lightPos"), 1, glm::value_ptr(lightPos));
		glUniform3fv(glGetUniformLocation(program, "viewPos"), 1, glm::value_ptr(camPos));

		glDrawArrays(GL_TRIANGLES, 0, out_vertices.size());

		glUseProgram(0);
		glBindVertexArray(0);
	}
}

/////////////////////////////////////////////////

GLuint vert_shader;
GLuint frag_shader;
GLuint program;
GLuint vao_vert;

void GLinit(int width, int height) {
	glViewport(0, 0, width, height);
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);
	glClearDepth(1.f);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	GlobalVars::WIDTH = width;
	GlobalVars::HEIGHT = height;
	RV::_projection = glm::perspective(RV::FOV, (float)width / (float)height, RV::zNear, RV::zFar);

	// Setup shaders & geometry
	Axis::setupAxis();
	Cube::setupCube();
	Model::setupModel();

	/////////////////////////////////////////////////////TODO
	// Do your init code here
	// ...
	//vert_shader = compileShader(vertext_shader_source, GL_VERTEX_SHADER, Cube::);
	//frag_shader = compileShader(vertext_shader_source, GL_FRAGMENT_SHADER, cube_fragShader);
	//program = glCreateProgram();
	//glAttachShader(program, vert_shader);
	//glAttachShader(program, frag_shader);
	//linkProgram(program);

	//glGenVertexArrays(1, &vao_vert);

	//glPointSize(40.0f);
	// ...
	// ...
	/////////////////////////////////////////////////////////
}

void GLcleanup() {
	Axis::cleanupAxis();
	Cube::cleanupCube();
	Model::cleanupModel();

	/////////////////////////////////////////////////////TODO
	// Do your cleanup code here
	// ...
	glDeleteVertexArrays(1, &vao_vert);
	glDeleteProgram(program);
	glDeleteShader(vert_shader);
	glDeleteShader(frag_shader);
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





	static float accum = 0.f;
	static float zoom = 0;

	if (zoombutton) {

		RV::_modelView = glm::mat4(1.f);
		RV::_modelView = glm::translate(RV::_modelView, glm::vec3(2.0f - accum, -2.0f, -11.0f));
		//RV::_modelView = glm::rotate(RV::_modelView, glm::radians(20.f), glm::vec3(1.f, 0.f, 0.f));
	}
		
	if (FOVbutton) {
		zoom = 0.0f + sinf((float)accum);
	}

	if (zoombutton || FOVbutton) {
		accum += dt;
		if (accum > glm::two_pi<float>()) {
			accum = 0.f;

		}

		RenderVars::FOV = 65.0f + sinf((float)accum) * 6.2f;

		RV::_projection = glm::perspective(glm::radians(RV::FOV), (float)(GlobalVars::WIDTH / GlobalVars::HEIGHT), 1.0f, 50.0f);

		glm::vec3 cameraPos = glm::vec3(0.0f, 3.0f, 10.0f - zoom);
		RV::_modelView = glm::lookAt(cameraPos, glm::vec3(0), glm::vec3(0.0f, 1.0f, 0.0f));
	}
		

	RV::_MVP = RV::_projection * RV::_modelView;
	Axis::drawAxis();
	Cube::drawCube(dt);
	Model::drawModel(dt);



	ImGui::Render();
}

void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		ImGui::Text("Lights");
		ImGui::DragFloat("Ambient", &GlobalVars::AMBIENT_VALUE, 0.1f, MIN_AMBIENT, MAX_AMBIENT, "%.3f");
		ImGui::DragFloat("Diffuse", &GlobalVars::DIFFUSE_VALUE, 0.1f, MIN_DIFFUSE, MAX_DIFFUSE, "%.3f");
		ImGui::DragFloat("Specular", &GlobalVars::SPECULAR_VALUE, 0.1f, MIN_SPECULAR, MAX_SPECULAR, "%.3f");

		/////////////////////////////////////////////////////TODO
		// Do your GUI code here....
		// Light Parameters
		//ImGui::DragFloat("Float",&f1,0.005f);

		// Camera Movements
		
		if(ImGui::Button("Zoom", ImVec2(50, 50)))zoombutton = !zoombutton;
			
		if(ImGui::Button("FOV", ImVec2(50, 50)))FOVbutton = !FOVbutton;
		if(zoombutton)ImGui::Text("Current Camera Effect: Zoom");
		if(FOVbutton)ImGui::Text("Current Camera Effect: FOV");
		if(zoombutton && FOVbutton)ImGui::Text("Current Camera Effect: Dolly");
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