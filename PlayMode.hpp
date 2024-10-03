#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "Font.hpp"
#include "WalkMesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

// ------- from Jim's notes -------
struct PosTexVertex {
    glm::vec3 Position;
    glm::vec2 TexCoord;
};
static_assert( sizeof(PosTexVertex) == 3*4 + 2*4, "PosTexVertex is packed." );
struct {
    GLuint tex = 0; //created at startup
    GLuint vbo = 0; //vertex buffer (of PosTexVertex)
    GLuint vao = 0; //vertex array object

    GLuint count = 0; //number of vertices in buffer
    glm::mat4 CLIP_FROM_LOCAL = glm::mat4(1.0f); //transform to use when drawing
} tex_example;
// -------------------------------


struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	
	std::shared_ptr< Sound::PlayingSample > music_loop;

	Scene::Transform *raccoon = nullptr;
	Scene::Transform *duck = nullptr;
	glm::quat raccoon_rotation;
	glm::quat duck_rotation;
	float wobble = 0.0f;
	

	std::shared_ptr< Font > TextFont;
	std::vector<Text> texts;

	std::string newline= "                                                                                ";


	unsigned int const script_line_length = 50;
	unsigned int const script_line_height = 50;
	unsigned int const font_size = 48;
	unsigned int const font_width = 1480;
	unsigned int const font_height = 800;

	std::string bottomText;

	glm::vec2 obj_bbox;

	//player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		//camera is at player's head and will be pitched by mouse up/down motion:
		Scene::Camera *camera = nullptr;
	} player;
};
