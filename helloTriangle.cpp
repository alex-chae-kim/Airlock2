// template based on material from learnopengl.com
// additional code by Alex Kim and Matthew Lozito 
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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Vec3 {
    float x, y, z;
};

struct Vec2 {
    float u, v;
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);


// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
bool gShowDepth = false;
bool gToggleDepthLastFrame = false;

// interaction state
glm::vec3 gTranslation(0.0f, 0.0f, 0.0f);
glm::vec3 gRotation(0.0f, 0.0f, 0.0f); // in radians
glm::vec3 gScale(1.0f, 1.0f, 1.0f);
glm::vec3 gLightPos(0.0f, 1.0f, 1.0f);

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
    for (size_t i = 0; i < vertices.size(); i += 11) {
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

    for (size_t i = 0; i < vertices.size(); i += 11) {
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
    std::vector<Vec2> texcoords;
    std::vector<Vec3> normals;
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
        else if (prefix == "vt") {
            Vec2 tc;
            iss >> tc.u >> tc.v;
            texcoords.push_back(tc);
        }
        else if (prefix == "vn") {
            Vec3 n;
            iss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (prefix == "f") {
            struct FaceIndices {int pos, tex, norm;};
            std::vector<FaceIndices> faceIndices;
            std::string token;

            while (iss >> token) {
                std::stringstream tokenStream(token);
                std::string position, texture, normal;
                std::getline(tokenStream, position, '/');
                std::getline(tokenStream, texture, '/');
                std::getline(tokenStream, normal, '/');

                if (position.empty()) {
                    std::cerr << "Bad face token in OBJ: " << token << std::endl;
                    return false;
                }

                int objIndex = std::stoi(position);
                int texIndex = texture.empty() ? 0 : std::stoi(texture);
                int normalIndex = normal.empty() ? 0 : std::stoi(normal);
                faceIndices.push_back({objIndex - 1, texIndex - 1, normalIndex - 1});
            }

            if (faceIndices.size() < 3) {
                std::cerr << "Face has fewer than 3 vertices." << std::endl;
                return false;
            }

            // triangulate each polygon as a fan:
            // (0,1,2), (0,2,3), (0,3,4), ...
            for (size_t i = 1; i + 1 < faceIndices.size(); i++) {
                FaceIndices tri[3] = {
                    faceIndices[0],
                    faceIndices[i],
                    faceIndices[i + 1]
                };
                
                for (int k = 0; k < 3; k++) {
                    FaceIndices idx = tri[k];
                    Vec3 p = positions[idx.pos];
                    Vec3 n = normals[idx.norm];

                    // UV — fall back to (0,0) if no texcoords present
                    float u = 0.0f, v = 0.0f;
                    if (idx.tex >= 0 && idx.tex < (int)texcoords.size()) {
                        u = texcoords[idx.tex].u;
                        v = texcoords[idx.tex].v;
                    }

                    // position
                    vertices.push_back(p.x);
                    vertices.push_back(p.y);
                    vertices.push_back(p.z);

                    // default color
                    vertices.push_back(1.0f);
                    vertices.push_back(1.0f);
                    vertices.push_back(1.0f);

                    // UV
                    vertices.push_back(u);
                    vertices.push_back(v);

                    // normals
                    vertices.push_back(n.x);
                    vertices.push_back(n.y);
                    vertices.push_back(n.z);
                }
            }
        }
    }

    return true;
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
    std::string vertexShaderCode = readShaderFile("shaders/phong.vs");
    const char* vertexShaderSource = vertexShaderCode.c_str();
    std::string fragmentShaderCode = readShaderFile("shaders/phong.fs");
    const char* fragmentShaderSource = fragmentShaderCode.c_str();

    std::string vertexLightShaderCode = readShaderFile("shaders/source.vs");
    const char* vertexLightShaderSource = vertexLightShaderCode.c_str();
    std::string fragmentLightShaderCode = readShaderFile("shaders/source.fs");
    const char* fragmentLightShaderSource = fragmentLightShaderCode.c_str();

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

    // light vertex shader
    unsigned int vertexLightShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexLightShader, 1, &vertexLightShaderSource, NULL);
    glCompileShader(vertexLightShader);
    // check for shader compile errors
    glGetShaderiv(vertexLightShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexLightShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::LIGHT::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // light fragment shader
    unsigned int fragmentLightShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentLightShader, 1, &fragmentLightShaderSource, NULL);
    glCompileShader(fragmentLightShader);
    // check for shader compile errors
    glGetShaderiv(fragmentLightShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentLightShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::LIGHT::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
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

    unsigned int shaderLightProgram = glCreateProgram();
    glAttachShader(shaderLightProgram, vertexLightShader);
    glAttachShader(shaderLightProgram, fragmentLightShader);
    glLinkProgram(shaderLightProgram);
    // check for linking errors
    glGetProgramiv(shaderLightProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderLightProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::LIGHT::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexLightShader);
    glDeleteShader(fragmentLightShader);


    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    std::vector<float> originalVertices;
    std::vector<float> lightVertices;

    if (!loadOBJ("objs/skull.obj", originalVertices)) {
        std::cerr << "Could not load OBJ geometry." << std::endl;
        glfwTerminate();
        return -1;
    }
    if (!loadOBJ("objs/cube.obj", lightVertices)) {
        std::cerr << "Could not load OBJ geometry." << std::endl;
        glfwTerminate();
        return -1;
    }
    centerAndNormalizeMesh(originalVertices);
    // centerAndNormalizeMesh(lightVertices);
    // std::vector<float> transformedLightVertices = lightVertices;
    unsigned int numVertices = originalVertices.size() / 11;
    unsigned int numLightVertices = lightVertices.size() / 11;


    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);


    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, originalVertices.size() * sizeof(float), originalVertices.data(), GL_DYNAMIC_DRAW);


    // position attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // color attributes
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // normal attributes
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // UV attributes
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);


    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 


    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0); 


    unsigned int VBO2, VAO2;
    glGenVertexArrays(1, &VAO2);
    glGenBuffers(1, &VBO2);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO2);


    glBindBuffer(GL_ARRAY_BUFFER, VBO2);
    glBufferData(GL_ARRAY_BUFFER, lightVertices.size() * sizeof(float), lightVertices.data(), GL_DYNAMIC_DRAW);


    // position attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // color attributes
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // normal attributes
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // UV attributes
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);


    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0);


    // Texture setup
    unsigned int handleTex;
    glGenTextures(1, &handleTex);
    glBindTexture(GL_TEXTURE_2D, handleTex);

    // Wrapping & filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Try loading from disk first; fall back to a procedural white texture
    stbi_set_flip_vertically_on_load(true);
    int texW, texH, texChannels;
    unsigned char* texData = stbi_load("textures/skull.png", &texW, &texH, &texChannels, 0);
    if (texData) {
        GLenum fmt = (texChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, texW, texH, 0, fmt, GL_UNSIGNED_BYTE, texData);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(texData);
        std::cout << "Loaded texture: textures/skull.png (" << texW << "x" << texH << ")" << std::endl;
    } else {
        // Procedural white texture
        const int CB = 8;
        unsigned char checker[CB * CB * 3];
        for (int row = 0; row < CB; ++row) {
            for (int col = 0; col < CB; ++col) {
                int idx = (row * CB + col) * 3;
                checker[idx] = 255; checker[idx+1] = 255; checker[idx+2] = 255;
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, CB, CB, 0, GL_RGB, GL_UNSIGNED_BYTE, checker);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "No texture file found — using procedural checkerboard." << std::endl;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    // -----------------------------------------------------------------------

    glEnable(GL_DEPTH_TEST);
    GLint modelViewLoc = glGetUniformLocation(shaderProgram, "uMVP");
    GLint showDepthLoc = glGetUniformLocation(shaderProgram, "uShowDepth");
    GLint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    GLint lightPosLoc = glGetUniformLocation(shaderProgram, "uLightPos");
    GLint cameraPosLoc = glGetUniformLocation(shaderProgram, "uCameraPos");
    GLint texLoc = glGetUniformLocation(shaderProgram, "uTex");
    GLint modelViewLocLight = glGetUniformLocation(shaderLightProgram, "uMVP");
    GLint showDepthLocLight = glGetUniformLocation(shaderLightProgram, "uShowDepth");

    double lastFPSTime = glfwGetTime();
    int frameCount = 0;

    // uncomment this call to draw in wireframe polygons.
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


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

        // build light Model matrix
        glm::mat4 lightModel(1.0f);
        lightModel = glm::translate(lightModel, gLightPos);
        lightModel = glm::scale(lightModel, glm::vec3(0.01));

        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),                    // FOV
            (float)SCR_WIDTH / (float)SCR_HEIGHT,   // aspect ratio
            2.0f,                                   // near plane
            4.0f                                    // far plane
        );

        // simple View matrix
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f)); // translated so the camera doesn't get placed inside the object
        glm::mat4 mvp = projection * view * model;
        glm::mat4 lightMvp = projection * view * lightModel;

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // pass depth toggle to shader
        glUniform1i(showDepthLoc, gShowDepth ? 1 : 0);

        glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(lightPosLoc, 1, glm::value_ptr(gLightPos));
        glUniform3f(cameraPosLoc, 0.0f, 0.0f, 3.0f);

        // bind texture to unit 0 and point sampler at it
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, handleTex);
        glUniform1i(texLoc, 0);
        glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
        glDrawArrays(GL_TRIANGLES, 0, numVertices);
        glBindVertexArray(0); // unbind our VA no need to unbind it every time

        // switch to light shader, define uniform locations, and draw
        glUseProgram(shaderLightProgram);
        glUniformMatrix4fv(modelViewLocLight, 1, GL_FALSE, glm::value_ptr(lightMvp));
        glUniform1i(showDepthLocLight, gShowDepth ? 1 : 0);
        glBindVertexArray(VAO2);
        glDrawArrays(GL_TRIANGLES, 0, numLightVertices);
        glBindVertexArray(0);
        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();

        frameCount++;

        // fps logging
        double currentTime = glfwGetTime();
        double elapsed = currentTime - lastFPSTime;

        if (elapsed >= 1.0) {
            double fps = frameCount / elapsed;

            std::cout << "FPS: " << fps << std::endl;

            frameCount = 0;
            lastFPSTime = currentTime;
        }
    }


    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &VAO2);
    glDeleteBuffers(1, &VBO2);
    glDeleteProgram(shaderLightProgram);
    glDeleteTextures(1, &handleTex);

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
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)     gTranslation.z += moveStep;
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)     gTranslation.z -= moveStep;

    // rotation
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gRotation.y += rotStep;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gRotation.y -= rotStep;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) gRotation.x += rotStep;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) gRotation.x -= rotStep;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) gRotation.z += rotStep;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) gRotation.z -= rotStep;

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

    // toggle depth mode
    bool dPressedNow = (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS);
    if (dPressedNow && !gToggleDepthLastFrame) {
        gShowDepth = !gShowDepth;
    }
    gToggleDepthLastFrame = dPressedNow;
}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}