//Header file
#include "frogger_game.h"

//Home made imports
#include "debug.h"
#include "ecs.h"
#include "fs.h"
#include "gpu.h"
#include "heap.h"
#include "net.h"
#include "render.h"
#include "timer_object.h"
#include "transform.h"
#include "wm.h"

//C Lib imports
#include <string.h>
#include<stdio.h>
#include <time.h>

#define _USE_MATH_DEFINES
#include <math.h>

typedef struct transform_component_t
{
	transform_t transform;
} transform_component_t;

typedef struct camera_component_t
{
	mat4f_t projection;
	mat4f_t view;
} camera_component_t;

typedef struct model_component_t
{
	gpu_mesh_info_t* mesh_info;
	gpu_shader_info_t* shader_info;
} model_component_t;

typedef struct player_component_t
{
	int index;
} player_component_t;

typedef struct enemy_component_t
{
	int index;
} enemy_component_t;

typedef struct name_component_t
{
	char name[32];
} name_component_t;

typedef struct frogger_game_t
{
	heap_t* heap;
	fs_t* fs;
	wm_window_t* window;
	render_t* render;
	net_t* net;

	timer_object_t* timer;

	ecs_t* ecs;
	int transform_type;
	int camera_type;
	int model_type;
	int player_type;
	int enemy_type;
	int name_type;
	ecs_entity_ref_t player_ent;
	ecs_entity_ref_t camera_ent;
	ecs_entity_ref_t enemy_ent;

	gpu_mesh_info_t cube_mesh;
	
	//Extra meshes for different colored enemies
	gpu_mesh_info_t cube_mesh_A;
	gpu_mesh_info_t cube_mesh_B;
	gpu_mesh_info_t cube_mesh_C;

	gpu_shader_info_t cube_shader;
	fs_work_t* vertex_shader_work;
	fs_work_t* fragment_shader_work;
} frogger_game_t;

static void load_resources(frogger_game_t* game);
static void unload_resources(frogger_game_t* game);
static void spawn_player(frogger_game_t* game, int index);
static void spawn_enemy(frogger_game_t* game, int index, int zone, int letter);
static void spawnEnemies(frogger_game_t* game, int start);
static void spawn_camera(frogger_game_t* game);
static void update_players(frogger_game_t* game);
static void update_enemies(frogger_game_t* game, int dir);
static void check_collision(frogger_game_t* game);
static void draw_models(frogger_game_t* game);

//Makes the frogger game
frogger_game_t* frogger_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render, int argc, const char** argv)
{
	frogger_game_t* game = heap_alloc(heap, sizeof(frogger_game_t), 8);
	game->heap = heap;
	game->fs = fs;
	game->window = window;
	game->render = render;
	game->timer = timer_object_create(heap, NULL);
	
	game->ecs = ecs_create(heap);
	game->transform_type = ecs_register_component_type(game->ecs, "transform", sizeof(transform_component_t), _Alignof(transform_component_t));
	game->camera_type = ecs_register_component_type(game->ecs, "camera", sizeof(camera_component_t), _Alignof(camera_component_t));
	game->model_type = ecs_register_component_type(game->ecs, "model", sizeof(model_component_t), _Alignof(model_component_t));
	game->player_type = ecs_register_component_type(game->ecs, "player", sizeof(player_component_t), _Alignof(player_component_t));
	game->enemy_type = ecs_register_component_type(game->ecs, "enemy", sizeof(enemy_component_t), _Alignof(enemy_component_t));
	game->name_type = ecs_register_component_type(game->ecs, "name", sizeof(name_component_t), _Alignof(name_component_t));

	load_resources(game);

	spawn_player(game, 0);

	spawnEnemies(game, 0);

	spawn_camera(game);

	return game;
}

//Cleans up after frogger game is done
void frogger_game_destroy(frogger_game_t* game)
{
	//net_destroy(game->net);
	ecs_destroy(game->ecs);
	timer_object_destroy(game->timer);
	unload_resources(game);
	heap_free(game->heap, game);
}

