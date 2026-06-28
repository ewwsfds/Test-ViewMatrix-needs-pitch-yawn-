#include <windows.h>
#include <cstdint>
#include <tlhelp32.h>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>


struct FloatPair
{
    float pitch, yawn;
};


FloatPair getPitch_Yawn(HANDLE hProcess, uintptr_t address)
{
    FloatPair result = { 0, 0 };

    ReadProcessMemory(
        hProcess,
        (LPCVOID)address,
        &result,
        sizeof(FloatPair),
        nullptr
    );

    return result;
}


glm::vec3 getCamera_pos(HANDLE hProcess, uintptr_t address)
{
    glm::vec3 result;

    ReadProcessMemory(
        hProcess,
        (LPCVOID)address,
        &result,
        sizeof(glm::vec3),
        nullptr
    );

    return result;
}

HANDLE OpenProcessByPid(DWORD pid)
{
    HANDLE hProcess = OpenProcess(
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
        FALSE,
        pid
    );

    if (hProcess == NULL)
    {
        std::cout << "Failed to open process. Error: " << GetLastError() << std::endl;
        return NULL;
    }

    std::cout << "Process opened successfully!" << std::endl;
    return hProcess;
}

uintptr_t GetModuleBaseAddress(DWORD pid, const std::wstring& moduleName)
{
    uintptr_t baseAddress = 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        std::cout << "Failed to create snapshot. Error: " << GetLastError() << std::endl;
        return 0;
    }

    MODULEENTRY32W modEntry;
    modEntry.dwSize = sizeof(modEntry);

    if (Module32FirstW(snapshot, &modEntry))
    {
        do
        {
            if (moduleName == modEntry.szModule)
            {
                baseAddress = (uintptr_t)modEntry.modBaseAddr;
                break;
            }
        } while (Module32NextW(snapshot, &modEntry));
    }

    CloseHandle(snapshot);

    return baseAddress;
}



DWORD pid = 24780;

uintptr_t base = GetModuleBaseAddress(pid, L"game.exe");

uintptr_t Pitch_address = 0x00007FF6476FBCE0 +(4*3);
uintptr_t Camera_address = 0x00007FF6476FBCE0;



FloatPair pitchYaw;
glm::vec3 camera;



// ---------------- CAMERA ----------------


// ---------------- ENTITY ----------------
struct Entity
{
    glm::vec3 position;
    glm::vec3 rotation; // in degrees (x=pitch, y=yaw, z=roll)
    glm::vec3 scale;
};

// ---------------- FORWARD (Y UP WORLD) ----------------
// OpenGL standard: Y is UP, Z is forward (right-handed)

glm::vec3 getForward_when_up_Y(float pitch, float yaw)
{
    float radPitch = glm::radians(pitch);
    float radYaw = glm::radians(yaw);

    glm::vec3 forward;
    forward.x = cos(radYaw) * cos(radPitch);
    forward.y = sin(radPitch);
    forward.z = sin(radYaw) * cos(radPitch);

    return glm::normalize(forward);
}

// ---------------- FORWARD (Z UP WORLD) ----------------
// Used in some engines where Z is UP

glm::vec3 getForward_when_up_Z(float pitch, float yaw)
{
    float radPitch = glm::radians(pitch);
    float radYaw = glm::radians(yaw);

    glm::vec3 forward;
    forward.x = cos(radYaw) * cos(radPitch);
    forward.y = -sin(radYaw) * cos(radPitch);
    forward.z = sin(radPitch);

    return glm::normalize(forward);
}

// ---------------- MODEL MATRIX ----------------
glm::mat4 getModel(const Entity& gameObject)
{
    glm::mat4 model = glm::mat4(1.0f);

    // translation
    model = glm::translate(model, gameObject.position);

    // rotation (Euler angles)
    model = glm::rotate(model, glm::radians(gameObject.rotation.x), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(gameObject.rotation.y), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(gameObject.rotation.z), glm::vec3(0, 0, 1));

    // scale
    model = glm::scale(model, gameObject.scale);

    return model;
}



void init(Entity& enemy)
{
    enemy.position = glm::vec3(0.0f, 0.0f, 0.0f);
    enemy.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    enemy.rotation = glm::vec3(0.0f, 0.0f, 0.0f);

}


glm::mat4 getView(glm::vec3 Position, glm::vec3 Forward, glm::vec3 Up)
{
    return glm::lookAt(
        Position,
        Position + Forward,
        Up
    );
}


glm::mat4 getProjection(float fov, float width, float height)
{
    float aspectRatio = width / height;

    return glm::perspective(
        glm::radians(fov),
        aspectRatio,
        0.1f,
        100.0f
    );
}



Entity enemy;









// Vertex shader
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// Fragment shader
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0, 0.5, 0.2, 1.0);
}
)";

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

int main()
{
    HANDLE process = OpenProcessByPid(pid);

    if (process == NULL)
    {
        std::cout << "Could not open process." << std::endl;
        return 1;
    }


    // Init GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Test OpenGL Triangle", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    
    // Load GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    // Build vertex shader
    int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Build fragment shader
    int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Shader program
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Triangle vertices
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    init(enemy);



    // Render loop
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);

        pitchYaw = getPitch_Yawn(process, Pitch_address);
        camera = getCamera_pos(process, Camera_address);


        glm::vec3 forward = getForward_when_up_Y(pitchYaw.pitch, pitchYaw.yawn);


        glm::mat4 view = getView(camera, forward, glm::vec3(0, 1, 0));

        glm::mat4 projection = getProjection(90.0f, 800.0f, 600.0f);

        glm::mat4 model = getModel(enemy);

        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
