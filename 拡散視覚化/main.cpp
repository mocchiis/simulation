#include <cstdio>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "common.h"
#include "diff_equation.h"

static int WIN_WIDTH = 500;                       // ウィンドウの幅
static int WIN_HEIGHT = 500;                       // ウィンドウの高さ
static const char *WIN_TITLE = "diff";     // ウィンドウのタイトル

static const std::string TEX_FILE = std::string(DATA_DIRECTORY) + "hue.png";

static const std::string VERT_SHADER_FILE = std::string(SHADER_DIRECTORY) + "render.vert";
static const std::string FRAG_SHADER_FILE = std::string(SHADER_DIRECTORY) + "render.frag";

// VAO関連の変数
GLuint vaoId;
GLuint vboId;
GLuint iboId;

// シェーダを参照する番号
GLuint vertShaderId;
GLuint fragShaderId;
GLuint programId;

// テクスチャ
GLuint textureId;

// 拡散方程式の計算に使うパラメータ
DiffEquation diffEqn;
static int texWidth = 1000;//分割の本数
static int texHeight = 1000;
static const double dx = 0.01;
float heatInit = 2.0;//初期中心温度
int radiusInit = 15;//初期中心温度半径
float diff_num = 0.25;

float clip_near = 0.1f;
float clip_far = 10.0f;

// 頂点のデータ
std::vector<glm::vec3> positions;

// Arcballコントロールのための変数
bool isDragging = false;

enum ArcballMode {
	ARCBALL_MODE_NONE = 0x00,
	ARCBALL_MODE_TRANSLATE = 0x01,
	ARCBALL_MODE_ROTATE = 0x02,
	ARCBALL_MODE_SCALE = 0x04
};

int arcballMode = ARCBALL_MODE_NONE;

glm::ivec2 oldPos;
glm::ivec2 newPos;

glm::mat4 modelMat, viewMat, projMat;
glm::mat4 acRotMat;

glm::mat4 acScaleMat;
float acScale = 1.0f;

void initVAO() {
	// 頂点データの初期化
	for (int i = 0; i < texHeight; i++) {
		for (int j = 0; j < texWidth; j++) {
			double vx = (i - texWidth / 2) * dx;
			double vy = (j - texHeight / 2)* dx;
			positions.push_back(glm::vec3(vx, vy, 0.0));
			//diffEqn.set(i, j, 2.0 * exp(-6.0 * ((vx-3) * (vx-3)+vy * vy)));//setの3引数は高さ
		}
	}

	diffEqn.start();

	// VAOの用意
	glGenVertexArrays(1, &vaoId);
	glBindVertexArray(vaoId);

	glGenBuffers(1, &vboId);
	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * positions.size(),
		&positions[0], GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);

	std::vector<unsigned int> indices;
	for (int y = 0; y < texHeight - 1; y++) {
		for (int x = 0; x < texWidth - 1; x++) {
			const int i0 = y * texWidth + x;
			const int i1 = y * texWidth + (x + 1);
			const int i2 = (y + 1) * texWidth + x;
			const int i3 = (y + 1) * texWidth + (x + 1);

			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i3);
			indices.push_back(i0);
			indices.push_back(i3);
			indices.push_back(i2);
		}
	}

	glGenBuffers(1, &iboId);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboId);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		sizeof(unsigned int) * indices.size(),
		&indices[0], GL_STATIC_DRAW);

	glBindVertexArray(0);

}

//myGLSLに実装
GLuint compileShader(const std::string &filename, GLuint type) {
	// シェーダの作成
	GLuint shaderId = glCreateShader(type);

	// ファイルの読み込み
	std::ifstream reader;
	size_t codeSize;
	std::string code;

	// ファイルを開く
	reader.open(filename.c_str(), std::ios::in);
	if (!reader.is_open()) {
		// ファイルを開けなかったらエラーを出して終了
		fprintf(stderr, "Failed to load a shader: %s\n", VERT_SHADER_FILE.c_str());
		exit(1);
	}

	// ファイルをすべて読んで変数に格納 (やや難)
	reader.seekg(0, std::ios::end);             // ファイル読み取り位置を終端に移動 
	codeSize = reader.tellg();                  // 現在の箇所(=終端)の位置がファイルサイズ
	code.resize(codeSize);                      // コードを格納する変数の大きさを設定
	reader.seekg(0);                            // ファイルの読み取り位置を先頭に移動
	reader.read(&code[0], codeSize);            // 先頭からファイルサイズ分を読んでコードの変数に格納

	// ファイルを閉じる
	reader.close();

	// コードのコンパイル
	const char *codeChars = code.c_str();
	glShaderSource(shaderId, 1, &codeChars, NULL);
	glCompileShader(shaderId);

	// コンパイルの成否を判定する
	GLint compileStatus;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus == GL_FALSE) {
		// コンパイルが失敗したらエラーメッセージとソースコードを表示して終了
		fprintf(stderr, "Failed to compile a shader!\n");

		// エラーメッセージの長さを取得する
		GLint logLength;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			// エラーメッセージを取得する
			GLsizei length;
			std::string errMsg;
			errMsg.resize(logLength);
			glGetShaderInfoLog(shaderId, logLength, &length, &errMsg[0]);

			// エラーメッセージとソースコードの出力
			fprintf(stderr, "[ ERROR ] %s\n", errMsg.c_str());
			fprintf(stderr, "%s\n", code.c_str());
		}
		exit(1);
	}

	return shaderId;
}

