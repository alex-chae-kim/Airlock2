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

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);


// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// interaction state
glm::vec3 gTranslation(0.0f, 0.0f, -1.5f);
glm::vec3 gRotation(0.0f, 0.0f, 0.0f); // in radians
glm::vec3 gScale(1.0f, 1.0f, 1.0f);

bool gUseCPUTransform = false;
bool gSpacePressedLastFrame = false;

const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "uniform mat4 uModelView;\n"
    "out vec3 vertexColor;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = uModelView * vec4(aPos, 1.0);\n"
    "   vertexColor = aColor;"
    "}\0";
const char *fragmentShaderSource = "#version 330 core\n"
    "in vec3 vertexColor;\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(vertexColor.x, vertexColor.y, vertexColor.z, 1.0f);\n"
    "}\n\0";

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

void updateCPUTransformedVertices(const std::vector<float>& originalVertices, std::vector<float>& transformedVertices, const glm::mat4& modelView) {
    transformedVertices.resize(originalVertices.size());
    for (size_t i = 0; i < originalVertices.size(); i += 6) {
        glm::vec4 p(originalVertices[i], originalVertices[i + 1], originalVertices[i + 2], 1.0f);
        glm::vec4 transformed = modelView * p;

        transformedVertices[i] = transformed.x;
        transformedVertices[i + 1] = transformed.y;
        transformedVertices[i + 2] = transformed.z;

        // copy color unchanged
        transformedVertices[i + 3] = originalVertices[i + 3];
        transformedVertices[i + 4] = originalVertices[i + 4];
        transformedVertices[i + 5] = originalVertices[i + 5];
    }
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
    std::vector<float> originalVertices;

    if (!loadOBJ("cow.obj", originalVertices)) {
        std::cerr << "Could not load OBJ geometry." << std::endl;
        glfwTerminate();
        return -1;
    }
    std::vector<float> transformedVertices = originalVertices;
    unsigned int numVertices = originalVertices.size() / 6;


    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);


    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, originalVertices.size() * sizeof(float), originalVertices.data(), GL_DYNAMIC_DRAW);


    // position attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // color attributes
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);


    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 


    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0); 


    glEnable(GL_DEPTH_TEST);
    GLint modelViewLoc = glGetUniformLocation(shaderProgram, "uModelView");

    double cpuTransformMsAccum = 0.0;
    int cpuTransformFrameCount = 0;

    double cpuUploadMsAccum = 0.0;
    int cpuUploadFrameCount = 0;

    // uncomment this call to draw in wireframe polygons.
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        processInput(window);

        // build Model matrix
        glm::mat4 model(1.0f);
        model = glm::translate(model, gTranslation);
        model = glm::rotate(model, gRotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, gRotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, gRotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, gScale);

        // simple View matrix
        glm::mat4 view(1.0f);
        glm::mat4 modelView = view * model;

        if (gUseCPUTransform) {
            auto t1 = std::chrono::high_resolution_clock::now();
            updateCPUTransformedVertices(originalVertices, transformedVertices, modelView);
            auto t2 = std::chrono::high_resolution_clock::now();

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, transformedVertices.size() * sizeof(float), transformedVertices.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            auto t3 = std::chrono::high_resolution_clock::now();

            cpuTransformMsAccum += std::chrono::duration<double, std::milli>(t2 - t1).count();
            cpuUploadMsAccum += std::chrono::duration<double, std::milli>(t3 - t2).count();
            cpuTransformFrameCount++;
            cpuUploadFrameCount++;
        }

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        if (gUseCPUTransform) {
            // already transformed on CPU, so send identity
            glm::mat4 identity(1.0f);
            glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, glm::value_ptr(identity));
        } else {
            // GPU transforms the original vertices
            glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, glm::value_ptr(modelView));
        }

        glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
        glDrawArrays(GL_TRIANGLES, 0, numVertices);
        // glBindVertexArray(0); // unbind our VA no need to unbind it every time 
 
        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // performance logs
    if (cpuTransformFrameCount > 0) {
        std::cout << "Average CPU transform time per frame: " << (cpuTransformMsAccum / cpuTransformFrameCount) << " ms\n";
        std::cout << "Average CPU->GPU upload time per frame: " << (cpuUploadMsAccum / cpuUploadFrameCount) << " ms\n";
    }


    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
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
    const float moveStep = 0.01f;
    const float rotStep  = 0.02f;
    const float scaleStep = 0.01f;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // translation
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  gTranslation.x -= moveStep;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) gTranslation.x += moveStep;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    gTranslation.y += moveStep;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  gTranslation.y -= moveStep;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)     gTranslation.z += moveStep;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)     gTranslation.z -= moveStep;

    // rotation
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gRotation.y += rotStep;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gRotation.y -= rotStep;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) gRotation.x += rotStep;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) gRotation.x -= rotStep;

    // scaling
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        gScale += glm::vec3(scaleStep);
    }
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        gScale -= glm::vec3(scaleStep);
        gScale.x = std::max(gScale.x, 0.05f);
        gScale.y = std::max(gScale.y, 0.05f);
        gScale.z = std::max(gScale.z, 0.05f);
    }

    // toggle CPU/GPU mode
    bool spacePressedNow = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    if (spacePressedNow && !gSpacePressedLastFrame) {
        gUseCPUTransform = !gUseCPUTransform;
        std::cout << (gUseCPUTransform ? "CPU transform mode\n" : "GPU transform mode\n");
    }
    gSpacePressedLastFrame = spacePressedNow;
}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
