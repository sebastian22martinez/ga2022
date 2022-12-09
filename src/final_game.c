//Header file
#include "final_game.h"

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

//Lua Imports
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"

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


////////////////////////////////////////////
/**** Structs to hold data of enitities from Lua files ****/

//Player Components
typedef struct lPlayer_comp_t
{
	int index;
	transform_t transform;
	char name[32];
	char model[32];
	
} lPlayer_comp_t;

//Enemy Components
typedef struct lEnemy_comp_t
{
	float offset;
	float zone;
	char name[32];
	char model[32];

} lEnemy_comp_t;

//Camera Components
typedef struct lCamera_comp_t
{
	char name[32];
	float angle;
	float aspect;
	float z_near;
	float z_far;
	float left;
	float right;
	float top;
	float bot;
	float eyePos;

} lCamera_comp_t;

////////////////////////////////////////////

typedef struct final_game_t
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

	//Meshes for Player Cube
	gpu_mesh_info_t cube_mesh;
	
	//Extra meshes for different colored enemies
	gpu_mesh_info_t cube_mesh_A;
	gpu_mesh_info_t cube_mesh_B;
	gpu_mesh_info_t cube_mesh_C;

	//Shaders for Cubes
	gpu_shader_info_t cube_shader;
	fs_work_t* vertex_shader_work;
	fs_work_t* fragment_shader_work;

	//Lua Configs
	lPlayer_comp_t playerConfigs;
	lEnemy_comp_t enemyConfigs;
	lCamera_comp_t camConfigs;

} final_game_t;

//Static functions that the game uses
static void load_resources(final_game_t* game);
static void unload_resources(final_game_t* game);
static void spawn_player(final_game_t* game, int index);
static void spawn_enemy(final_game_t* game, int index, int region);
static void spawnEnemies(final_game_t* game, int start);
static void spawn_camera(final_game_t* game);
static void update_players(final_game_t* game);
static void update_enemies(final_game_t* game, int dir);
static void check_collision(final_game_t* game);
static void draw_models(final_game_t* game);

//Static functions to run to extract Lua data that the C program will use for entities
static void playerConfigs(final_game_t* game);
static void enemyConfigs(final_game_t* game);
static void cameraConfigs(final_game_t* game);

//Makes the final frogger game
final_game_t* final_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render, int argc, const char** argv)
{
	final_game_t* game = heap_alloc(heap, sizeof(final_game_t), 8);
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

	//Loads resources for game
	load_resources(game);

	//Sets up game configs from Lua
	playerConfigs(game);
	enemyConfigs(game);
	cameraConfigs(game);

	//Sets the player and enemies into motion
	spawn_player(game, 0);
	spawnEnemies(game, 0);
	spawn_camera(game);

	return game;
}

////////////////////////////////////////////
/**** Start of entity configuration functions that uses Lua data ****/

static void playerConfigs(final_game_t* game) 
{
	//Create New Lua State
	lua_State* L;
	L = luaL_newstate();
	luaL_openlibs(L);
	luaL_dofile(L, "luaGamePlayerComps.lua");

	//Player Components
	lua_getglobal(L, "Player");

	//Transform
	lua_pushstring(L, "Transform");
	lua_gettable(L, -2);

	//x
	lua_pushstring(L, "x");
	lua_gettable(L, -2);
	game->playerConfigs.transform.translation.x = lua_tonumber(L, -1);

	//y
	lua_pop(L, 1);
	lua_pushstring(L, "y");
	lua_gettable(L, -2);
	game->playerConfigs.transform.translation.y = lua_tonumber(L, -1);

	//z
	lua_pop(L, 1);
	lua_pushstring(L, "z");
	lua_gettable(L, -2);
	game->playerConfigs.transform.translation.z = lua_tonumber(L, -1);

	//Enter next Lua Table
	lua_pop(L, 1);

	//Name
	lua_pop(L, 1);

	lua_pushstring(L, "Name");
	lua_gettable(L, -2);

	lua_pushstring(L, "nomen");
	lua_gettable(L, -2);

	strcpy_s(game->playerConfigs.name, sizeof(game->playerConfigs.name), lua_tostring(L, -1));

	lua_pop(L, 1);

	//Model
	lua_pop(L, 1);

	lua_pushstring(L, "Model");
	lua_gettable(L, -2);
	lua_pushstring(L, "letter");
	lua_gettable(L, -2);

	strcpy_s(game->playerConfigs.model, sizeof(game->playerConfigs.model), lua_tostring(L, -1));
	
	//Clear Stack
	lua_settop(L, 0);

	//Close Lua Object
	lua_close(L);
}

