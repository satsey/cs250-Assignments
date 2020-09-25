/*****************************************************************************/
/*!
\file main.cpp
\author Yap Jin Ying Akina
\par email: akina.yap\@digipen.edu
\par DigiPen login: akina.yap
\par Course: A1
\par Assignment 1
\date 23 MAY 2017

Copyright (C) 2017 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/*****************************************************************************/

#include <imgui.h>
#include "imgui_impl_glfw_gl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#include <vector>
#include <filesystem>

static void Init();
static bool LoadModel(const std::string& path);
static void Draw();
static void Update();
static void Cleanup();

static bool CreateLogFile(std::string file_name);
static void ErrorCallbackForGLFW(int error, char const* description);
template <typename... Args> bool WriteToFile(std::string file_name, Args&&... args);
static void WindowResizeCallbackForGLFW(GLFWwindow* window, int width, int height);
static void KeyCallbackForGLFW(GLFWwindow* window, int key, int scancode, int action, int mods);
static void LogGLParams(std::string file_name);
static double UpdateTime(double &fps, double int_fps_calc = 1.0);
static void get_shader_file_contents(const std::string& shader, char*& content);
static bool CheckShaderCompileStatus(GLuint shader_hdl, std::string diag_msg);
static bool CheckShaderProgramLinkStatus(GLuint program_hdl, std::string diag_msg);

static GLFWwindow* s_window_handle; // handle to both window and GL context
static std::string s_log_filename{ "debuglog.txt" }; // log file name
static int s_window_width = 1600, s_window_height = 900; // viewport dimensions
static double s_frames_per_second;
static bool s_window_close_flag = false;
static bool s_window_fullsize = false;


struct Object
{
    // everything required to define the geometry of the object ...
    std::vector<glm::vec3> allVertices; //positions
    std::vector<int> allIndices; //index

    std::vector<glm::vec3> allVertexNormals; //normals

    glm::vec3 colorForAllVertices = glm::vec3(0.f, 0.f, 0.f); //color
    glm::vec3 colorForNormals = glm::vec3(1.0f, 1.0f, 1.0f);

    GLuint s_vao_hdl; //vertex array Object
    GLuint s_vbo_hdl; //vertex buffer object
    GLuint s_ebo_hdl; //Index buffer Object

    GLuint s_nvao_hdl; //vertex array Object to draw normal
    GLuint s_nvbo_hdl; //vertex buffer object
    GLuint s_nebo_hdl; //Index Buffer Object 

    //variables used for computing vertices when loading model
    float unitScale;  //compute the untiScale to Scale model to normalized coordinates
    glm::vec3 center; //compute the centre of the object to translate the object to 0,0,0

    GLuint numOfTriangles{0};

    GLuint ModelDrawMethod{ GL_TRIANGLES };
    GLuint NormalDrawMethod{ GL_LINES };
};

glm::vec3 colorForAllVertices = glm::vec3(1.f, 0.f, 0.f); //color

std::vector<Object> ObjectList; //list of Model Object Data


// everything required to update and display object ...


static GLuint s_shaderpgm_hdl; //Shader program ID


static glm::vec3 s_scale_factors(5.f, 5.f, 5.f); // scale parameters
static glm::vec3 s_world_position(0.f, 0.f, -2.f); // position in world frame
static GLfloat s_angular_displacement = 0.f; // current angular displacement in degrees

static glm::vec3 s_orientation_axis = glm::normalize(glm::vec3(0.f, 0.f, 1.f)); // orientation axis

static glm::mat4 s_proj_mtx = glm::mat4(1.f); // perspective xform - computed once for the whole scene
static glm::mat4 s_view_mtx = glm::mat4(1.f); // view xform - again computed once for the whole scene
static glm::mat4 s_mvp_xform = glm::mat4(1.f); // model-world-view-clip transform matrix ...

static bool drawNormals = false; //parameter for the Imgui to draw the normals for the model

// For the editor
static float fov = 45.f;   
static float near = 1.f;
static float far = 200.f;
std::vector<const char*> modelList;
static int model = -1;

int main() {

  Init(); // very first update

  {
      //ImGui::CreateContext();
      ImGui_ImplGlfwGL3_Init(s_window_handle, true);
  }


  // Load Default Model
  //LoadModel("assets/cube.obj");


  while (!glfwWindowShouldClose(s_window_handle))
  {
    Update(); // create graphics task i+1
    Draw(); // render graphics task i

  }

  ImGui_ImplGlfwGL3_Shutdown();
  Cleanup();
}

