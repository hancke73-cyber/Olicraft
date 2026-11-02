#pragma once
#include <glad/gl.h>

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Block.h"
#include "Entities.h"
#include "Hud.h"
#include "Shader.h"
#include "Sky.h"
#include "TextureStore.h"
#include "World.h"

namespace oc {

struct FrameInfo {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::vec3 camPos{0.0f};
    int width = 1, height = 1;
    float timeOfDay = 0.3f;  // 0..1, 0.25 = Mittag, 0.75 = Mitternacht
    int moonPhase = 0;       // 0 Vollmond .. 7
    float time = 0.0f;       // Sekunden, fuer Funkeln der Sterne
    bool underwater = false;
    bool underLava = false;
    int renderDistance = 8;  // in Chunks
    bool hasTarget = false;
    glm::ivec3 target{0};
    HudState hud;
    // Entity-Geometrie (kamerarelative Weltkoordinaten), vom Spiel mit EntityManager::buildMesh erzeugt
    const EntityBatches* entities = nullptr;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool init(const TextureStore& textures);
    // Laedt geaenderte Texturen (Atlas, Skins) auf die Grafikkarte
    void syncTextures(TextureStore& textures);
    void shutdown();
    void render(const World& world, const FrameInfo& f);

    bool saveScreenshot(const std::string& path, int width, int height) const;

    // Stand der Sonne: Hoehe -1..1
    static float sunHeight(float timeOfDay);
    static float daylight(float timeOfDay);

    int drawnChunks() const { return drawnChunks_; }
    uint64_t drawnQuads() const { return drawnQuads_; }

private:
    struct SkyColors {
        glm::vec3 horizon, zenith, dusk;
        float duskAmt = 0.0f;
    };
    void drawSky(const FrameInfo& f, const SkyColors& sc);
    void drawWorld(const World& world, const FrameInfo& f, const glm::vec3& fogColor, float sunLevel);
    void drawSelection(const FrameInfo& f);
    void drawHud(const FrameInfo& f);
    void drawEntities(const FrameInfo& f, const glm::mat4& projView);

    void flushHud(const FrameInfo& f);

    Shader chunkShader_, colorShader_, hudShader_, domeShader_, spriteShader_, starShader_;
    GLuint sunTex_ = 0, moonTex_ = 0, glowTex_ = 0;
    GLuint spriteVao_ = 0, spriteVbo_ = 0;
    GLuint atlas_ = 0;
    GLuint guiTex_ = 0;
    GLuint viewTex_ = 0;     // Bild der Editor-Leinwand
    int previewSkin_ = -1;   // Skin fuer die Editor-Vorschau
    GpuMesh itemMesh_;
    GpuMesh skinMesh_[SKIN_COUNT], skinHurtMesh_[SKIN_COUNT];
    GLuint skinTex_[SKIN_COUNT] = {};
    HudBatch hudBatch_;

    GLuint skyVao_ = 0, skyVbo_ = 0;      // dynamische Quads fuer Sonne/Mond
    GLuint starVao_ = 0, starVbo_ = 0;
    int starCount_ = 0;
    GLuint lineVao_ = 0, lineVbo_ = 0;    // Auswahlrahmen
    GLuint hudVao_ = 0, hudVbo_ = 0;

    int drawnChunks_ = 0;
    uint64_t drawnQuads_ = 0;
    bool initialized_ = false;
};

}  // namespace oc