//Updates frame and current progress of frogger game
void frogger_game_update(frogger_game_t* game)
{
	timer_object_update(game->timer);
	ecs_update(game->ecs);
	//net_update(game->net);
	update_players(game);
	update_enemies(game, 0);
	check_collision(game);
	draw_models(game);
	render_push_done(game->render);
}

//Resources necessary for game
static void load_resources(frogger_game_t* game)
{
	game->vertex_shader_work = fs_read(game->fs, "shaders/triangle.vert.spv", game->heap, false, false);
	game->fragment_shader_work = fs_read(game->fs, "shaders/triangle.frag.spv", game->heap, false, false);
	game->cube_shader = (gpu_shader_info_t)
	{
		.vertex_shader_data = fs_work_get_buffer(game->vertex_shader_work),
		.vertex_shader_size = fs_work_get_size(game->vertex_shader_work),
		.fragment_shader_data = fs_work_get_buffer(game->fragment_shader_work),
		.fragment_shader_size = fs_work_get_size(game->fragment_shader_work),
		.uniform_buffer_count = 1,
	};

	static vec3f_t cube_verts_player[] =
	{
		{ -0.5f, -0.5f,  0.5f }, { 0.0f, 0.8f,  0.0f },
		{  0.5f, -0.5f,  0.5f }, { 0.0f, 0.8f,  0.0f },
		{  0.5f,  0.5f,  0.5f }, { 0.0f, 0.8f,  0.0f },
		{ -0.5f,  0.5f,  0.5f }, { 0.0f, 0.8f,  0.0f },
		{ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.8f,  0.0f },
		{  0.5f, -0.5f, -0.5f }, { 0.0f, 0.8f,  0.0f },
		{  0.5f,  0.5f, -0.5f }, { 0.0f, 0.8f,  0.0f },
		{ -0.5f,  0.5f, -0.5f }, { 0.0f, 0.8f,  0.0f },
	};

	static vec3f_t cube_verts_A[] =
	{
		{ -0.5f, -0.5f,  0.5f }, { 0.8f, 0.0f,  0.0f },
		{  0.5f, -0.5f,  0.5f }, { 0.8f, 0.0f,  0.0f },
		{  0.5f,  0.5f,  0.5f }, { 0.8f, 0.0f,  0.0f },
		{ -0.5f,  0.5f,  0.5f }, { 0.8f, 0.0f,  0.0f },
		{ -0.5f, -0.5f, -0.5f }, { 0.8f, 0.0f,  0.0f },
		{  0.5f, -0.5f, -0.5f }, { 0.8f, 0.0f,  0.0f },
		{  0.5f,  0.5f, -0.5f }, { 0.8f, 0.0f,  0.0f },
		{ -0.5f,  0.5f, -0.5f }, { 0.8f, 0.0f,  0.0f },
	};

	static vec3f_t cube_verts_B[] =
	{
		{ -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f,  0.8f },
		{  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f,  0.8f },
		{  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f,  0.8f },
		{ -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f,  0.8f },
		{ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f,  0.8f },
		{  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f,  0.8f },
		{  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f,  0.8f },
		{ -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f,  0.8f },
	};

	static vec3f_t cube_verts_C[] =
	{
		{ -0.5f, -0.5f,  0.5f }, { 0.8f, 0.0f,  0.8f },
		{  0.5f, -0.5f,  0.5f }, { 0.0f, 0.8f,  0.8f },
		{  0.5f,  0.5f,  0.5f }, { 0.8f, 0.8f,  0.0f },
		{ -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f,  0.0f },
		{ -0.5f, -0.5f, -0.5f }, { 0.8f, 0.8f,  0.8f },
		{  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f,  0.8f },
		{  0.5f,  0.5f, -0.5f }, { 0.8f, 0.0f,  0.0f },
		{ -0.5f,  0.5f, -0.5f }, { 0.8f, 0.8f,  0.8f },
	};

	static uint16_t cube_indices[] =
	{
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};
	game->cube_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts_player,
		.vertex_data_size = sizeof(cube_verts_player),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};

	game->cube_mesh_A = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts_A,
		.vertex_data_size = sizeof(cube_verts_A),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};

	game->cube_mesh_B = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts_B,
		.vertex_data_size = sizeof(cube_verts_B),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};

	game->cube_mesh_C = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts_C,
		.vertex_data_size = sizeof(cube_verts_C),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};
}