struct VtxPosClr {
  glm::vec3 pos; // vertex position coordinates (x, y, z)
  glm::vec3 clr; // vertex color coordinates (r, g, b)
};

void DrawObject(Object obj) {

    std::vector<glm::vec3> bufferData;

    for (int i = 0; i < obj.allVertexNormals.size(); i++)
    {
        bufferData.push_back(obj.allVertices[i]);
        bufferData.push_back(obj.allVertexNormals[i]);
    }

    {
        glGenVertexArrays(1, &obj.s_vao_hdl);
        glGenBuffers(1, &obj.s_vbo_hdl);
        glGenBuffers(1, &obj.s_ebo_hdl);

        glBindVertexArray(obj.s_vao_hdl);

        glBindBuffer(GL_ARRAY_BUFFER, obj.s_vbo_hdl);
        glBufferData(GL_ARRAY_BUFFER, bufferData.size() * sizeof(glm::vec3), &bufferData[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj.s_ebo_hdl);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, obj.allIndices.size() * sizeof(GLuint),
            &obj.allIndices[0], GL_STATIC_DRAW);

        // vertex positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3) * 2, (void*)0);
        // vertex normals used for colors
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3) * 2, (void*)sizeof(glm::vec3));
        // vertex texture coords
        //glEnableVertexAttribArray(2);
        //glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

	// Create pointer for all indices
	//@todo: IMPLEMENT ME

	// set up buffer object with indices
	//@todo: IMPLEMENT ME

	// create client-side vertex data for 1 triangle
	//@todo: IMPLEMENT ME

	// create vertex buffer object
	//@todo: IMPLEMENT ME

	// create vertex array object
	//@todo: IMPLEMENT ME

	// now, unbind triangle's VBO and VAO
	//@todo: IMPLEMENT ME
}

void DrawNormals(Object obj)
{
    std::vector<glm::vec3> bufferData;

    for (int i = 0; i < obj.allVertices.size(); i++) 
    {
        bufferData.push_back(obj.allVertices[i]);
        bufferData.push_back(obj.colorForNormals);

        bufferData.push_back(obj.allVertexNormals[i]);
        bufferData.push_back(obj.colorForNormals);
    }

    {
        glGenVertexArrays(1, &obj.s_nvao_hdl);
        glGenBuffers(1, &obj.s_nvbo_hdl);
        //glGenBuffers(1, &obj.s_nebo_hdl);

        glBindVertexArray(obj.s_vao_hdl);

        glBindBuffer(GL_ARRAY_BUFFER, obj.s_vbo_hdl);
        glBufferData(GL_ARRAY_BUFFER, bufferData.size() * sizeof(glm::vec3), &bufferData[0], GL_STATIC_DRAW);

        //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj.s_ebo_hdl);
        //glBufferData(GL_ELEMENT_ARRAY_BUFFER, obj.allIndices.size() * sizeof(GLuint),
        //    &obj.allIndices[0], GL_STATIC_DRAW);

        // vertex positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3) * 2, (void*)0);
        // vertex color
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3) * 2, (void*)sizeof(glm::vec3));
        // vertex texture coords
        //glEnableVertexAttribArray(2);
        //glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

        glBindVertexArray(0);
    }
	// create pointer to indices
	//@todo: IMPLEMENT ME

	// set up buffer object with indices
	//@todo: IMPLEMENT ME

	// client-side vertex data for 1 triangle...
	//@todo: IMPLEMENT ME

	// create vertex buffer object
	//@todo: IMPLEMENT ME

	// create vertex array object
	//@todo: IMPLEMENT ME

	// now, unbind triangle's VBO and VAO
	//@todo: IMPLEMENT ME
  
}