static void enemyConfigs(final_game_t* game) 
{
	//Create New Lua State
	lua_State* L;
	L = luaL_newstate();
	luaL_openlibs(L);
	luaL_dofile(L, "luaGameEnemyComps.lua");

	//Enemy Components
	lua_getglobal(L, "Enemy");

	//Transform
	lua_pushstring(L, "Transform");
	lua_gettable(L, -2);

	//offset
	lua_pushstring(L, "offset");
	lua_gettable(L, -2);
	game->enemyConfigs.offset = lua_tonumber(L, -1);

	//zone
	lua_pop(L, 1);
	lua_pushstring(L, "zone");
	lua_gettable(L, -2);
	game->enemyConfigs.zone = lua_tonumber(L, -1);

	lua_pop(L, 1);

	//Name
	lua_pop(L, 1);

	lua_pushstring(L, "Name");
	lua_gettable(L, -2);
	lua_pushstring(L, "nomen");
	lua_gettable(L, -2);

	strcpy_s(game->enemyConfigs.name, sizeof(game->enemyConfigs.name), lua_tostring(L, -1));

	lua_pop(L, 1);

	//Model
	lua_pop(L, 1);

	lua_pushstring(L, "Model");
	lua_gettable(L, -2);
	lua_pushstring(L, "letter");
	lua_gettable(L, -2);

	strcpy_s(game->enemyConfigs.model, sizeof(game->enemyConfigs.model), lua_tostring(L, -1));

	//Clear Lua Stack
	lua_settop(L, 0);

	//Close Lua Object
	lua_close(L);
}

static void cameraConfigs(final_game_t* game) 
{
	//Create New Lua State
	lua_State* L;
	L = luaL_newstate();
	luaL_openlibs(L);
	luaL_dofile(L, "luaGameCameraComps.lua");

	//Camera Compnents
	lua_getglobal(L, "Camera");

	//Name
	lua_pushstring(L, "name");
	lua_gettable(L, -2);
	strcpy_s(game->camConfigs.name, sizeof(game->camConfigs.name), lua_tostring(L, -1));

	//angle
	lua_pop(L, 1);
	lua_pushstring(L, "angle");
	lua_gettable(L, -2);
	game->camConfigs.angle = lua_tonumber(L, -1);

	//aspect
	lua_pop(L, 1);
	lua_pushstring(L, "aspect");
	lua_gettable(L, -2);
	game->camConfigs.aspect = lua_tonumber(L, -1);

	//z_near
	lua_pop(L, 1);
	lua_pushstring(L, "z_near");
	lua_gettable(L, -2);
	game->camConfigs.z_near = lua_tonumber(L, -1);

	//z_far
	lua_pop(L, 1);
	lua_pushstring(L, "z_far");
	lua_gettable(L, -2);
	game->camConfigs.z_far = lua_tonumber(L, -1);

	//left
	lua_pop(L, 1);
	lua_pushstring(L, "left");
	lua_gettable(L, -2);
	game->camConfigs.left = lua_tonumber(L, -1);

	//right
	lua_pop(L, 1);
	lua_pushstring(L, "right");
	lua_gettable(L, -2);
	game->camConfigs.right = lua_tonumber(L, -1);

	//top
	lua_pop(L, 1);
	lua_pushstring(L, "top");
	lua_gettable(L, -2);
	game->camConfigs.top = lua_tonumber(L, -1);

	//bot
	lua_pop(L, 1);
	lua_pushstring(L, "bot");
	lua_gettable(L, -2);
	game->camConfigs.bot = lua_tonumber(L, -1);

	//eyePos
	lua_pop(L, 1);
	lua_pushstring(L, "eyePos");
	lua_gettable(L, -2);
	game->camConfigs.eyePos = lua_tonumber(L, -1);

	//Pops off final element of Stack
	lua_pop(L, 1);

	//Clears Lua Stack
	lua_settop(L, 0);

	//Closes Lua Object
	lua_close(L);
}
////////////////////////////////////////////

//Cleans up after frogger game is done
void final_game_destroy(final_game_t* game)
{
	//net_destroy(game->net);
	ecs_destroy(game->ecs);
	timer_object_destroy(game->timer);
	unload_resources(game);
	heap_free(game->heap, game);
}

//Updates frame and current progress of frogger game
void final_game_update(final_game_t* game)
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
static void load_resources(final_game_t* game)
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
	//Letter Z for the Player color
	game->cube_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts_player,
		.vertex_data_size = sizeof(cube_verts_player),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};

	//Letter A for the Enemy color
	game->cube_mesh_A = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts_A,
		.vertex_data_size = sizeof(cube_verts_A),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};

	//Letter B for the Enemy color
	game->cube_mesh_B = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts_B,
		.vertex_data_size = sizeof(cube_verts_B),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};

	//Letter C for the Enemy color
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
static void unload_resources(final_game_t* game)
{
	heap_free(game->heap, fs_work_get_buffer(game->vertex_shader_work));
	heap_free(game->heap, fs_work_get_buffer(game->fragment_shader_work));
	fs_work_destroy(game->fragment_shader_work);
	fs_work_destroy(game->vertex_shader_work);
}