//myGLSLの中に実装
GLuint buildShaderProgram(const std::string &vShaderFile, const std::string &fShaderFile) {
	// シェーダの作成
	GLuint vertShaderId = compileShader(vShaderFile, GL_VERTEX_SHADER);
	GLuint fragShaderId = compileShader(fShaderFile, GL_FRAGMENT_SHADER);

	// シェーダプログラムのリンク
	GLuint programId = glCreateProgram();
	glAttachShader(programId, vertShaderId);
	glAttachShader(programId, fragShaderId);
	glLinkProgram(programId);

	// リンクの成否を判定する
	GLint linkState;
	glGetProgramiv(programId, GL_LINK_STATUS, &linkState);
	if (linkState == GL_FALSE) {
		// リンクに失敗したらエラーメッセージを表示して終了
		fprintf(stderr, "Failed to link shaders!\n");

		// エラーメッセージの長さを取得する
		GLint logLength;
		glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			// エラーメッセージを取得する
			GLsizei length;
			std::string errMsg;
			errMsg.resize(logLength);
			glGetProgramInfoLog(programId, logLength, &length, &errMsg[0]);

			// エラーメッセージを出力する
			fprintf(stderr, "[ ERROR ] %s\n", errMsg.c_str());
		}
		exit(1);
	}

	// シェーダを無効化した後にIDを返す
	glUseProgram(0);
	return programId;
}

// シェーダの初期化
void initShaders() {
	programId = buildShaderProgram(VERT_SHADER_FILE, FRAG_SHADER_FILE);//shaders1
}


// OpenGLの初期化関数
void initializeGL() {
	// 背景色の設定 (黒)
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// 深度テストの有効化
	glEnable(GL_DEPTH_TEST);

	// 拡散方程式シミュレーションの初期化
	diffEqn.initParams(texWidth, texHeight, diff_num);

	// VAOの初期化
	initVAO();

	// シェーダの用意
	initShaders();

	// テクスチャの用意
	int texWidth, texHeight, channels;
	unsigned char *bytes = stbi_load(TEX_FILE.c_str(), &texWidth, &texHeight, &channels, STBI_rgb_alpha);
	if (!bytes) {
		fprintf(stderr, "Failed to load image file: %s\n", TEX_FILE.c_str());
		exit(1);
	}

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_1D, textureId);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, texWidth,
		0, GL_RGBA, GL_UNSIGNED_BYTE, bytes);

	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_1D, 0);

	stbi_image_free(bytes);

	modelMat = glm::mat4(1.0);
	acRotMat = glm::mat4(1.0);
	acScaleMat = glm::mat4(1.0);

	modelMat = glm::translate(modelMat, glm::vec3(0.0f, 0.0f, 1.0f));

	// 座標の変換
	projMat = glm::perspective(45.0f,
		(float)WIN_WIDTH / (float)WIN_HEIGHT, clip_near, clip_far);

	viewMat = glm::lookAt(glm::vec3(0.0f, 0.0f, 8.0f),   // 視点の位置
		glm::vec3(0.0f, 0.0f, 0.0f),   // 見ている先
		glm::vec3(1.0f, 0.0f, 0.0f));  // 視界の上方向

}

// OpenGLの描画関数
void paintGL() {
	// 背景色と深度値のクリア
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 mvpMat = projMat * viewMat *modelMat * acRotMat;

	// VAOの有効化
	glBindVertexArray(vaoId);

	// シェーダの有効化
	glUseProgram(programId);

	// テクスチャの有効化
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, textureId);

	// Uniform変数の転送
	GLuint uid;
	uid = glGetUniformLocation(programId, "u_mvpMat");
	glUniformMatrix4fv(uid, 1, GL_FALSE, glm::value_ptr(mvpMat));

	uid = glGetUniformLocation(programId, "u_texture");
	glUniform1i(uid, 0);

	// 三角形の描画
	glDrawElements(GL_TRIANGLES, 3 * (texHeight - 1) * (texWidth - 1) * 2, GL_UNSIGNED_INT, 0);

	// VAOの無効化
	glBindVertexArray(0);

	// シェーダの無効化
	glUseProgram(0);

	// テクスチャの無効化
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, 0);
}