bool LoadModel(const std::string& path)
{
	// Read and store data of model
	//@todo: IMPLEMENT ME

    s_scale_factors = glm::vec3(5.f, 5.f, 5.f); // scale parameters
    s_world_position = glm::vec3(0.f, 0.f, -2.f); // position in world frame
    s_angular_displacement = 0.f; // current angular displacement in degrees
    s_orientation_axis = glm::normalize(glm::vec3(0.f, 0.f, 1.f)); // orientation axis

    Object obj; //create a new model Object

    FILE* file = fopen(path.c_str(), "r");
    if (file == NULL) {
        printf("Impossible to open the file !\n");
        return false;
    }

    glm::vec3 min = { 0,0,0 }; //find the minimum x,y,z
    glm::vec3 max = { 0,0,0 };; //find the maximum x,y,z
    int numberOfTriangles = 0;

    while (1) {

        char lineHeader[128];
        // read the first word of the line
        int res = fscanf(file, "%s", lineHeader); //return the number of string
        if (res == EOF)
            break; // EOF = End Of File. Quit the loop.

        if (strcmp(lineHeader, "v") == 0) {
            glm::vec3 vertex;
            fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
            obj.allVertices.push_back(vertex); 

            if (obj.allVertices.size() == 1)
            {
                min = vertex;
                max = vertex;
            }
            else
            {
            //comparing current vertex with minimum value
                if (min.x > vertex.x)
                    min.x = vertex.x;
                if (min.y > vertex.y)
                    min.y = vertex.y;               
                if (min.z > vertex.z)
                    min.z = vertex.z;
            //comparing current vertex with maximum value
                if (max.x < vertex.x)
                    max.x = vertex.x;
                if (max.y < vertex.y)
                    max.y = vertex.y;
                if (max.z < vertex.z)
                    max.z = vertex.z;
            }
            
        }
        //else if (strcmp(lineHeader, "vt") == 0) {
        //    glm::vec2 uv;
        //    fscanf(file, "%f %f\n", &uv.x, &uv.y);
        //    temp_uvs.push_back(uv);
        //
        //}
        //else if (strcmp(lineHeader, "vn") == 0) {
        //    glm::vec3 normal;
        //    fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
        //    temp_normals.push_back(normal);
        //
        //}

        else if (strcmp(lineHeader, "f") == 0) {
            std::string vertex1, vertex2, vertex3;
            //unsigned int vertexIndex[3], /*uvIndex[3],*/ normalIndex[3];

            //int matches = fscanf(file, "%d//%d %d//%d %d//%d\n", &vertexIndex[0], &normalIndex[0], &vertexIndex[1], &normalIndex[1], &vertexIndex[2], &normalIndex[2]);
            char string[128];

            for (int i = 0; i < 3; i++)
            {

                fscanf(file, "%s", string);
               // while (fscanf(file, "%s", string))
               // {
               //     if (str.compare("") != 0)
               //         break;
               // }
                std::string str(string);

                int position = str.find("/");
                if (position != std::string::npos)
                {
                    str = str.substr(0, position);
                }
                float vert = std::stof(str);
                obj.allIndices.push_back(vert);
            }
            

            //if (matches != 6) {
            //    printf("File can't be read by our simple parser : ( Try exporting with other options\n");
            //    return false;
            //}
            //obj.allIndices.push_back(vertexIndex[0]);
            //obj.allIndices.push_back(vertexIndex[1]);
            //obj.allIndices.push_back(vertexIndex[2]);

            numberOfTriangles++;
            //uvIndices.push_back(uvIndex[0]);
            //uvIndices.push_back(uvIndex[1]);
            //uvIndices.push_back(uvIndex[2]);
            //normalIndices.push_back(normalIndex[0]);
            //normalIndices.push_back(normalIndex[1]);
            //normalIndices.push_back(normalIndex[2]);
        }
    }

    // else : parse lineHeader

    obj.numOfTriangles = numberOfTriangles;

    // Calculate vertex normals here
//@todo: IMPLEMENT ME
    for (int vertexPos = 0; vertexPos < obj.allVertices.size(); vertexPos++) //check for each vertex position
    {
        glm::vec3 normal = { 0,0,0 };

        int triangleCount = 0;
        int indexCount = 0;
        for (int i = 0; i < obj.allIndices.size(); i++)
        {
            if (obj.allIndices[i] == (vertexPos + 1) /*plus 1 because vertex index start from 1*/)
            {
                glm::vec3 A = obj.allVertices[obj.allIndices[i] - 1];
                glm::vec3 B = obj.allVertices[obj.allIndices[((i + 1) % 3) + (triangleCount * 3)] - 1];
                glm::vec3 C = obj.allVertices[obj.allIndices[((i + 2) % 3) + (triangleCount * 3)] - 1];

                glm::vec3 N = glm::cross(B - A, C - A); //vector facing out of model?
                float sin_alpha = N.length() / (glm::length(B - A) * glm::length(C - A));
                normal += (glm::normalize(N) * glm::asin(sin_alpha)); //add all the normals of different faces consisting of this vertex position
            }

            indexCount++;

            if (indexCount == 3)
            {
                indexCount = 0;
                triangleCount++;
            }
        }
        obj.allVertexNormals.push_back(glm::normalize(normal)); //normalize the final normal and add to vector of normals
    }

// Calculating Center and unit scale
//@todo: IMPLEMENT ME
//find center of model
    obj.center = (min + max) / 2.0f;
//Calculate unit scale of model - the axis with the largest scale
    float xScale = max.x - min.x;
    float yScale = max.y - min.y;
    float zScale = max.z - min.z;

    if (xScale > yScale)
    {
        obj.unitScale = xScale;

        if (zScale > xScale)
            obj.unitScale = zScale;
    }
    else //yScale > xScale
    {
        obj.unitScale = yScale;

        if (zScale > yScale)
            obj.unitScale = zScale;
    }
 

// Offset object to center
//@todo: IMPLEMENT ME
//translate vertices of model to centre - can use matrix or just manually minus off obj center
    for (auto& vertices : obj.allVertices)
        vertices -= obj.center;



// Resize object to (1, 1, 1)
//@todo: IMPLEMENT ME
//Normalize the model scale to 1 by 1 by 1. - Can use matrix or just manual
    for (auto& vertices : obj.allVertices)
        vertices /= obj.unitScale;




	DrawObject(obj);
    DrawNormals(obj);

    ObjectList.push_back(obj);

	return true;
}

