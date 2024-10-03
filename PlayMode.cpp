#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "TextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint phonebank_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > phonebank_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("waddle.pnct"));
	phonebank_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > phonebank_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("waddle.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = phonebank_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = phonebank_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > game5_music_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("game5.opus"));
});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > phonebank_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("waddle.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

PlayMode::PlayMode() : scene(*phonebank_scene) {
	for (auto &transform : scene.transforms) {
		if (transform.name == "Raccoon") raccoon = &transform;
		else if (transform.name == "Duck") duck = &transform;
		else if (transform.name == "Swan.012") swan = &transform;
	}

	if (raccoon == nullptr) throw std::runtime_error("Raccoon not found.");
	else if (duck == nullptr) throw std::runtime_error("Duck not found.");
	else if (swan == nullptr) throw std::runtime_error("Swan not found.");

	raccoon_rotation = raccoon->rotation;
	duck_rotation = duck->rotation;

	obj_bbox = glm::vec2(0.5f);

	swan_bbox = glm::vec2(10.f);

	Text text1(newline + "You are a Raccoon                                                                                     Press enter to Start",
				script_line_length,
				script_line_height
				);

	texts.push_back(text1);

	{ //set up vertex array and buffers:
		glGenVertexArrays(1, &tex_example.vao);
		glGenBuffers(1, &tex_example.vbo);

		glBindVertexArray(tex_example.vao);
		glBindBuffer(GL_ARRAY_BUFFER, tex_example.vbo);

		glEnableVertexAttribArray( texture_program->Position_vec4 );
		glVertexAttribPointer( texture_program->Position_vec4, 3, GL_FLOAT, GL_FALSE, sizeof(PosTexVertex), (GLbyte *)0 + offsetof(PosTexVertex, Position) );

		glEnableVertexAttribArray( texture_program->TexCoord_vec2 );
		glVertexAttribPointer( texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE, sizeof(PosTexVertex), (GLbyte *)0 + offsetof(PosTexVertex, TexCoord) );

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	GL_ERRORS();

	{ //---------- texture example generate some vertices ----------
		std::vector< PosTexVertex > verts;

		verts.emplace_back(PosTexVertex{
			.Position = glm::vec3(-0.5f, -0.8f, 0.0f),
			.TexCoord = glm::vec2(0.0f, 0.0f),
		});
		verts.emplace_back(PosTexVertex{
			.Position = glm::vec3(-0.5f, 0.7f, 0.0f),
			.TexCoord = glm::vec2(0.0f, 1.0f),
		});
		verts.emplace_back(PosTexVertex{
			.Position = glm::vec3(1.5f, -0.8f, 0.0f),
			.TexCoord = glm::vec2(1.0f, 0.0f),
		});
		verts.emplace_back(PosTexVertex{
			.Position = glm::vec3(1.5f, 0.7f, 0.0f),
			.TexCoord = glm::vec2(1.0f, 1.0f),
		});

		glBindBuffer(GL_ARRAY_BUFFER, tex_example.vbo);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(verts[0]), verts.data(), GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		tex_example.count = verts.size();

		GL_ERRORS();

		//identity transform (just drawing "on the screen"):
		// slight scaling to make text smaller
		tex_example.CLIP_FROM_LOCAL = glm::mat4(
			0.8f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.05f, 0.0f, 0.0f, 1.0f
		);
	}

	TextFont = std::make_shared<Font> (data_path("Sixtyfour_Convergence/SixtyfourConvergence-Regular.ttf"), font_size, font_width, font_height);
	TextFont->gen_texture(tex_example.tex, texts);

	bottomText = "Mouse motion looks; WASD moves; escape ungrabs mouse";

	//create a player transform:
	scene.transforms.emplace_back();
	player.transform = &scene.transforms.back();

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;
	player.camera->transform->parent = player.transform;

	//player's eyes are 1.8 units above the ground:
	player.camera->transform->position = glm::vec3(0.0f, 0.0f, 0.8f);

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//start player walking at nearest walk point:
	player.at = walkmesh->nearest_walk_point(player.transform->position);

	music_loop = Sound::loop_3D(*game5_music_sample, 1.0f, player.camera->transform->position, 10.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_f) {
			cont.downs += 1;
			cont.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_1) {
			one.downs += 1;
			one.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_2) {
			two.downs += 1;
			two.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_3) {
			three.downs += 1;
			three.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_f) {
			cont.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_1) {
			one.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_2) {
			two.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_3) {
			three.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			glm::vec3 upDir = walkmesh->to_world_smooth_normal(player.at);
			player.transform->rotation = glm::angleAxis(-motion.x * player.camera->fovy, upDir) * player.transform->rotation;

			float pitch = glm::pitch(player.camera->transform->rotation);
			pitch += motion.y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	// wobble raccoon
	{
		//slowly rotates through [0,1):
		wobble += elapsed / 10.0f;
		wobble -= std::floor(wobble);

		raccoon->rotation = raccoon_rotation * glm::angleAxis(
			glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
		duck->rotation = duck_rotation * glm::angleAxis(
			glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
	}
	
	//player walking:
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 3.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		//get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);

		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);

		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		}

		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
		*/
	}

	tex_example.CLIP_FROM_LOCAL = player.camera->make_projection() * glm::mat4(player.camera->transform->make_world_to_local()) * glm::mat4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		5.0f, 0.0f, 0.0f, 1.0f
	);

	{
		// check if player is near duck/raccoon
		// https://developer.mozilla.org/en-US/docs/Games/Techniques/3D_collision_detection
		// barely a bbox, more like a bsquare

		float mminX = player.transform->position.x - obj_bbox.x;
		float mmaxX = player.transform->position.x + obj_bbox.x;
		float mminY = player.transform->position.y - obj_bbox.y;
		float mmaxY = player.transform->position.y + obj_bbox.y;
		
		float rminX = raccoon->position.x - obj_bbox.x;
		float rmaxX = raccoon->position.x + obj_bbox.x;
		float rminY = raccoon->position.y - obj_bbox.y;
		float rmaxY = raccoon->position.y + obj_bbox.y;

		float dminX = duck->position.x - obj_bbox.x;
		float dmaxX = duck->position.x + obj_bbox.x;
		float dminY = duck->position.y - obj_bbox.y;
		float dmaxY = duck->position.y + obj_bbox.y;

		float sminX = swan->position.x - swan_bbox.x;
		float smaxX = swan->position.x + swan_bbox.x;
		float sminY = swan->position.y - swan_bbox.y;
		float smaxY = swan->position.y + swan_bbox.y;

		bool raccoonCollide = (mminX <= rmaxX && mmaxX >= rminX && mminY <= rmaxY && mmaxY >= rminY);
		bool duckCollide = (mminX <= dmaxX && mmaxX >= dminX && mminY <= dmaxY && mmaxY >= dminY);
		bool swanCollide = (mminX <= smaxX && mmaxX >= sminX && mminY <= smaxY && mmaxY >= sminY);
		
		if(swanCollide){
			if(cont.pressed) trigger = true;
			if(trigger){
				bottomText = "Counting mushrooms huh...NERD. HONK HONK HONK HAHAHAHAHAHAHAHAHA";
				swantalk = true;
			} else {
				bottomText = "LOL what a scrub HA! Whatcha doin? [F]";
			}
		} else if(raccoonCollide) {
			if(cont.pressed) trigger = true;
			if(trigger){
				if(swantalk){
					if(!one.pressed && two.pressed && !three.pressed) ranswer = true;
					else if (one.pressed || three.pressed) wrongAnswer = true;
				} else if(one.pressed || two.pressed || three.pressed) haventmoved = true;
				if (haventmoved) bottomText = "Look. I get that you're lazy. Me too. but please.";
				else if (wrongAnswer) bottomText = "HUHHHH??? No way that's right...";
				else bottomText = "Well? [1] 5 [2] 7 [3] 9";
			} else {
				bottomText = "Tell me how many brown mushrooms are out there... [F]"; // 7
			}
			if(ranswer) bottomText = "NIIIIIIIIIICE.";
		} else if (duckCollide) {
			if(cont.pressed) trigger = true;
			if(trigger){
				if(swantalk){
					if(one.pressed && !two.pressed && !three.pressed) danswer = true;
					else if (two.pressed || three.pressed) wrongAnswer = true;
				} else if(one.pressed || two.pressed || three.pressed) haventmoved = true;
				if (haventmoved) bottomText = "You didn't even look around...";
				else if (wrongAnswer) bottomText = "That doesn't sound right...";
				else bottomText = "Well? [1] 9 [2] 4 [3] 8";
			} else {
				bottomText = "Tell me how many red mushrooms are out there... [F]"; // 9
			}
			if(danswer) bottomText = "Oh geez. Invasive species amirite?";
		} else {
			trigger = false;
			wrongAnswer = false;
			haventmoved = false;
			bottomText = "Mouse motion looks; WASD moves; escape ungrabs mouse";
		}
	}

	
	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	cont.downs = 0;
	one.downs = 0;
	two.downs = 0;
	three.downs = 0;
	
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*player.camera);

	/* In case you are wondering if your walkmesh is lining up with your scene, try:
	{
		glDisable(GL_DEPTH_TEST);
		DrawLines lines(player.camera->make_projection() * glm::mat4(player.camera->transform->make_world_to_local()));
		for (auto const &tri : walkmesh->triangles) {
			lines.draw(walkmesh->vertices[tri.x], walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.y], walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.z], walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		}
	}
	*/

	{ //texture example drawing
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(texture_program->program);
		glBindVertexArray(tex_example.vao);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex_example.tex);

		glUniformMatrix4fv( texture_program->CLIP_FROM_LOCAL_mat4, 1, GL_FALSE, glm::value_ptr(tex_example.CLIP_FROM_LOCAL) );

		glDrawArrays(GL_TRIANGLE_STRIP, 0, tex_example.count);

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);

		glDisable(GL_BLEND);
		
	}
	GL_ERRORS();

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text(bottomText,
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(bottomText,
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0xff, 0x00));
	}
	GL_ERRORS();
}