//NOT IMPLEMENTED
static void player_net_configure(ecs_t* ecs, ecs_entity_ref_t entity, int type, void* user)
{
	final_game_t* game = user;
	model_component_t* model_comp = ecs_entity_get_component(ecs, entity, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;
}

//Spawns the next batch of enemies
static void spawnEnemies(final_game_t* game, int start)
{
	if (start == 0) { srand((int)time(NULL)); } // Initialization, should only be called once.
	int maxEntity = 4;

	int r = (rand() % maxEntity) ;      // Returns a pseudo-random integer between 0 and RAND_MAX.

	//First Batch of Enemies
	for (int i = 0; i < r; i++)
	{
		spawn_enemy(game, i, -3);
	}

	r = (rand() % maxEntity) + 1;

	//Second Batch of Enemies
	for (int j = 0; j < r; j++)
	{
		spawn_enemy(game, j, 0);
	}

	r = (rand() % maxEntity) + 2;

	//Third Batch of Enemies
	for (int k = 0; k < r; k++)
	{
		spawn_enemy(game, k, 3);
	}
}

//Spawns a player object
static void spawn_player(final_game_t* game, int index)
{
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->player_type) |
		(1ULL << game->name_type);
	game->player_ent = ecs_entity_add(game->ecs, k_player_ent_mask);

	//Player Transform Component
	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	transform_comp->transform.translation.y = (float)index * game->playerConfigs.transform.translation.y;
	transform_comp->transform.translation.z = game->playerConfigs.transform.translation.z;

	//Player Name Component
	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), game->playerConfigs.name);

	//Player Component
	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->index = index;

	//Player Model Component
	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);

	//Decides Player Color based on Lua Player Model Value
	if (strcmp(game->playerConfigs.model, "Z") == 0)
	{
		model_comp->mesh_info = &game->cube_mesh;
		model_comp->shader_info = &game->cube_shader;
	}
}

//Spawns an enemy object
static void spawn_enemy(final_game_t* game, int index, int region)
{
	uint64_t k_enemy_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->enemy_type) |
		(1ULL << game->name_type);
	game->enemy_ent = ecs_entity_add(game->ecs, k_enemy_ent_mask);

	//Enemy Transform Component
	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->enemy_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);
	transform_comp->transform.translation.y = ((float)index * 1.0f) + game->enemyConfigs.offset;
	transform_comp->transform.translation.z = (float)region * game->enemyConfigs.zone;

	//Enemy Name Component
	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->enemy_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), game->enemyConfigs.name);

	//Enemy Component
	enemy_component_t* enemy_comp = ecs_entity_get_component(game->ecs, game->enemy_ent, game->player_type, true);
	enemy_comp->index = index;

	//Enemy Model Component
	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->enemy_ent, game->model_type, true);

	//Decides Enemy Color based on Lua Enemy Model Value
	if(strcmp(game->enemyConfigs.model, "A") == 0)
	{
		model_comp->mesh_info = &game->cube_mesh_A;
	}
	if (strcmp(game->enemyConfigs.model, "B") == 0)
	{
		model_comp->mesh_info = &game->cube_mesh_B;
	}
	if (strcmp(game->enemyConfigs.model, "C") == 0)
	{
		model_comp->mesh_info = &game->cube_mesh_C;
	}
	model_comp->shader_info = &game->cube_shader;
}

//Spawns a camera object
static void spawn_camera(final_game_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	//Gets name from Lua Camera Configs
	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), game->camConfigs.name);

	//Gets bounds and perspective from Lua Camera Configs
	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
	mat4f_make_ortho(&camera_comp->projection, game->camConfigs.angle
		, game->camConfigs.aspect, game->camConfigs.z_near, game->camConfigs.z_far, game->camConfigs.left, game->camConfigs.right, game->camConfigs.top, game->camConfigs.bot);

	//Gets eye position from Lua Camera Configs
	vec3f_t eye_pos = vec3f_scale(vec3f_forward(), game->camConfigs.eyePos);
	vec3f_t forward = vec3f_forward();
	vec3f_t up = vec3f_up();
	mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
}

//Gathers player input to update player models
static void update_players(final_game_t* game)
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

		//Keeps the player bounded
		if (transform_comp->transform.translation.z <= -6.0f)
		{
			transform_comp->transform.translation.z = 6.0f;
		}
		if (transform_comp->transform.translation.z > 6.0f)
		{
			transform_comp->transform.translation.z = 6.0f;
		}

		//Responsible for player movement
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
static void update_enemies(final_game_t* game, int dir)
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

		//Removes Enemies that are off screen
		if (transform_comp->transform.translation.y < -13.0f)
		{
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
		}

		transform_t move;
		transform_identity(&move);
		
		//Responsible for whether enemies move left or right
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
static void check_collision(final_game_t* game) 
{
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

		float dist = vec3f_dist2(transform_comp->transform.translation, transform_comp_player->transform.translation);

		if (dist <= 0.5f)
		{
			//ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
			transform_comp_player->transform.translation.z = 6.0f;
		}

	}

}

//Draws every model
static void draw_models(final_game_t* game)
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