/*
Initialize GLFW 3
Initialize GLEW
Initialize GL
Create log file
*/
void Init() {
  CreateLogFile(s_log_filename);

  glfwSetErrorCallback(ErrorCallbackForGLFW); // register GLFW error callback ...
  if (!glfwInit()) {  // start GL context and OS window using GLFW ...
    std::cerr << "ERROR: Could not start GLFW3" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  std::string gstr("GLFW version: "); gstr += glfwGetVersionString();
  WriteToFile(s_log_filename, gstr.c_str());

  // specify modern GL version ...
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  // our framebuffer will consist of 24-bit depthbuffer and double buffered 24-bit RGB color buffer
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
  glfwWindowHint(GLFW_RED_BITS, 8);
  glfwWindowHint(GLFW_GREEN_BITS, 8);
  glfwWindowHint(GLFW_BLUE_BITS, 8);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

  glfwWindowHint(GLFW_SAMPLES, 4); // set multisample antialiasing sample count to 4

  GLFWmonitor* mon = nullptr;
  if (s_window_fullsize) { // full-screen window
    mon = glfwGetPrimaryMonitor();
    GLFWvidmode const* video_mode = glfwGetVideoMode(mon);
    s_window_width = video_mode->width; s_window_height = video_mode->height;
  }
  s_window_handle = glfwCreateWindow(s_window_width, s_window_height, "Model Loader", mon, nullptr);

  if (!s_window_handle) {
    std::cerr << "ERROR: Could not open window with GLFW3" << std::endl;
    glfwTerminate();
    std::exit(EXIT_FAILURE);
  }
  glfwSetWindowSizeCallback(s_window_handle, WindowResizeCallbackForGLFW);
  glfwMakeContextCurrent(s_window_handle);

  // don't wait for device's vertical sync for front and back buffers to be swapped
  glfwSwapInterval(0);

  glfwSetKeyCallback(s_window_handle, KeyCallbackForGLFW);

  // write title to window with current fps ...
  UpdateTime(s_frames_per_second, 1.0);
  std::stringstream sstr;
  sstr << std::fixed << std::setprecision(2) << "Model Loader: " << s_frames_per_second;
  glfwSetWindowTitle(s_window_handle, sstr.str().c_str());

  glewExperimental = GL_TRUE; // start GLEW extension library
  GLenum err = glewInit();
  if (GLEW_OK != err) {
    std::cerr << "Error: Unable to initialize GLEW " << glewGetErrorString(err) << std::endl;
    glfwDestroyWindow(s_window_handle); // destroy window and corresponding GL context
    glfwTerminate();
    std::exit(EXIT_FAILURE);
  }

  if (!GLEW_VERSION_4_3) { // check support for core GL 4.3 ...
    std::cout << "Error: Cannot access GL 4.3 API" << std::endl;
    glfwDestroyWindow(s_window_handle);
    glfwTerminate();
    std::exit(EXIT_FAILURE);
  }
  LogGLParams(s_log_filename);

  // set viewport - we'll use the entire window as viewport ...
  GLsizei fbwidth, fbheight;
  glfwGetFramebufferSize(s_window_handle, &fbwidth, &fbheight);
  glViewport(0, 0, fbwidth, fbheight);

  // compile, link and use shader programs
  //@todo: IMPLEMENT ME
  {
      std::string Fragmentpath = "./Shaders/FragmentShader.frag";
      std::string Vertexpath = "./Shaders/vertexShader.vert";

      char* FragmentString;
      char* VertexString;
      get_shader_file_contents(Fragmentpath, FragmentString);
      get_shader_file_contents(Vertexpath, VertexString);

  
     GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
     glShaderSource(VertexShaderID, 1, &VertexString, NULL);
         glCompileShader(VertexShaderID);
     std::string diag_msg;
     if (!CheckShaderCompileStatus(VertexShaderID, diag_msg))
         std::cout << "VertexShader unable to compile: " << diag_msg << std::endl;
  
      
      GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(FragmentShaderID, 1, &FragmentString, NULL);
          glCompileShader(FragmentShaderID);
      if (!CheckShaderCompileStatus(FragmentShaderID, diag_msg))
          std::cout << "FragmentShader unable to compile: " << diag_msg << std::endl;


      s_shaderpgm_hdl = glCreateProgram();
      glAttachShader(s_shaderpgm_hdl, VertexShaderID);
      glAttachShader(s_shaderpgm_hdl, FragmentShaderID);
      glLinkProgram(s_shaderpgm_hdl);
      if (!CheckShaderProgramLinkStatus(s_shaderpgm_hdl, diag_msg))
          std::cout << "ShaderProgram unable to link: " << diag_msg << std::endl;

      glDeleteShader(VertexShaderID);
      glDeleteShader(FragmentShaderID);
  }

  // Get all models
  {
      std::string path = "./assets";
      for (const auto& entry : std::filesystem::directory_iterator(path))
      {
          std::cout << entry.path() << std::endl;
          std::string pathstring = entry.path().u8string();

          //load model
          LoadModel(pathstring);

          int num = pathstring.find_last_of("\\\\") + 1;
          std::string objectName = pathstring.substr(num).c_str();
          objectName = objectName.substr(0, objectName.find_last_of("."));

          //std::cout << objectName.c_str() << std::endl;

          char* array = new char[objectName.size() + 1];
          strcpy(array, objectName.c_str());
          modelList.push_back(array);
      }
  }

}

void Draw() {
  glClearColor(1.f, 1.f, 0.f, 1.f); // clear drawing surface with this color
  glEnable(GL_DEPTH_TEST);
  //// cull back-faced or clockwise oriented primitives
  //glEnable(GL_CULL_FACE);
  //glFrontFace(GL_CCW);
  //glCullFace(GL_BACK);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear framebuffer (color and depth)

                            // write window title with current fps ...
  std::stringstream sstr;
  sstr << std::fixed << std::setprecision(2) << "Model Loader: " << s_frames_per_second;
  glfwSetWindowTitle(s_window_handle, sstr.str().c_str());

  // load vertex and fragment programs to corresponding processors
	//@todo: IMPLEMENT ME
  glUseProgram(s_shaderpgm_hdl);

  // load "model-to-world-to-view-to-clip" matrix to uniform variable named "uMVP" in vertex shader
	//@todo: IMPLEMENT ME
  glUniformMatrix4fv(glGetUniformLocation(s_shaderpgm_hdl, "uMVP"), 1, GL_FALSE, glm::value_ptr(s_mvp_xform));

  // transfer vertices from server (GPU) buffers to vertex processer which must (at the very least)
  // compute the clip frame coordinates followed by assembly into triangles ...
  // bind VAO of triangle mesh
  //@todo: IMPLEMENT ME
  // bind index buffer object
  //@todo: IMPLEMENT ME
  // Draw triangles
  //@todo: IMPLEMENT ME
  if (model >= 0)
  {
      //{
      //    static float arr[18] = { -0.5, 0, -1, 0.0, 0.0, 0.0, 
      //                    0.5, 0.0, -1, 0.0, 0.0, 0.0,
      //                    0.0 , 0.5, -1, 0.0, 0.0, 0.0 };
      //    static GLuint buffer;
      //    static int value = 1;
      //
      //    if (value == 1)
      //    {
      //        glGenBuffers(1, &buffer);
      //        glBindBuffer(GL_ARRAY_BUFFER, buffer);
      //        glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), arr, GL_STATIC_DRAW);
      //
      //        // vertex positions
      //        glEnableVertexAttribArray(0);
      //        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3) * 2, (void*)0);
      //        // vertex color
      //        glEnableVertexAttribArray(1);
      //        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3) * 2, (void*)sizeof(glm::vec3));
      //
      //        value = 0;
      //    }
      //
      //    {
      //        glBindBuffer(GL_ARRAY_BUFFER, buffer);
      //        glDrawArrays(GL_TRIANGLES, 0, 3);
      //    }
      //}


      glBindVertexArray(ObjectList[model].s_vao_hdl);
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ObjectList[model].s_ebo_hdl);
      glDrawElements(ObjectList[model].ModelDrawMethod, ObjectList[model].allIndices.size(), GL_UNSIGNED_INT, 0);

      // programming tip: always reset state - application will be easier to debug ...
      //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glBindVertexArray(0);
     // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glUseProgram(0);

      if (drawNormals)
      {
          // use program to draw normals here
          //@todo: IMPLEMENT ME
          glBindVertexArray(ObjectList[model].s_vao_hdl);
          glDrawArrays(ObjectList[model].NormalDrawMethod, 0, ObjectList[model].allVertexNormals.size());
      }
  }
}

