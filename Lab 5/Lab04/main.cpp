#pragma region includes
// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <omp.h>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// Project includes
#include "maths_funcs.h"
#include "stb_image.h"

#pragma endregion

#pragma region constants

/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define MESH_NAME "models/sphere.dae"
#define ASTEROID_MESH "models/sphere.dae"
#define NUM_TEXTURES 16

#define INDEX_BUFFER 0
#define POSITION_LOCATION 1
#define TEX_COORD_LOCATION 2
#define NORMAL_LOCATION 3
#define INSTANCE_MAT_LOCATION 4

#define ASTEROID_BELT_RADIUS 175.0f
// Saturn Ring Constants expressed relative to Saturn's radius
#define ASTEROID_SCALE_FACTOR planets[0].radius * 0.4f

// rings require much much smaller particles and a much tighter radius
// they should appear to be annuli viewed at a distance
#define RING_SCALE_FACTOR 0.10f
#define RING_Y_SCALE_FACTOR 0.016125f

#define D_RING_RADIUS 0.0644f
#define C_RING_RADIUS 0.1502f
#define B_RING_RADIUS 0.2190f
#define A_RING_RADIUS 0.1254f
#define F_RING_RADIUS 0.0002f

#define DC_GAP_DIAMETER 0.0025f
#define CB_GAP_DIAMETER 0.0000f
#define BA_GAP_DIAMETER 0.0807f
#define AF_GAP_DIAMETER 0.0446f

#define INNER_RING_LIM 1.1489f
#define OUTER_RING_LIM 2.4073f

// end Saturn Ring Constants

#define NUM_ASTEROIDS 1024
#define NUM_RING_PARTS 128
/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

#pragma endregion

#pragma region SimpleTypes
typedef struct
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
} ModelData;

typedef struct
{
	const char * name;
	bool is_visible;
	GLfloat orbit_radius;
	GLfloat orbit_vel;
	GLfloat orbit_pos;
	GLfloat radius;
	GLfloat rotation_speed;
	GLfloat rotation_pos;
} planet;

#pragma endregion SimpleTypes

#pragma region globals

using namespace std;

GLuint shaderProgramID, skybox_shader_program_ID, asteroid_shader_program_ID;

ModelData mesh_data, asteroid_data;
unsigned int mesh_vao = 0;
unsigned int vao = 0;
unsigned int skybox_vao = 0;
unsigned int vp_vbo = 0;
unsigned int asteroid_vao = 0;
int skybox_in_use = 0;

GLuint asteroid_buffers[5];
std::vector<unsigned int> Indices;
int width;
int height;

planet planets[9];

const int font = (int)GLUT_BITMAP_TIMES_ROMAN_10;

GLuint loc1, loc2, loc3, loc4;
unsigned int rt_vbo;
unsigned int instance_mat_buffer;

GLfloat rotatez = 0.0f;
int mouse_x = 0;
int mouse_y = 0;
bool no_warp = false;

const GLfloat mouse_dy = 0.5f;

vec3 cam_pos = vec3(1.0f, 10.0f, 20.0f);
vec3 target_pos = vec3(0.0f, 0.5f, 0.0f);
vec3 cam_to_target;
vec3 up = vec3(0.0f, 1.0f, 0.0f);

mat4 asteroid_model_matrices[2][NUM_ASTEROIDS];

// inner -> outer rings ascending numerical order
mat4 saturn_ring_matrices[5][NUM_RING_PARTS];

char * locs[NUM_TEXTURES] = {
	"textures/sun.jpg",
	"textures/planets/mercury.jpg",
	"textures/planets/venus.jpg",
	"textures/planets/earth.jpg",
	"textures/planets/mars.jpg",
	"textures/planets/jupiter.jpg",
	"textures/planets/saturn.jpg",
	"textures/planets/uranus.jpg",
	"textures/planets/neptune.jpg",
	"textures/planets/pluto.jpg",
	"textures/satellites/d_ring.png",
	"textures/satellites/c_ring.png",
	"textures/satellites/b_ring.png",
	"textures/satellites/a_ring.png",
	"textures/satellites/moon.jpg",
	"textures/satellites/kuiper_belt.png"
};

unsigned int textures[NUM_TEXTURES];
std::vector<unsigned int> skyboxes;

#pragma endregion

#pragma region body_data
GLfloat earth_orbit_vel = 1.0f;
// inaccurate, but the inner planets are difficult to discern due to their size

// hold on to planet local matrices
mat4 planet_local[9] = { 
	identity_mat4(), 
	identity_mat4(),
	identity_mat4(),
	identity_mat4(),
	identity_mat4(),
	identity_mat4(),
	identity_mat4(),
	identity_mat4(),
	identity_mat4(),
};

