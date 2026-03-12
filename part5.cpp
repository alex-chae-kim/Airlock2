// template based on material from learnopengl.com
#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <chrono>

struct Vec3 {
    float x, y, z;
};

struct MeshObject {
    std::vector<float> vertices;
    GLuint VAO = 0;
    GLuint VBO = 0;
    unsigned int numVertices = 0;

    glm::vec3 baseTranslation{0.0f, 0.0f, 0.0f};
    glm::vec3 baseRotation{0.0f, 0.0f, 0.0f};
    glm::vec3 baseScale{1.0f, 1.0f, 1.0f};
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// animation state
bool gAnimateOrbit = false;
bool gSpacePressedLastFrame = false;
float gOrbitAngle = 0.0f;
const float gOrbitSpeed = 0.01f;

std::string readShaderFile(const std::string& filename) {
    std::ifstream shader(filename);
    if (!shader.is_open()) {
        std::cout << "Failed to open shader." << std::endl;
    }
    std::string line;
    std::string s;
    while (getline(shader, line)) {
        s.append(line);
        s.append("\n");
    }
    shader.close();
    return s;
}

void centerAndNormalizeMesh(std::vector<float>& vertices)
{
    if (vertices.empty()) return;

    float minX = vertices[0], maxX = vertices[0];
    float minY = vertices[1], maxY = vertices[1];
    float minZ = vertices[2], maxZ = vertices[2];

    // find bounding box
    for (size_t i = 0; i < vertices.size(); i += 6) {
        float x = vertices[i];
        float y = vertices[i + 1];
        float z = vertices[i + 2];

        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
        minZ = std::min(minZ, z);
        maxZ = std::max(maxZ, z);
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;

    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;

    float maxDim = std::max(sizeX, std::max(sizeY, sizeZ));

    // scale largest dimension to len 1.0
    float scale = 1.0f / maxDim;

    for (size_t i = 0; i < vertices.size(); i += 6) {
        vertices[i] = (vertices[i] - centerX) * scale;
        vertices[i + 1] = (vertices[i + 1] - centerY) * scale;
        vertices[i + 2] = (vertices[i + 2] - centerZ) * scale;
    }
}

bool loadOBJ(const std::string& filename, std::vector<float>& vertices)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filename << std::endl;
        return false;
    }

    std::vector<Vec3> positions;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            Vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (prefix == "f") {
            std::vector<int> faceIndices;
            std::string token;

            while (iss >> token) {
                std::stringstream tokenStream(token);
                std::string indexStr;
                std::getline(tokenStream, indexStr, '/'); // only read up to first / 

                if (indexStr.empty()) {
                    std::cerr << "Bad face token in OBJ: " << token << std::endl;
                    return false;
                }

                int objIndex = std::stoi(indexStr);
                faceIndices.push_back(objIndex - 1); // faces use 1 based indexing
            }

            if (faceIndices.size() < 3) {
                std::cerr << "Face has fewer than 3 vertices." << std::endl;
                return false;
            }

            // triangulate each polygon as a fan:
            // (0,1,2), (0,2,3), (0,3,4), ...
            for (size_t i = 1; i + 1 < faceIndices.size(); i++) {
                int tri[3] = {
                    faceIndices[0],
                    faceIndices[i],
                    faceIndices[i + 1]
                };
                
                for (int k = 0; k < 3; k++) {
                    int idx = tri[k];
                    Vec3 p = positions[idx];

                    // position
                    vertices.push_back(p.x);
                    vertices.push_back(p.y);
                    vertices.push_back(p.z);

                    // default color
                    vertices.push_back(1.0f);
                    vertices.push_back(1.0f);
                    vertices.push_back(1.0f);
                }
            }
        }
    }

    return true;
}