void Update() {
  // time between previous and current frame
  double delta_time = UpdateTime(s_frames_per_second, 1.0);
  // Update other events like input handling 
  glfwPollEvents();
  if (s_window_close_flag) {
    glfwSetWindowShouldClose(s_window_handle, 1);
  }
  // Update all game components using previously computed delta_time ...

    ImGui_ImplGlfwGL3_NewFrame();

  // to compute perspective transform, get the current width and height of viewport (in case
  // the user has changed its size) ...
  GLsizei fb_width, fb_height;
  glfwGetFramebufferSize(s_window_handle, &fb_width, &fb_height);

  float objPos[3] = { s_world_position.x, s_world_position.y, s_world_position.z };
  float objScale[3] = { s_scale_factors.x, s_scale_factors.y, s_scale_factors.z };
  float objAngle = s_angular_displacement;
  float objAxis[3] = { s_orientation_axis.x, s_orientation_axis.y, s_orientation_axis.z };
  float objColor[3] = { colorForAllVertices.x, colorForAllVertices.y, colorForAllVertices.z };

  // Draw your GUI here
  //@todo: IMPLEMENT ME
  {
      ImGui::Begin("Modifiers");
      if (ImGui::TreeNode("Scene"))
      {
          ImGui::Text("Projection");
          {
              ImGui::DragFloat("FOV degrees", &fov, 0.2f, 0.1f, 180.0f);
              ImGui::DragFloat("Near Plane", &near, 0.2f, 0.1f);
              ImGui::DragFloat("Far Plane", &far, 0.2f, 0.1f);
          }
          ImGui::TreePop();
      }
      if (ImGui::TreeNode("Object"))
      {
          //Model Selection
          ImGui::Combo("Model", &model, &modelList[0], modelList.size());

          ImGui::DragFloat3("Scale", glm::value_ptr(s_scale_factors), 2);
          ImGui::DragFloat3("Translate", glm::value_ptr(s_world_position), 2);
          ImGui::DragFloat("Rotate X axis", &s_angular_displacement);
          ImGui::Checkbox("Draw Normals", &drawNormals);
          ImGui::TreePop();
      }
      ImGui::End();
  }

  // compute view and projection transforms once for entire scene ...
  //@todo: IMPLEMENT ME

  // compute model-view-projection transformation matrix (this was covered in CS 250) ...
  //@todo: IMPLEMENT ME
  glm::mat4 modelMatrix = glm::mat4(1.0f);
  modelMatrix = glm::translate(modelMatrix, s_world_position);
  modelMatrix = glm::rotate(modelMatrix, glm::radians(s_angular_displacement), s_orientation_axis);
  modelMatrix = glm::scale(modelMatrix, s_scale_factors);

  s_proj_mtx = glm::perspective(glm::radians(fov), (float)(s_window_width / s_window_height), near, far);

  s_mvp_xform = s_proj_mtx * s_view_mtx * modelMatrix;

  // render your GUI
  //@todo: IMPLEMENT ME
  {
      ImGui::Render(); //rendering the imgui window
  }

  // put the stuff we've been drawing onto the display


  glfwSwapBuffers(s_window_handle);
}