//Cleans up resources used
static void unload_resources(frogger_game_t* game)
{
	heap_free(game->heap, fs_work_get_buffer(game->vertex_shader_work));
	heap_free(game->heap, fs_work_get_buffer(game->fragment_shader_work));
	fs_work_destroy(game->fragment_shader_work);
	fs_work_destroy(game->vertex_shader_work);
}

//NOT IMPLEMENTED
static void player_net_configure(ecs_t* ecs, ecs_entity_ref_t entity, int type, void* user)
{
	frogger_game_t* game = user;
	model_component_t* model_comp = ecs_entity_get_component(ecs, entity, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;
}

//Spawns the next batch of enemies
static void spawnEnemies(frogger_game_t* game, int start)
{
	if (start == 0) { srand((int)time(NULL)); } // Initialization, should only be called once.
	int maxEntity = 4;

	int r = (rand() % maxEntity) ;      // Returns a pseudo-random integer between 0 and RAND_MAX.

	//Uses C Mesh
	for (int i = 0; i < r; i++)
	{
		spawn_enemy(game, i, -3, 2);
	}

	r = (rand() % maxEntity) + 1;

	//Uses C Mesh
	for (int j = 0; j < r; j++)
	{
		spawn_enemy(game, j, -0, 2);
	}

	r = (rand() % maxEntity) + 2;

	//Uses C Mesh
	for (int k = 0; k < r; k++)
	{
		spawn_enemy(game, k, 3, 2);
	}
}

//Spawns a player oject
static void spawn_player(frogger_game_t* game, int index)
{
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->player_type) |
		(1ULL << game->name_type);
	game->player_ent = ecs_entity_add(game->ecs, k_player_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	transform_comp->transform.translation.y = (float)index * 1.0f;
	transform_comp->transform.translation.z = 6.0f;

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "player");

	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->index = index;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;
}

//Spawns an enemy object
static void spawn_enemy(frogger_game_t* game, int index, int zone, int letter)
{
	uint64_t k_enemy_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->enemy_type) |
		(1ULL << game->name_type);
	game->enemy_ent = ecs_entity_add(game->ecs, k_enemy_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->enemy_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	transform_comp->transform.translation.y = ((float)index * 1.0f) + 10.0f;
	transform_comp->transform.translation.z = (float)zone * -1.0f;

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->enemy_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "enemy");

	enemy_component_t* enemy_comp = ecs_entity_get_component(game->ecs, game->enemy_ent, game->player_type, true);
	enemy_comp->index = index;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->enemy_ent, game->model_type, true);

	if(letter == 0)
	{
		model_comp->mesh_info = &game->cube_mesh_A;
	}
	if (letter == 1)
	{
		model_comp->mesh_info = &game->cube_mesh_B;
	}
	if (letter == 2)
	{
		model_comp->mesh_info = &game->cube_mesh_C;
	}
	model_comp->shader_info = &game->cube_shader;
}

//Spawns a camera object
static void spawn_camera(frogger_game_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "camera");

	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
	mat4f_make_ortho(&camera_comp->projection, (float)M_PI / 2.0f, 16.0f / 9.0f, 0.1f, 100.0f, -10.0f, 10.0f, 6.5f, -6.5f);

	vec3f_t eye_pos = vec3f_scale(vec3f_forward(), -20.0f);
	vec3f_t forward = vec3f_forward();
	vec3f_t up = vec3f_up();
	mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
}

