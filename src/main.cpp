#include <iostream>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "Camera.h"
#include "Interaction.h"
#include "core/Vec2.h"

// Global application state
Camera g_camera;
PageModel g_page;
InteractionController g_interaction;

// Forward declare callbacks
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static void window_size_callback(GLFWwindow* window, int width, int height);

int main() 
{
    // Initialize subsystems
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "Daruma Simplified", NULL, NULL);
    if (!window) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, false); // false = do not install callbacks automatically
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Setup Inputs
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);

    FilterRegistry::initDefaults();
    
    // Initialize Renderer (requires GL context)
    Renderer renderer;
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    g_camera.setSize(w, h);
    renderer.setSize(w, h);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Clear Background
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render Scene
        // Note: Assuming InteractionController exposes state via a getter or public member.
        // If 'm_state' is private, you will need to add: const InteractionState& state() const { return m_state; }
        // to InteractionController in Interaction.h
        // renderer.render(g_camera, g_page, g_interaction.state()); 

        // Render UI
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    renderer.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    Vec2 mouseWorld = g_camera.screenToWorld(Vec2((float)xpos, (float)ypos));
    g_interaction.onCursorPos(g_page, g_camera, mouseWorld);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;
    
    double x, y; glfwGetCursorPos(window, &x, &y);
    Vec2 mouseWorld = g_camera.screenToWorld(Vec2((float)x, (float)y));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) g_interaction.onMouseDown(g_page, g_camera, mouseWorld);
        else if (action == GLFW_RELEASE) g_interaction.onMouseUp();
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    if (ImGui::GetIO().WantCaptureMouse) return;
    double x, y; glfwGetCursorPos(window, &x, &y);
    g_interaction.onScroll(g_page, g_camera, (float)yoffset, Vec2((float)x, (float)y));
}

static void window_size_callback(GLFWwindow* window, int width, int height) {
    g_camera.setSize(width, height);
}