/*
Return resources acquired by GLFW, GLEW, OpenGL context back to system ...
*/
void Cleanup() {
  // delete all vbo's attached to s_vao_hdl
  GLint max_vtx_attrib = 0;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vtx_attrib);

  for (auto& model : ObjectList)
  {
      glBindVertexArray(model.s_vao_hdl);
      for (int i = 0; i < max_vtx_attrib; ++i) {
          GLuint vbo_handle = 0;
          glGetVertexAttribIuiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vbo_handle);
          if (vbo_handle > 0) {
              glDeleteBuffers(1, &vbo_handle);
          }
      }
      glBindVertexArray(model.s_nvao_hdl);
      for (int i = 0; i < max_vtx_attrib; ++i) {
          GLuint vbo_handle = 0;
          glGetVertexAttribIuiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vbo_handle);
          if (vbo_handle > 0) {
              glDeleteBuffers(1, &vbo_handle);
          }
      }

      glDeleteBuffers(1, &model.s_vao_hdl);
      glDeleteBuffers(1, &model.s_ebo_hdl);
      glDeleteBuffers(1, &model.s_nvao_hdl);
      glDeleteBuffers(1, &model.s_nebo_hdl);
  }


  glDeleteProgram(s_shaderpgm_hdl);
  glfwDestroyWindow(s_window_handle);
  glfwTerminate();
}