//Gathers player input to update player models
static void update_players(frogger_game_t* game)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;

	uint32_t key_mask = wm_get_key_mask(game->window);

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->player_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		player_component_t* player_comp = ecs_query_get_component(game->ecs, &query, game->player_type);

		if (transform_comp->transform.translation.z <= -6.0f)
		{
			transform_comp->transform.translation.z = 6.0f;
		}
		if (transform_comp->transform.translation.z > 6.0f)
		{
			transform_comp->transform.translation.z = 6.0f;
		}

		transform_t move;
		transform_identity(&move);
		if (key_mask & k_key_up)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), -dt*2));
		}
		if (key_mask & k_key_down)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), dt * 2));
		}
		transform_multiply(&transform_comp->transform, &move);
	}


}

//Updates all enemy objects
static void update_enemies(frogger_game_t* game, int dir)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;

	uint32_t key_mask = wm_get_key_mask(game->window);

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->enemy_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		enemy_component_t* enemy_comp = ecs_query_get_component(game->ecs, &query, game->enemy_type);

		if (transform_comp->transform.translation.y < -13.0f)
		{
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
		}

		transform_t move;
		transform_identity(&move);

		
		
		if (dir == 0)
		{
			
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -dt * 3.2f));
		}
		if (dir == 1)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), dt * 3.2f));
		}
		transform_multiply(&transform_comp->transform, &move);
	}

	ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
	if (!(ecs_query_is_valid(game->ecs, &query))) 
	{
		spawnEnemies(game,1);
	}
}

//Checks to see if player has collided with an enemy object
static void check_collision(frogger_game_t* game) 
{
	//
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;

	//uint32_t key_mask = wm_get_key_mask(game->window);

	uint64_t k_query_mask_enemy = (1ULL << game->transform_type) | (1ULL << game->enemy_type);

	uint64_t k_query_mask_player = (1ULL << game->transform_type) | (1ULL << game->player_type);

	ecs_query_t queryP = ecs_query_create(game->ecs, k_query_mask_player);

	transform_component_t* transform_comp_player = ecs_query_get_component(game->ecs, &queryP, game->transform_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask_enemy);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		enemy_component_t* enemy_comp = ecs_query_get_component(game->ecs, &query, game->enemy_type);

		float dist = vec3f_dist2(transform_comp->transform.translation, transform_comp_player->transform.translation);//transform_comp->transform.translation

		//printf("THIS IS DIST FROM ENEMIES: %f", dist);

		if (dist <= 0.5f)
		{
			//ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
			transform_comp_player->transform.translation.z = 6.0f;
		}

	}

	//vec3f_dist2(vec3f_t a, vec3f_t b)
}

//Draws every model
static void draw_models(frogger_game_t* game)
{
	uint64_t k_camera_query_mask = (1ULL << game->camera_type);
	for (ecs_query_t camera_query = ecs_query_create(game->ecs, k_camera_query_mask);
		ecs_query_is_valid(game->ecs, &camera_query);
		ecs_query_next(game->ecs, &camera_query))
	{
		camera_component_t* camera_comp = ecs_query_get_component(game->ecs, &camera_query, game->camera_type);

		uint64_t k_model_query_mask = (1ULL << game->transform_type) | (1ULL << game->model_type);
		for (ecs_query_t query = ecs_query_create(game->ecs, k_model_query_mask);
			ecs_query_is_valid(game->ecs, &query);
			ecs_query_next(game->ecs, &query))
		{
			transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
			model_component_t* model_comp = ecs_query_get_component(game->ecs, &query, game->model_type);
			ecs_entity_ref_t entity_ref = ecs_query_get_entity(game->ecs, &query);

			struct
			{
				mat4f_t projection;
				mat4f_t model;
				mat4f_t view;
			} uniform_data;
			uniform_data.projection = camera_comp->projection;
			uniform_data.view = camera_comp->view;
			transform_to_matrix(&transform_comp->transform, &uniform_data.model);
			gpu_uniform_buffer_info_t uniform_info = { .data = &uniform_data, sizeof(uniform_data) };

			render_push_model(game->render, &entity_ref, model_comp->mesh_info, model_comp->shader_info, &uniform_info);
		}
	}
}