void resizeGL(GLFWwindow *window, int width, int height) {

	// ユーザ管理のウィンドウサイズを変更	
	if (WIN_WIDTH != width) {
		WIN_WIDTH = width;
		WIN_HEIGHT = WIN_WIDTH;
	}
	else if (WIN_HEIGHT != height) {
		WIN_HEIGHT = height;
		WIN_WIDTH = WIN_HEIGHT;
	}

	// GLFW管理のウィンドウサイズを変更
	glfwSetWindowSize(window, WIN_WIDTH, WIN_HEIGHT);

	// 実際のウィンドウサイズ (ピクセル数) を取得
	int renderBufferWidth, renderBufferHeight;
	glfwGetFramebufferSize(window, &renderBufferWidth, &renderBufferHeight);

	// ビューポート変換の更新
	glViewport(0, 0, renderBufferWidth, renderBufferHeight);
}

// アニメーションのためのアップデート
void animate() {

	// 波動データの更新
	diffEqn.step();

	for (int y = 0; y < texHeight; y++) {
		for (int x = 0; x < texWidth; x++) {
			positions[y * texWidth + x].z = diffEqn.get(x, y);
		}
	}

	glBindVertexArray(vaoId);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * positions.size(), &positions[0]);
	glBindVertexArray(0);
}

bool Press = false;
const char *Press_button;

void mouseEvent(GLFWwindow *window, int button, int action, int mods) {
	// クリックされた位置を取得
	double px, py;
	glfwGetCursorPos(window, &px, &py);

	if (action == GLFW_PRESS) {
		const int cx = (WIN_WIDTH - (int)px - 1);
		const int cy = (WIN_HEIGHT - (int)py - 1);

		int x = std::round((cx - 0.5) * texWidth / WIN_WIDTH);
		int y = std::round((cy - 0.5) * texHeight / WIN_HEIGHT);

		// クリックしたボタンで処理を切り替える
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			Press_button = "LEFT";

			//ニアクリッピング面でのy座標最大値
			float y_max = 0.1f * tan(45.0f / 2);
			//ニアクリッピング面でのx座標最大値
			float x_max = ((float)WIN_WIDTH / (float)WIN_HEIGHT) * 0.1f * tan(45.0f / 2);

			//中心を原点とする座標
			float ey = -(py - ((float)WIN_HEIGHT / 2));
			float ex = px - ((float)WIN_WIDTH / 2);

			//ニアクリッピング面の座標
			float y_near = (2 / (float)WIN_HEIGHT) * y_max * ey;
			float x_near = (2 / (float)WIN_WIDTH) * x_max * ex;

			//ファークリッピング面の座標
			float x_far = (clip_far / clip_near) * y_near;
			float y_far = (clip_far / clip_near) * x_near;

			//カメラ座標
			glm::mat4 M = acRotMat * modelMat; //*modelMat

			//クリッピング面のベクトル
			glm::vec4 A = glm::vec4(x_near, y_near, clip_near, 1.0f);
			glm::vec4 B = glm::vec4(x_far, y_far, clip_far, 1.0f);

			//座標変換後のベクトル
			glm::vec4 A_t = M * A;
			glm::vec4 B_t = M * B;

			//ニアからファーまでのベクトル（直線の傾き）
			glm::vec4 AB_t = B_t - A_t;

			//変換後の座標
			float x_t = A_t.x + (0 - A_t.z) * (AB_t.x / AB_t.z);
			float y_t = A_t.y + (0 - A_t.z) * (AB_t.y / AB_t.z);

			const int wx = (WIN_WIDTH - (int)px - 1);
			const int wy = (WIN_HEIGHT - (int)py - 1);

			//ピクセルに変換
			int gx = std::round(((int)wx - 0.5) * (texWidth ) / (2 * A_t.x));
			int gy = std::round(((int)wy - 0.5) * (texHeight ) / (2 * A_t.y));

			for (int i = -radiusInit; i <= radiusInit; i++) {
				for (int j = -radiusInit; j <= radiusInit; j++) {

					if (x + i < 0 || y + j < 0) {
						continue;
					}

					//円柱
					float r = sqrt(i * i + j * j);

					//初期中心温度（濃度）半径内のとき
					if (r < radiusInit) diffEqn.set(x + i, y + j, heatInit);//物理量

				}

			}
			std::cout << "Mouse position:" << gx << " , " << gy << std::endl;
		}
		else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			arcballMode = ARCBALL_MODE_ROTATE;
			Press_button = "RIGHT";
		}

		std::cout << "Press button: " << Press_button << std::endl;

		if (!isDragging) {
			isDragging = true;
			oldPos = glm::ivec2(px, py);
			newPos = glm::ivec2(px, py);
		}
	}
	else {
		isDragging = false;
		oldPos = glm::ivec2(0, 0);
		newPos = glm::ivec2(0, 0);
		arcballMode = ARCBALL_MODE_NONE;
	}
}