/*
Template function that writes the values of one or more parameters to file specified by first parameter
Function will ensure that file is closed before returning!!!
*/
template <typename... Args>
bool WriteToFile(std::string file_name, Args&&... args) {
  std::ofstream ofs(file_name.c_str(), std::ofstream::app);
  if (!ofs) {
    std::cerr << "ERROR: could not open log file " << file_name << " for writing" << std::endl;
    return false;
  }
  int dummy[sizeof...(Args)] = { (ofs << std::forward<Args>(args) << ' ', 0)... };
  ofs << std::endl;
  ofs.close();
  return true;
}

/*
Start a new log file with current time and date timestamp followed by timestamp of application build ...
Function will ensure that file is closed before returning!!!
*/
bool CreateLogFile(std::string file_name) {
  std::ofstream ofs(file_name.c_str(), std::ofstream::out);
  if (!ofs) {
    std::cerr << "ERROR: could not open log file " << file_name << " for writing" << std::endl;
    return false;
  }

  std::time_t curr_time = time(nullptr); // get current time
  ofs << "OpenGL Application Log File - local time: " << std::ctime(&curr_time); // convert current time to C-string format
  ofs << "Build version: " << __DATE__ << " " << __TIME__ << std::endl << std::endl;
  ofs.close();
  return true;
}

/*
Logs GL parameters ...
*/
void LogGLParams(std::string file_name) {
  GLenum param_enums[] = {
    GL_VENDOR,                    // 0
    GL_RENDERER,
    GL_VERSION,
    GL_SHADING_LANGUAGE_VERSION,  // 3

    GL_MAJOR_VERSION,             // 4
    GL_MINOR_VERSION,
    GL_MAX_ELEMENTS_VERTICES,
    GL_MAX_ELEMENTS_INDICES,
    GL_MAX_GEOMETRY_OUTPUT_VERTICES,
    GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
    GL_MAX_CUBE_MAP_TEXTURE_SIZE,
    GL_MAX_DRAW_BUFFERS,
    GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
    GL_MAX_TEXTURE_IMAGE_UNITS,
    GL_MAX_TEXTURE_SIZE,
    GL_MAX_VARYING_FLOATS,
    GL_MAX_VERTEX_ATTRIBS,
    GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
    GL_MAX_VERTEX_UNIFORM_COMPONENTS, // 18

    GL_MAX_VIEWPORT_DIMS,         // 19

    GL_STEREO                     // 20
  };
  char const* param_names[] = {
    "GL_VENDOR",                    // 0
    "GL_RENDERER",
    "GL_VERSION",
    "GL_SHADING_LANGUAGE_VERSION",  // 3

    "GL_MAJOR_VERSION",             // 4
    "GL_MINOR_VERSION",
    "GL_MAX_ELEMENTS_VERTICES",
    "GL_MAX_ELEMENTS_INDICES",
    "GL_MAX_GEOMETRY_OUTPUT_VERTICES",
    "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS",
    "GL_MAX_CUBE_MAP_TEXTURE_SIZE",
    "GL_MAX_DRAW_BUFFERS",
    "GL_MAX_FRAGMENT_UNIFORM_COMPONENTS",
    "GL_MAX_TEXTURE_IMAGE_UNITS",
    "GL_MAX_TEXTURE_SIZE",
    "GL_MAX_VARYING_FLOATS",
    "GL_MAX_VERTEX_ATTRIBS",
    "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS",
    "GL_MAX_VERTEX_UNIFORM_COMPONENTS", // 18

    "GL_MAX_VIEWPORT_DIMS",         // 19

    "GL_STEREO"                     // 20
  };
  // C-strings for 1st four parameters
  WriteToFile(file_name, "GL version information and context parameters:");
  int i = 0;
  for (i = 0; i < 4; ++i) {
    WriteToFile(file_name, param_names[i], reinterpret_cast<char const*>(glGetString(param_enums[i])));
  }

  // one integer for next set of fifteen parameters
  for (; i < 19; ++i) {
    GLint val;
    glGetIntegerv(param_enums[i], &val);
    WriteToFile(file_name, param_names[i], val);
  }

  // two integers for next parameter
  GLint dim[2];
  glGetIntegerv(param_enums[19], dim);
  WriteToFile(file_name, param_names[19], dim[0], dim[1]);

  // bool for next parameter
  GLboolean flag;
  glGetBooleanv(param_enums[20], &flag);
  WriteToFile(file_name, param_names[20], static_cast<GLint>(flag));

  WriteToFile(file_name, "-----------------------------");
}