bool setupMeshObject(MeshObject& obj, const std::string& objFile)
{
    if (!loadOBJ(objFile, obj.vertices)) {
        return false;
    }

    centerAndNormalizeMesh(obj.vertices);
    obj.numVertices = static_cast<unsigned int>(obj.vertices.size() / 6);

    glGenVertexArrays(1, &obj.VAO);
    glGenBuffers(1, &obj.VBO);

    glBindVertexArray(obj.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, obj.VBO);
    glBufferData(GL_ARRAY_BUFFER, obj.vertices.size() * sizeof(float), obj.vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

glm::mat4 buildLocalModelMatrix(const MeshObject& obj)
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, obj.baseTranslation);
    model = glm::rotate(model, obj.baseRotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, obj.baseRotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, obj.baseRotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, obj.baseScale);
    return model;
}

glm::mat4 buildAxisRotationAroundLine(const glm::vec3& pointOnAxis, const glm::vec3& axisDir, float angle)
{
glm::mat4 M(1.0f);
M = glm::translate(M, pointOnAxis);
M = glm::rotate(M, angle, glm::normalize(axisDir));
M = glm::translate(M, -pointOnAxis);
return M;
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif


    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "viewGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);


    // // glew: load all OpenGL function pointers
    glewInit();

    // read in vertex and fragment shader
    std::string vertexShaderCode = readShaderFile("shaders/source.vs");
    std::string fragmentShaderCode = readShaderFile("shaders/source.fs");

    const char* vertexShaderSource = vertexShaderCode.c_str();
    const char* fragmentShaderSource = fragmentShaderCode.c_str();

    // build and compile our shader program
    // ------------------------------------
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // link shaders
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);


    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    MeshObject obj1, obj2;

    if (!setupMeshObject(obj1, "objs/skull.obj")) {
        std::cerr << "Could not load first OBJ.\n";
        glfwTerminate();
        return -1;
    }

    if (!setupMeshObject(obj2, "objs/cow.obj")) {
        std::cerr << "Could not load second OBJ.\n";
        glfwTerminate();
        return -1;
    }

    obj1.baseTranslation = glm::vec3(-0.5f, -0.3f, 0.0f);
    obj2.baseTranslation = glm::vec3( 0.5f, 0.4f, 0.0f);

    glEnable(GL_DEPTH_TEST);
    GLint modelViewLoc = glGetUniformLocation(shaderProgram, "uModelView");

    // uncomment this call to draw in wireframe polygons.
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        processInput(window);

        glm::mat4 view(1.0f);

        glm::mat4 localModel1 = buildLocalModelMatrix(obj1);
        glm::mat4 localModel2 = buildLocalModelMatrix(obj2);

        // object centers in world space before orbit rotation
        glm::vec3 center1 = glm::vec3(localModel1 * glm::vec4(0, 0, 0, 1));
        glm::vec3 center2 = glm::vec3(localModel2 * glm::vec4(0, 0, 0, 1));

        glm::vec3 axis = center2 - center1;
        glm::vec3 midpoint = 0.5f * (center1 + center2);

        glm::mat4 orbitTransform(1.0f);
        if (glm::length(axis) > 1e-6f) {
            orbitTransform = buildAxisRotationAroundLine(midpoint, axis, gOrbitAngle);
        }

        glm::mat4 model1 = orbitTransform * localModel1;
        glm::mat4 model2 = orbitTransform * localModel2;

        glm::mat4 modelView1 = view * model1;
        glm::mat4 modelView2 = view * model2;


        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // draw first obj
        glBindVertexArray(obj1.VAO);
        glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, glm::value_ptr(modelView1));
        glDrawArrays(GL_TRIANGLES, 0, obj1.numVertices);

        // draw second obj
        glBindVertexArray(obj2.VAO);
        glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, glm::value_ptr(modelView2));
        glDrawArrays(GL_TRIANGLES, 0, obj2.numVertices);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }


    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &obj1.VAO);
    glDeleteBuffers(1, &obj1.VBO);
    glDeleteVertexArrays(1, &obj2.VAO);
    glDeleteBuffers(1, &obj2.VBO);
    glDeleteProgram(shaderProgram);


    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}


// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    bool spacePressedNow = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    if (spacePressedNow && !gSpacePressedLastFrame) {
        gAnimateOrbit = !gAnimateOrbit;
    }
    gSpacePressedLastFrame = spacePressedNow;

    if (gAnimateOrbit) {
        gOrbitAngle += gOrbitSpeed;
    }
}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