// inaccurate, but they're imperceptible on their true scale (relative to size of sun)
const int planet_moons[9] = {
	0, 0, 1, 2, 4, 1, 5, 1, 5
};

const float lunar_rel_radius[9] = { 
	0.25f, 0.25, 0.25, 0.25, 0.1, 0.1, 0.1, 0.1, 0.25
};

const char * planet_names[9] = {
	"Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto"
};
const GLfloat planet_orbit_radius[9] = {
	2.0f, 3.0f, 4.0f, 5.0f, 9.0f, 14.0f, 16.0f, 18.4f, 20.0f
};
const GLfloat planet_radius[9] = {
	0.08f, 0.18f, 0.2f, 0.1f, 0.5f, 0.45f, 0.35f, 0.32f, 0.06f
};
const GLfloat planet_orbit_vel[9] = {
	earth_orbit_vel * 1.7f,
	earth_orbit_vel * 1.25f,
	earth_orbit_vel * 1.0f,
	earth_orbit_vel * 0.84f,
	earth_orbit_vel * 0.4f,
	earth_orbit_vel * 0.39f,
	earth_orbit_vel * 0.31f,
	earth_orbit_vel * 0.26f,
	earth_orbit_vel * 0.18f
};
GLfloat planet_orbital_pos[9] = {
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f
};
const GLfloat planet_rotation_vel[9] = {
	earth_orbit_vel * 0.016f,
	earth_orbit_vel * -0.004f,
	earth_orbit_vel * 1.0f,
	earth_orbit_vel * 0.9f,
	earth_orbit_vel * 2.5f,
	earth_orbit_vel * 2.4f,
	earth_orbit_vel * -1.43f,
	earth_orbit_vel * 1.54f,
	earth_orbit_vel * 0.16f
};
GLfloat planet_rotation_pos[9] = {
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f
};

#pragma endregion

#pragma region skybox

std::vector<char*> blue_skybox_locations = {
	"textures/skybox/blue/bkg1_right.png",
	"textures/skybox/blue/bkg1_left.png",
	"textures/skybox/blue/bkg1_top.png",	
	"textures/skybox/blue/bkg1_bot.png",
	"textures/skybox/blue/bkg1_front.png",
	"textures/skybox/blue/bkg1_back.png",
};

std::vector<char*> lightblue_skybox_locations = {
	"textures/skybox/lightblue/right.png",
	"textures/skybox/lightblue/left.png",	
	"textures/skybox/lightblue/top.png",
	"textures/skybox/lightblue/bot.png",
	"textures/skybox/lightblue/front.png",
	"textures/skybox/lightblue/back.png",
};

std::vector<char*> red_skybox_one_locations = {
	"textures/skybox/red/bkg1_right1.png",
	"textures/skybox/red/bkg1_left2.png",
	"textures/skybox/red/bkg1_top3.png",
	"textures/skybox/red/bkg1_bottom4.png",
	"textures/skybox/red/bkg1_front5.png",
	"textures/skybox/red/bkg1_back6.png",
};

std::vector<char*> red_skybox_two_locations = {
	"textures/skybox/red/bkg2_right1.png",
	"textures/skybox/red/bkg2_left2.png",
	"textures/skybox/red/bkg2_top3.png",
	"textures/skybox/red/bkg2_bottom4.png",
	"textures/skybox/red/bkg2_front5.png",
	"textures/skybox/red/bkg2_back6.png",
};

std::vector<char*> red_skybox_three_locations = {
	"textures/skybox/red/bkg3_right1.png",
	"textures/skybox/red/bkg3_left2.png",
	"textures/skybox/red/bkg3_top3.png",
	"textures/skybox/red/bkg3_bottom4.png",
	"textures/skybox/red/bkg3_front5.png",
	"textures/skybox/red/bkg3_back6.png",
};

float skybox_vertices[] = {
	// positions          
	-1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	-1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f
};

#pragma endregion

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */
	const aiScene* scene = aiImportFile(
		file_name, 
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	); 

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		printf("    %i bones in mesh\n", mesh->mNumBones);
		printf("    mesh has bones? %s \n", (mesh->HasBones() ? "true" : "false"));
		printf("    mesh has textures? %s \n", (mesh->HasTextureCoords(0) ? "true" : "false"));


		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}
	}

	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING

#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}