/*
This error callback is specifically called whenever GLFW encounters an error.
GLFW supplies an error code and a human-readable description that is written to standard output ...
*/
void ErrorCallbackForGLFW(int error, char const* description) {
  std::cerr << "GLFW Error id: " << error << " | description: " << description << std::endl;
}

/*
This callback function is called when the window is resized ...
*/
void WindowResizeCallbackForGLFW(GLFWwindow* window, int width, int height) {
  s_window_width = width; s_window_height = height;
  // Update any perspective matrices used here since aspect ratio might have changed
  // ...
  // Update viewport
  GLsizei fbwidth, fbheight;
  glfwGetFramebufferSize(window, &fbwidth, &fbheight);
  glViewport(0, 0, fbwidth, fbheight);
}

/*
This callback function is called whenever a keyboard key is pressed or released ...
*/
void KeyCallbackForGLFW(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    s_window_close_flag = true;
  }
}

/*
This function is first called in Init() and once each game loop by Update(). It uses GLFW's time functions to:
1. compute interval in seconds between each frame and return this value
2. compute the frames per second every "int_fps_calc" seconds - the default
value of parameter int_fps_calc is 1.0
*/
double UpdateTime(double &fps, double int_fps_calc) {
  // determine time (in seconds) between previous and current frame ...
  static double prev_time = glfwGetTime();
  double curr_time = glfwGetTime();
  double delta_time = curr_time - prev_time; // time between frames
  prev_time = curr_time;

  // fps calculations
  static double count = 0.0; // number of game loop iterations
  static double start_time = glfwGetTime();
  double elapsed_time = curr_time - start_time;

  ++count;

  // Update fps at least every 10 seconds ...
  int_fps_calc = (int_fps_calc < 0.0) ? 0.0 : int_fps_calc;
  int_fps_calc = (int_fps_calc > 10.0) ? 10.0 : int_fps_calc;
  if (elapsed_time >= int_fps_calc) {
    fps = count / elapsed_time;
    start_time = curr_time;
    count = 0.0;
  }
  return delta_time;
}

void get_shader_file_contents(const std::string& shader, char*& content)
{
  FILE *file;
  size_t count = 0;

  if (shader.c_str() != NULL)
  {
    fopen_s(&file, shader.c_str(), "rt");
    if (file != NULL)
    {
      fseek(file, 0, SEEK_END);
      count = ftell(file);
      rewind(file);

      if (count > 0)
      {
        content = (char *)malloc(sizeof(char) * (count + 1));
        count = fread(content, sizeof(char), count, file);
        content[count] = '\0';
      }

      fclose(file);
    }
    else
    {
      std::cout << "nothing on file";
    }
  }
}

/*
1. Check compile status of shader source.
2. If the shader source has not successfully compiled, then print any diagnostic messages and return false to caller.
3. Otherwise, return true.
*/
static bool CheckShaderCompileStatus(GLuint shader_hdl, std::string diag_msg) {
  GLint result;
  glGetShaderiv(shader_hdl, GL_COMPILE_STATUS, &result);
  if (GL_FALSE == result) {
    GLint log_len;
    glGetShaderiv(shader_hdl, GL_INFO_LOG_LENGTH, &log_len);
    if (log_len > 0) {
      char *error_log_str = new GLchar[log_len];
      GLsizei written_log_len;
      glGetShaderInfoLog(shader_hdl, log_len, &written_log_len, error_log_str);
      diag_msg = error_log_str;
      delete[] error_log_str;
    }
    return false;
  }
  return true;
}

/*
1. Check link status of program.
2. If the program has not successfully linked, then print any diagnostic messages and return false to caller.
3. Otherwise, return true.
*/
static bool CheckShaderProgramLinkStatus(GLuint program_hdl, std::string diag_msg) {
  GLint result;
  glGetProgramiv(program_hdl, GL_LINK_STATUS, &result);
  if (GL_FALSE == result) {
    GLint log_len;
    glGetProgramiv(program_hdl, GL_INFO_LOG_LENGTH, &log_len);
    if (log_len > 0) {
      char *error_log_str = new GLchar[log_len];
      GLsizei written_log_len;
      glGetProgramInfoLog(program_hdl, log_len, &written_log_len, error_log_str);
      diag_msg = error_log_str;
      delete[] error_log_str;
    }
    return false;
  }
  return true;
}