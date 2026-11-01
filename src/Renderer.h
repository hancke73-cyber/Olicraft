#pragma once
#include <glad/gl.h>

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "Shader.h"
#include "World.h"

namespace oc {

struct FrameInfo {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::vec3 camPos{0.0f};
    int width = 1, height = 1;
    float timeOfDay = 0.3f;  // 0..1, 0.25 = Mittag, 0.75 = Mitternacht
    bool underwater = false;
    int renderDistance = 8;  // in Chunks
    bool hasTarget = false;
    glm::ivec3 target{0};
    int hotbarSlot = 0;
    const BlockId* hotbar = nullptr;
    bool showHud = true;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool init();
    void shutdown();
    void render(const World& world, const FrameInfo& f);

    bool saveScreenshot(const std::string& path, int width, int height) const;

    // Stand der Sonne: Hoehe -1..1
    static float sunHeight(float timeOfDay);
    static float daylight(float timeOfDay);

    int drawnChunks() const { return drawnChunks_; }
    uint64_t drawnQuads() const { return drawnQuads_; }

private:
    struct HudVertex {
        float x, y, u, v, r, g, b, a;
    };

    void drawSky(const FrameInfo& f, const glm::vec3& skyColor);
    void drawWorld(const World& world, const FrameInfo& f, const glm::vec3& fogColor, float sunLevel);
    void drawSelection(const FrameInfo& f);
    void drawHud(const FrameInfo& f);

    void hudQuad(float x0, float y0, float x1, float y1, glm::vec4 c);
    void hudTexQuad(const glm::vec2 p[4], const glm::vec2 uv[4], glm::vec4 c);
    void hudBlockIcon(float cx, float cy, float size, BlockId id);
    void flushHud(const FrameInfo& f);

    Shader chunkShader_, colorShader_, hudShader_;
    GLuint atlas_ = 0;

    GLuint skyVao_ = 0, skyVbo_ = 0;      // dynamische Quads fuer Sonne/Mond
    GLuint starVao_ = 0, starVbo_ = 0;
    int starCount_ = 0;
    GLuint lineVao_ = 0, lineVbo_ = 0;    // Auswahlrahmen
    GLuint hudVao_ = 0, hudVbo_ = 0;
    std::vector<HudVertex> hud_;

    int drawnChunks_ = 0;
    uint64_t drawnQuads_ = 0;
    bool initialized_ = false;
};

}  // namespace oc