// スクリーン上の位置をアークボール球上の位置に変換する関数
glm::vec3 getVector(double x, double y) {
	glm::vec3 pt(2.0 * x / WIN_WIDTH - 1.0,
		-2.0 * y / WIN_HEIGHT + 1.0, 0.0);

	const double xySquared = pt.x * pt.x + pt.y * pt.y;

	if (xySquared <= 1.0) {

		// 単位円の内側ならz座標を計算

		pt.z = std::sqrt(1.0 - xySquared);

	}
	else {

		// 外側なら球の外枠上にあると考える
		pt = glm::normalize(pt);
	}
	return pt;
}


//回転角と回転軸の計算
void updateRotate() {
	static const double Pi = 4.0 * std::atan(1.0);

	// スクリーン座標をアークボール球上の座標に変換
	const glm::vec3 u = glm::normalize(getVector(newPos.x, newPos.y));
	const glm::vec3 v = glm::normalize(getVector(oldPos.x, oldPos.y));

	// カメラ座標における回転量 (=オブジェクト座標における回転量)
	const double angle = std::acos(std::max(-1.0f, std::min(glm::dot(u, v), 1.0f)));

	// カメラ空間における回転軸
	const glm::vec3 rotAxis = glm::cross(v, u);

	// カメラ座標の情報をワールド座標に変換する行列
	const glm::mat4 c2oMat = glm::inverse(viewMat * modelMat);

	// オブジェクト座標における回転軸
	const glm::vec3 rotAxisObjSpace = glm::vec3(c2oMat * glm::vec4(rotAxis, 0.0f));

	// 回転行列の更新
	acRotMat = glm::rotate((float)(4.0 * angle), rotAxisObjSpace) * acRotMat;
}


void updateScale() {
	acScaleMat = glm::scale(glm::vec3(acScale, acScale, acScale));
}

void updateMouse() {
	switch (arcballMode) {
	case ARCBALL_MODE_ROTATE:
		updateRotate();
		break;

	case ARCBALL_MODE_TRANSLATE:
		//updateTranslate();
		break;

	case ARCBALL_MODE_SCALE:
		acScale += (float)(oldPos.y - newPos.y) / WIN_HEIGHT;
		updateScale();
		break;
	}
}

void mouseMoveEvent(GLFWwindow *window, double xpos, double ypos) {
	if (isDragging) {
		// マウスの現在位置を更新
		newPos = glm::ivec2(xpos, ypos);

		// マウスがあまり動いていない時は処理をしない
		const double dx = newPos.x - oldPos.x;
		const double dy = newPos.y - oldPos.y;
		const double length = dx * dx + dy * dy;
		if (length < 2.0f * 2.0f) {
			return;
		}
		else {
			updateMouse();
			oldPos = glm::ivec2(xpos, ypos);
		}
	}
}

void wheelEvent(GLFWwindow *window, double xpos, double ypos) {
	acScale += ypos / 10.0;
	updateScale();
}

int main(int argc, char **argv) {
	// OpenGLを初期化する
	if (glfwInit() == GL_FALSE) {
		fprintf(stderr, "Initialization failed!\n");
		return 1;
	}

	// OpenGLのバージョン指定
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Windowの作成
	GLFWwindow *window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, WIN_TITLE,
		NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Window creation failed!");
		glfwTerminate();
		return 1;
	}

	// OpenGLの描画対象にWindowを追加
	glfwMakeContextCurrent(window);

	// マウスのイベントを処理する関数を登録
	glfwSetMouseButtonCallback(window, mouseEvent);
	glfwSetCursorPosCallback(window, mouseMoveEvent);
	glfwSetScrollCallback(window, wheelEvent);

	// OpenGL 3.x/4.xの関数をロードする (glfwMakeContextCurrentの後でないといけない)
	const int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0) {
		fprintf(stderr, "Failed to load OpenGL 3.x/4.x libraries!\n");
		return 1;
	}

	// バージョンを出力する
	printf("Load OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

	// ウィンドウのリサイズを扱う関数の登録
	glfwSetWindowSizeCallback(window, resizeGL);

	// OpenGLを初期化
	initializeGL();

	// メインループ
	while (glfwWindowShouldClose(window) == GL_FALSE) {
		// 描画
		paintGL();

		// アニメーション
		animate();

		// 描画用バッファの切り替え
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}