GLuint CompileShaders(char * vs_path, char * fs_path)
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	GLuint temp_shader_program_ID = glCreateProgram();
	if (temp_shader_program_ID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(temp_shader_program_ID, vs_path, GL_VERTEX_SHADER);
	AddShader(temp_shader_program_ID, fs_path, GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(temp_shader_program_ID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(temp_shader_program_ID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(temp_shader_program_ID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(temp_shader_program_ID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(temp_shader_program_ID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(temp_shader_program_ID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(temp_shader_program_ID);
	return temp_shader_program_ID;
}
#pragma endregion SHADER_FUNCTIONS

#pragma region VBO_FUNCTIONS
void generate_instance_buffer_mesh() {

	asteroid_data = load_mesh(ASTEROID_MESH);

	Indices.reserve(asteroid_data.mPointCount);

	for (int i = 0; i < asteroid_data.mPointCount; i++) {
		Indices.push_back(i);
	}
	glUseProgram(asteroid_shader_program_ID);
	// bind asteroid's VAO 
	glBindVertexArray(asteroid_vao);

	// gen buffers for vertex position, vertex texture, vertex normal and instance matrix at once
	glGenBuffers( ( sizeof(asteroid_buffers) / sizeof(asteroid_buffers[0]) ), asteroid_buffers);

	// Vertex Positions
	glBindBuffer(GL_ARRAY_BUFFER, asteroid_buffers[POSITION_LOCATION]);
	glBufferData(GL_ARRAY_BUFFER, asteroid_data.mPointCount * sizeof(vec3), &asteroid_data.mVertices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(POSITION_LOCATION);
	glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// Vertex Textures
	glBindBuffer(GL_ARRAY_BUFFER, asteroid_buffers[TEX_COORD_LOCATION]);
	glBufferData(GL_ARRAY_BUFFER, asteroid_data.mPointCount * sizeof(vec2), &asteroid_data.mTextureCoords, GL_STATIC_DRAW);
	glEnableVertexAttribArray(TEX_COORD_LOCATION);
	glVertexAttribPointer(TEX_COORD_LOCATION, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// Vertex Normals
	glBindBuffer(GL_ARRAY_BUFFER, asteroid_buffers[NORMAL_LOCATION]);
	glBufferData(GL_ARRAY_BUFFER, asteroid_data.mPointCount * sizeof(vec3), &asteroid_data.mNormals, GL_STATIC_DRAW);
	glEnableVertexAttribArray(NORMAL_LOCATION);
	glVertexAttribPointer(NORMAL_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, asteroid_buffers[INDEX_BUFFER]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Indices[0]) * Indices.size(), &Indices[0], GL_STATIC_DRAW);

	// Instance Matrix - DYNAMIC DRAW AS THEY ARE SUBJECT TO CHANGE
	// matrix buffer bound dynamically in the display function
	glBindBuffer(GL_ARRAY_BUFFER, asteroid_buffers[INSTANCE_MAT_LOCATION]);

	// need to split matrix into 4 vec4s as an attrib pointer can only point at 4 floats at a time, it can't point at a matrix
	for (int i = 0; i < 4; i++) {
		glEnableVertexAttribArray(INSTANCE_MAT_LOCATION + i);
		// pointer increments by 4 * sizeof(GLfloat) each time to correctly index the next 4 GLfloats in the vector 
		glVertexAttribPointer(INSTANCE_MAT_LOCATION + i, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (const GLvoid *)(sizeof(GLfloat) * i * 4));
		// tell opengl that this particular thingy is instanced
		glVertexAttribDivisor(INSTANCE_MAT_LOCATION + i, 1);
	}

	// unbind VAO
	glBindVertexArray(0);
} 
void generateObjectBufferMesh() {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.

#pragma region object_buffers

	// make sure that initially using the 'object' shader!
	glUseProgram(shaderProgramID);
	
	mesh_data = load_mesh(MESH_NAME);
	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	//loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");

	glGenBuffers(1, &vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);
	
	unsigned int vn_vbo = 0;
	glGenBuffers(1, &vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);
	
	unsigned int vt_vbo = 0;
	glGenBuffers (1, &vt_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	glBufferData (GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof (vec2), &mesh_data.mTextureCoords[0], GL_STATIC_DRAW);

	glBindVertexArray(vao);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray (12);
	glBindBuffer(GL_ARRAY_BUFFER, vt_vbo);
	glVertexAttribPointer (12, 2, GL_FLOAT, GL_FALSE, 0, NULL);

#pragma endregion

#pragma region skybox_buffers

	glUseProgram(skybox_shader_program_ID);

	glGenVertexArrays(1, &skybox_vao);
	glBindVertexArray(skybox_vao);

	unsigned int skybox_vbo = 0;
	glGenBuffers(1, &skybox_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skybox_vertices), &skybox_vertices[0], GL_STATIC_DRAW);

	loc4 = glGetAttribLocation(skybox_shader_program_ID, "skybox");

	glEnableVertexAttribArray(loc4);
	glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo);
	glVertexAttribPointer(loc4, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	
	glUseProgram(shaderProgramID);
#pragma endregion
}
#pragma endregion VBO_FUNCTIONS

#pragma region instance_mat_init

// initialising Saturn's rings as well as the asteroids
void init_asteroid_matrices() {

	for (int k = 0; k < 2; k++) {

#pragma omp parallel for
		for (int i = 0; i < NUM_ASTEROIDS; i++) {
			float angle = (float)i / (float)NUM_ASTEROIDS * 360.0f;

			// declare each matrix inside the for loop so each thread has its own copy 
			mat4 temp_model = identity_mat4();

			float disp = (rand() % (int)(2 * 25.0f * 100)) / (200.0f / (1 + (4*k)));
			float asteroid_x = (sin(angle) * ASTEROID_BELT_RADIUS * ((4 * k) + 1)) + disp / 2;

			disp = (rand() % (int)(2 * 25.0f * 100)) / (200.0f / (1 + (4 * k)));// -25.0f;
			float asteroid_y = disp * 0.4;

			disp = (rand() % (int)(2 * 25.0f * 100)) / (200.0f / (1 + (4 * k)));// -25.0f;
			float asteroid_z = (cos(angle) * ASTEROID_BELT_RADIUS * ( (4 * k) + 1)) + disp / 2;

			temp_model = translate(temp_model, vec3(asteroid_x, asteroid_y, asteroid_z));
			temp_model = scale(temp_model, vec3(ASTEROID_SCALE_FACTOR, ASTEROID_SCALE_FACTOR, ASTEROID_SCALE_FACTOR));
			asteroid_model_matrices[k][i] = temp_model;
		}

	}
}

void init_rings() {

	float d_ring_centre = planets[5].radius + ((INNER_RING_LIM + D_RING_RADIUS) * planets[5].radius);
	float c_ring_centre = d_ring_centre + ((D_RING_RADIUS + DC_GAP_DIAMETER + C_RING_RADIUS) * planets[5].radius);
	float b_ring_centre = c_ring_centre + ((C_RING_RADIUS + CB_GAP_DIAMETER + B_RING_RADIUS) * planets[5].radius);
	float a_ring_centre = b_ring_centre + ((B_RING_RADIUS + BA_GAP_DIAMETER + A_RING_RADIUS) * planets[5].radius);
	float f_ring_centre = planets[5].radius + (planets[5].radius * OUTER_RING_LIM);
	mat4 scaled_identity = scale(identity_mat4(), vec3(RING_SCALE_FACTOR, RING_Y_SCALE_FACTOR, RING_SCALE_FACTOR));

	int d_disp_dim = (int) ((D_RING_RADIUS * planets[5].radius) * 1000.0f);
	int c_disp_dim = (int) ((C_RING_RADIUS * planets[5].radius) * 1000.0f);
	int b_disp_dim = (int) ((B_RING_RADIUS * planets[5].radius) * 1000.0f);
	int a_disp_dim = (int) ((A_RING_RADIUS * planets[5].radius) * 1000.0f);

#pragma omp parallel for
	for (int i = 0; i < NUM_RING_PARTS; i++) {

		float angle = (float)i / (float)NUM_RING_PARTS * 360.0f;

		float disp = rand() % d_disp_dim;
		disp /= 1000.0f;
		float ring_one_x = sin(angle) * d_ring_centre + disp;
		
		disp = rand() % d_disp_dim;
		disp /= 1000.0f;
		float ring_one_z = cos(angle) * d_ring_centre + disp;
		
		mat4 temp_ring = translate(scaled_identity, vec3(ring_one_x, 0.0f, ring_one_z));
		saturn_ring_matrices[0][i] = planet_local[5] * temp_ring;

		disp = rand() % c_disp_dim;
		disp /= 1000.0f;
		float ring_two_x = sin(angle) * c_ring_centre + disp;
		
		disp = rand() % c_disp_dim;
		disp /= 1000.0f;
		float ring_two_z = cos(angle) * c_ring_centre + disp;
		
		temp_ring = translate(scaled_identity, vec3(ring_two_x, 0.0f, ring_two_z));
		saturn_ring_matrices[1][i] = planet_local[5] * temp_ring;

		disp = rand() % b_disp_dim;
		disp /= 1000.0f;
		float ring_three_x = sin(angle) * b_ring_centre + disp;
		
		disp = rand() % b_disp_dim;
		disp /= 1000.0f;
		float ring_three_z = cos(angle) * b_ring_centre + disp;
		
		temp_ring = translate(scaled_identity, vec3(ring_three_x, 0.0f, ring_three_z));
		saturn_ring_matrices[2][i] = planet_local[5] * temp_ring;

		disp = rand() % a_disp_dim;
		disp /= 1000.0f;
		float ring_four_x = sin(angle) * a_ring_centre + disp;
		
		disp = rand() % a_disp_dim;
		disp /= 1000.0f;
		float ring_four_z = cos(angle) * a_ring_centre + disp;
		
		temp_ring = translate(scaled_identity, vec3(ring_four_x, 0.0f, ring_four_z));
		saturn_ring_matrices[3][i] = planet_local[5] * temp_ring;

		float ring_five_x = sin(angle) * f_ring_centre;
		float ring_five_z = cos(angle) * f_ring_centre;
		
		temp_ring = translate(scaled_identity, vec3(ring_five_x, 0.0f, ring_five_z));
		saturn_ring_matrices[4][i] = planet_local[5] * temp_ring;


	}
}

#pragma endregion

void display() {

#pragma region init

	cam_to_target = target_pos - cam_pos;
	// tell GL to only draw onto a pixel if the shape is closer to the viewer	
	mat4 view = look_at(cam_pos, target_pos, up);
	mat4 persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 200.0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // we're not using the stencil buffer now
	glEnable(GL_DEPTH_TEST);

#pragma endregion	
#pragma region sun
	// *******************************************************************************
	//									 The Sun 
	// *******************************************************************************
	glUseProgram(shaderProgramID);
	glBindVertexArray(vao);
	////Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramID, "proj");

	mat4 sun_local = identity_mat4();

	sun_local = rotate_y_deg(sun_local, rotatez);
	// assign solar_rotation uniform at this point

	sun_local = scale(sun_local, vec3(2.0f, 2.0f, 2.0f));
	sun_local = translate(sun_local, vec3(0.0f, 0.0f, 0.0f));
																			
	mat4 sun_global = sun_local;	// for the root, we orient it in global space
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, sun_global.m);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);


#pragma endregion
#pragma region planets and moons

	for (int i = 0; i < 9; i++)
	{
		planet_local[i] = identity_mat4();
		// self-rotation - translate to origin 
		planet_local[i] = translate(planet_local[i], vec3(0.0f, 0.0f, 0.0f));
		planet_local[i] = rotate_y_deg(planet_local[i], planets[i].rotation_pos);

		// translate back from origin
		planet_local[i].m[3] = -planet_local[i].m[3];
		planet_local[i].m[7] = -planet_local[i].m[7];
		planet_local[i].m[11] = -planet_local[i].m[11];

		// scaling, dist. from origin and orbit now
		planet_local[i] = scale(planet_local[i], vec3(planets[i].radius, planets[i].radius, planets[i].radius));
			
		// jupiter will follow the sun
		planet_local[i] = translate(planet_local[i], vec3(planets[i].orbit_radius, 0, 0));
		planet_local[i] = sun_local * planet_local[i];
		planet_local[i] = rotate_y_deg(planet_local[i], planets[i].orbit_pos);

#pragma region moons

		if (planet_moons[i] > 0) {
			// generic moon texture at 14
			glBindTexture(GL_TEXTURE_2D, textures[14]);

			for (int j = 0; j < planet_moons[i]; j++) {
				mat4 moon_local = identity_mat4();
				moon_local = scale(moon_local, vec3(lunar_rel_radius[i], lunar_rel_radius[i], lunar_rel_radius[i]));
				float translate_offset = -1;

				for (int k = 0; k < j; k++)
					translate_offset *= -1;
				translate_offset += j / planet_moons[i];


				moon_local = translate(moon_local, vec3(translate_offset, translate_offset / 10, translate_offset));
				moon_local = rotate_y_deg(moon_local, j * planets[i].orbit_pos);
				moon_local = planet_local[i] * moon_local;
	

				glUniformMatrix4fv(matrix_location, 1, GL_FALSE, moon_local.m);

				glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);
			}
		}
#pragma endregion

		planets[i].orbit_pos += planets[i].orbit_vel;
		planets[i].rotation_pos += planets[i].rotation_speed;

		if (planets[i].orbit_pos > 360.0f)
			planets[i].orbit_pos -= 360.0f;

		if (planets[i].rotation_pos > 360.0f)
			planets[i].rotation_pos -= 360.0f;

		glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
		glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
		glUniformMatrix4fv(matrix_location, 1, GL_FALSE, planet_local[i].m);

		// offset texture index by +1 as the sun texture is at location 0
		glBindTexture(GL_TEXTURE_2D, textures[i + 1]);
		glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);
	}

	glBindVertexArray(0);

#pragma endregion
#pragma region asteroids


	// swap to asteroid shader
	glUseProgram(asteroid_shader_program_ID);
	// view and proj matrices for asteroid shader
	int asteroid_view_mat_location = glGetUniformLocation(asteroid_shader_program_ID, "view");
	int asteroid_proj_mat_location = glGetUniformLocation(asteroid_shader_program_ID, "proj");
	int instance_parent_matrix_location = glGetUniformLocation(asteroid_shader_program_ID, "parent");

	// inherit solar rotation for the asteroid orbits (NOTE CODE IN instance_vertex_shader.txt)
	glUniformMatrix4fv(instance_parent_matrix_location, 1, GL_FALSE, sun_local.m);

	// bind asteroid VAO
	glBindVertexArray(asteroid_vao);
	
	// bind & populate instance matrix buffer
	glBindBuffer(GL_ARRAY_BUFFER, asteroid_buffers[INSTANCE_MAT_LOCATION]);
	glBufferData(GL_ARRAY_BUFFER, NUM_ASTEROIDS * sizeof(mat4), &asteroid_model_matrices[0][0], GL_DYNAMIC_DRAW);

	// make asteroids look like mercury
	glBindTexture(GL_TEXTURE_2D, textures[1]);

	// assign uniforms
	glUniformMatrix4fv(asteroid_proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(asteroid_view_mat_location, 1, GL_FALSE, view.m);

	// draw the things
	glDrawElementsInstanced(GL_TRIANGLES, asteroid_data.mPointCount, GL_UNSIGNED_INT, 0, NUM_ASTEROIDS / 4);

// The code below is for asteroids with faster / slower orbit periods, but it's a massive source of slowdown - NOTE: REQUIRES IDENTITY_MAT4 PASSED AS PARENT TO VS
// update matrices for next go around
//#pragma omp parallel for
//	for (int i = 0; i < NUM_ASTEROIDS; i++) {
//		asteroid_model_matrices[i] = rotate_y_deg(asteroid_model_matrices[i], i * 0.001f);
//	}

#pragma endregion
#pragma region rings

	// inherit saturn rotation for the ring orbits (NOTE CODE IN instance_vertex_shader.txt)
	glUniformMatrix4fv(instance_parent_matrix_location, 1, GL_FALSE, planet_local[5].m);

	// ASTEROID RENDERING FOR SATURN RINGS
	for (int j = 0; j < 5; j++) {
		// bind instance matrix buffer
		glBufferData(GL_ARRAY_BUFFER, NUM_RING_PARTS * sizeof(mat4), &saturn_ring_matrices[j][0], GL_DYNAMIC_DRAW);

		if (j < 4) {
			glBindTexture(GL_TEXTURE_2D, textures[10 + j]);
		}
		else {
			// use b ring texture for f ring
			glBindTexture(GL_TEXTURE_2D, textures[12]);
		}
		glDrawElementsInstanced(
			GL_TRIANGLES,
			asteroid_data.mPointCount,
			GL_UNSIGNED_INT,
			0,
			NUM_RING_PARTS);
	}

#pragma endregion
#pragma region kuiper belt
	glUniformMatrix4fv(instance_parent_matrix_location, 1, GL_FALSE, sun_local.m);

	// bind asteroid VAO
	glBindVertexArray(asteroid_vao);

	// bind & populate instance matrix buffer
	glBindBuffer(GL_ARRAY_BUFFER, asteroid_buffers[INSTANCE_MAT_LOCATION]);
	glBufferData(GL_ARRAY_BUFFER, NUM_ASTEROIDS * sizeof(mat4), &asteroid_model_matrices[1][0], GL_DYNAMIC_DRAW);

	// white kuiper belt texture - they're icy comets
	glBindTexture(GL_TEXTURE_2D, textures[15]);

	// assign uniforms
	glUniformMatrix4fv(asteroid_proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(asteroid_view_mat_location, 1, GL_FALSE, view.m);

	// draw the things
	glDrawElementsInstanced(
		GL_TRIANGLES,
		asteroid_data.mPointCount,
		GL_UNSIGNED_INT,
		0,
		NUM_ASTEROIDS);

	// unbind the asteroid VAO
	glBindVertexArray(0);
#pragma endregion
#pragma region skybox_render
	// *******************************************************************************
	// *******************************************************************************
	//						SKYBOX SECTION - LAST FOR SPEED
	// *******************************************************************************
	// *******************************************************************************

	glUseProgram(skybox_shader_program_ID);
	int skybox_view_loc = glGetUniformLocation(skybox_shader_program_ID, "view");
	int skybox_projection_loc = glGetUniformLocation(skybox_shader_program_ID, "projection");

	glBindVertexArray(skybox_vao);

	// skybox lives at z=1.0 so don't clip it! reset to GL_LESS at the top of the loop 
	glDepthFunc(GL_LEQUAL);
	// remove translation from view matrix (upper-left mat3 only, rest of it identity)
	view.m[3] = 0; view.m[7] = 0; view.m[11] = 0; view.m[12] = 0; view.m[13] = 0; view.m[14] = 0; view.m[15] = 1;
	glUniformMatrix4fv(skybox_projection_loc, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(skybox_view_loc, 1, GL_FALSE, view.m);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxes[skybox_in_use]);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

#pragma endregion

	rotatez += 0.2f;
	if (rotatez > 360.0f)
		rotatez -= 360.0f;
	glutSwapBuffers();
}

#pragma region textures
void load_textures(unsigned char * data[], int width[], int height[]) {
	/*
	texture-loading code from https://learnopengl.com/Getting-started/Textures 
	*/
	for (int i = 0; i < NUM_TEXTURES; i++) {

		unsigned int tex;
		glGenTextures(1, &tex);

		glBindTexture(GL_TEXTURE_2D, tex);
		// set the texture wrapping/filtering options (on the currently bound texture object)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// load and generate the texture


		if (data[i])
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width[i], height[i], 0, GL_RGB, GL_UNSIGNED_BYTE, data[i]);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		else
		{
			std::cout << "Failed to load texture at " << locs[i] << ". " << std::endl;
		}

		textures[i] = tex;
	}
}

void load_skybox(unsigned char * data[], std::vector<char*> skybox_locs, int w[], int h[]) {
	unsigned int tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

	for (int i = 0; i < 6; i++) {
		if (data[i]) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, w[i], h[i], 0, GL_RGB, GL_UNSIGNED_BYTE, data[i]);
		}
		else {
			std::cout << "Failed to load image at path: " << skybox_locs[i] << std::endl;
		}
	}


	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	skyboxes.push_back(tex);
}
#pragma endregion

#pragma region user_input

void keypress(unsigned char key, int x, int y) {

	switch (key)
	{
	case('w'): {
		// also adding the 'y' to 'enable' flight - the scene is in space, being bounded at 'ground level' doesn't make sense
		cam_pos += cam_to_target * 0.01;
		target_pos += cam_to_target * 0.01;
		break;
	}
	case('s'): {
		cam_pos -= cam_to_target * 0.01;
		target_pos -= cam_to_target * 0.01;
		break;
	}
	case('a'): {
		// not adding the 'y' here, just strafing 
		GLfloat dx, dz;
		dx = cam_to_target.v[2] * 0.01;
		dz = -cam_to_target.v[0] * 0.01;
		cam_pos.v[0] += dx;
		cam_pos.v[2] += dz;
		target_pos.v[0] += dx; 
		target_pos.v[2] += dz;
		break;
	}
	case('d'): {
		GLfloat dx, dz;
		dx = cam_to_target.v[2] * 0.01;
		dz = -cam_to_target.v[0] * 0.01;
		cam_pos.v[0] -= dx;
		cam_pos.v[2] -= dz;
		target_pos.v[0] -= dx;
		target_pos.v[2] -= dz;
		break;
	}
	case(' '): {
		cam_pos.v[1] += 0.1f;
		target_pos.v[1] += 0.1f;

		break;
	}
	case('x'): {
		cam_pos.v[1] -= 0.1f;
		target_pos.v[1] -= 0.1f;
		break;
	}
	case ('h'): {
		skybox_in_use++;
		
		if (skybox_in_use == 5)
			skybox_in_use = 0;

		break;
	}
				// default included to stop the warning message
	default: {
		break;
	}
	}
}

void mouse_op(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON)	// let the cursor be hidden and rooted to middle of window
	{
		no_warp = false;
		glutSetCursor(GLUT_CURSOR_CROSSHAIR);
	}
	if (button == GLUT_RIGHT_BUTTON)	// cursor not hidden to show no longer rooted
	{
		no_warp = true;
		glutSetCursor(GLUT_CURSOR_INHERIT);
	}
}

void mouse_move(int x, int y) {

	GLfloat dx, dz;
	// don't let the mouse do things when 'clicked out'
	if (no_warp == true)
		return;

	if (x < (width/2)) {
		dx = cam_to_target.v[2] * 0.025;
		dz = -cam_to_target.v[0] * 0.025;
		
		// mouse is for looking, change target only
		target_pos.v[0] += dx;
		target_pos.v[2] += dz;
	}
	else if (x > (width / 2)) {
		dx = cam_to_target.v[2] * 0.025;
		dz = -cam_to_target.v[0] * 0.025;

		// mouse is for looking, change target only
		target_pos.v[0] -= dx;
		target_pos.v[2] -= dz;
	}


	if (y < (height / 2)) {
		// mouse is for looking, change target only
		target_pos.v[1] += mouse_dy;
	}
	else if (y > (height / 2)) {
		// mouse is for looking, change target only
		target_pos.v[1] -= mouse_dy;
	}

	glutWarpPointer(width / 2, height / 2);
}

#pragma endregion

void updateScene() {

	// Placeholder code, if you want to work with framerate
	// Wait until at least 16ms passed since start of last frame (Effectively caps framerate at ~60fps)
	static DWORD  last_time = 0;
	DWORD  curr_time = timeGetTime();
	float  delta = (curr_time - last_time) * 0.001f;
	
	if (delta > 0.03f)
		delta = 0.03f;

	last_time = curr_time;

	// Draw the next frame
	glutPostRedisplay();
}

void initialise_planets() {

#pragma omp parallel for
	for (int i = 0; i < sizeof(planets) / sizeof(planet); i++) {
		planets[i].orbit_pos = planet_orbital_pos[i];
		planets[i].radius = planet_radius[i];
		planets[i].orbit_vel = planet_orbit_vel[i];
		planets[i].orbit_radius = planet_orbit_radius[i];
		planets[i].rotation_pos = planet_rotation_pos[i];
		planets[i].rotation_speed = planet_rotation_vel[i];
		planets[i].name = planet_names[i];
		planets[i].is_visible = true;
	}

}

void init()
{
#pragma region shader_init
	// Set up the shaders
	char * instance_vs = "shaders/instance_vertex_shader.txt";
	char * instance_fs = "shaders/instance_fragment_shader.txt";
	char * object_vs = "shaders/simpleVertexShader.txt";
	char * object_fs = "shaders/simpleFragmentShader.txt";
	char * skybox_vs = "shaders/skybox_vertex_shader.txt";
	char * skybox_fs = "shaders/skybox_fragment_shader.txt";

	shaderProgramID = CompileShaders(object_vs, object_fs);
	skybox_shader_program_ID = CompileShaders(skybox_vs, skybox_fs);
	asteroid_shader_program_ID = CompileShaders(instance_vs, instance_fs);
#pragma endregion

	// load mesh into a vertex buffer array
	generateObjectBufferMesh();
	generate_instance_buffer_mesh();

#pragma region textures_init
	int width[NUM_TEXTURES], height[NUM_TEXTURES], nrchannels[NUM_TEXTURES];
	unsigned char * data[NUM_TEXTURES];

#pragma omp parallel for	
	for (int i = 0; i < NUM_TEXTURES; i++) {
		data[i] = stbi_load(locs[i], &width[i], &height[i], &nrchannels[i], 0);
	}
	load_textures(data, width, height);

#pragma omp parallel for	
	for (int i = 0; i < NUM_TEXTURES; i++) {
		stbi_image_free(data[i]);
	}
#pragma endregion

#pragma region skybox_loading_init

	int w[5][6], h[5][6], n_chan[5][6];
	unsigned char * skybox_data[5][6];

#pragma omp parallel for	
	for (int i = 0; i < 6; i++) {
		skybox_data[0][i] = stbi_load(blue_skybox_locations[i], &w[0][i], &h[0][i], &n_chan[0][i], 0);
		skybox_data[1][i] = stbi_load(lightblue_skybox_locations[i], &w[1][i], &h[1][i], &n_chan[1][i], 0);
		skybox_data[2][i] = stbi_load(red_skybox_one_locations[i], &w[2][i], &h[2][i], &n_chan[2][i], 0);
		skybox_data[3][i] = stbi_load(red_skybox_two_locations[i], &w[3][i], &h[3][i], &n_chan[3][i], 0);
		skybox_data[4][i] = stbi_load(red_skybox_three_locations[i], &w[4][i], &h[4][i], &n_chan[4][i], 0);
	}

	load_skybox(skybox_data[0], blue_skybox_locations, w[0], h[0]);
	load_skybox(skybox_data[1], lightblue_skybox_locations, w[1], h[1]);
	load_skybox(skybox_data[2], red_skybox_one_locations, w[2], h[2]);
	load_skybox(skybox_data[3], red_skybox_two_locations, w[3], h[3]);
	load_skybox(skybox_data[4], red_skybox_three_locations, w[4], h[4]);

#pragma omp parallel for	
	for (int i = 0; i < 6; i++) {
		stbi_image_free(skybox_data[0][i]);
		stbi_image_free(skybox_data[1][i]);
		stbi_image_free(skybox_data[2][i]);
		stbi_image_free(skybox_data[3][i]);
		stbi_image_free(skybox_data[4][i]);

	}
#pragma	endregion

	initialise_planets();
	init_asteroid_matrices();
	init_rings();

}

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	width = glutGet(GLUT_SCREEN_WIDTH);
	height = glutGet(GLUT_SCREEN_HEIGHT);

	glutInitWindowSize(width, height);
	//glutInitWindowPosition(width/2, height/2);
	glutCreateWindow("Hello Triangle");
	glutFullScreen();
	glutWarpPointer(width / 2, height / 2);			// mouse initialisation - put cursor in middle of window and hide it
	glutSetCursor(GLUT_CURSOR_CROSSHAIR);

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);
	glutMouseFunc(mouse_op);
	glutPassiveMotionFunc(mouse_move);
	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}