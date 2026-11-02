#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Atlas.h"
#include "GuiTexture.h"
#include "Noise.h"

namespace oc {

namespace {

const char* CHUNK_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aShade;
layout(location = 3) in float aSky;
uniform mat4 uProjView;
uniform vec3 uOffset;
out vec2 vUV;
out float vShade;
out float vSky;
out float vDist;
void main() {
    vec3 p = aPos + uOffset;
    gl_Position = uProjView * vec4(p, 1.0);
    vUV = aUV;
    vShade = aShade;
    vSky = aSky;
    vDist = length(p);
}
)";

const char* CHUNK_FS = R"(#version 330 core
in vec2 vUV;
in float vShade;
in float vSky;
in float vDist;
uniform sampler2D uAtlas;
uniform float uSun;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform int uCutout;
uniform vec3 uTint;
out vec4 FragColor;
void main() {
    vec4 t = texture(uAtlas, vUV);
    if (uCutout == 1 && t.a < 0.5) discard;
    float cave = min(0.26, uSun);
    float light = vSky > 1.5 ? 1.0 : mix(cave, uSun, vSky);
    vec3 c = t.rgb * vShade * light * uTint;
    float f = clamp((vDist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    c = mix(c, uFogColor, f);
    FragColor = vec4(c, uCutout == 1 ? 1.0 : t.a);
}
)";

const char* COLOR_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
uniform float uPointSize;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = uPointSize;
}
)";

const char* COLOR_FS = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)";

const char* HUD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vColor;
void main() {
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)";

const char* HUD_FS = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uAtlas;
uniform sampler2D uGui;
uniform int uUseAtlas;
out vec4 FragColor;
void main() {
    if (vUV.x < 0.0) { FragColor = vColor; return; }
    vec4 t = uUseAtlas == 1 ? texture(uAtlas, vUV) : texture(uGui, vUV);
    if (t.a < 0.1) discard;
    FragColor = vec4(t.rgb * vColor.rgb, t.a * vColor.a);
}
)";

float smooth01(float a, float b, float x) {
    float t = std::min(1.0f, std::max(0.0f, (x - a) / (b - a)));
    return t * t * (3.0f - 2.0f * t);
}

constexpr float PI = 3.14159265358979f;

struct Plane {
    glm::vec4 p;
};

void extractFrustum(const glm::mat4& m, glm::vec4 out[6]) {
    auto row = [&](int r) { return glm::vec4(m[0][r], m[1][r], m[2][r], m[3][r]); };
    out[0] = row(3) + row(0);
    out[1] = row(3) - row(0);
    out[2] = row(3) + row(1);
    out[3] = row(3) - row(1);
    out[4] = row(3) + row(2);
    out[5] = row(3) - row(2);
}

bool aabbVisible(const glm::vec4 planes[6], const glm::vec3& mn, const glm::vec3& mx) {
    for (int i = 0; i < 6; ++i) {
        const glm::vec4& p = planes[i];
        glm::vec3 v(p.x > 0 ? mx.x : mn.x, p.y > 0 ? mx.y : mn.y, p.z > 0 ? mx.z : mn.z);
        if (p.x * v.x + p.y * v.y + p.z * v.z + p.w < 0) return false;
    }
    return true;
}

}  // namespace

Renderer::~Renderer() { shutdown(); }

float Renderer::sunHeight(float t) { return std::sin(t * 2.0f * PI); }
float Renderer::daylight(float t) { return smooth01(-0.12f, 0.22f, sunHeight(t)); }

bool Renderer::init() {
    if (!chunkShader_.build(CHUNK_VS, CHUNK_FS, "chunk")) return false;
    if (!colorShader_.build(COLOR_VS, COLOR_FS, "color")) return false;
    if (!hudShader_.build(HUD_VS, HUD_FS, "hud")) return false;

    // Texturatlas
    std::vector<uint8_t> px = generateAtlasPixels();
    glGenTextures(1, &atlas_);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_PX, ATLAS_PX, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);  // Stufen bluten nicht ueber Kachelgrenzen (8px-Bloecke)
    glGenerateMipmap(GL_TEXTURE_2D);

    // GUI-Textur (Schrift, Symbole)
    {
        std::vector<uint8_t> g = generateGuiPixels();
        glGenTextures(1, &guiTex_);
        glBindTexture(GL_TEXTURE_2D, guiTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GUI_TEX_W, GUI_TEX_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, g.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Sonne/Mond (2 Quads), dynamisch
    glGenVertexArrays(1, &skyVao_);
    glGenBuffers(1, &skyVbo_);
    glBindVertexArray(skyVao_);
    glBindBuffer(GL_ARRAY_BUFFER, skyVbo_);
    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    // Sterne
    {
        std::vector<float> stars;
        for (int i = 0; i < 700; ++i) {
            uint32_t h1 = hash3(i, 1, 0, 77u), h2 = hash3(i, 2, 0, 77u);
            float u = (h1 & 0xFFFFu) / 65535.0f * 2.0f - 1.0f;
            float a = (h2 & 0xFFFFu) / 65535.0f * 2.0f * PI;
            float r = std::sqrt(1.0f - u * u);
            stars.push_back(r * std::cos(a) * 90.0f);
            stars.push_back(u * 90.0f);
            stars.push_back(r * std::sin(a) * 90.0f);
        }
        starCount_ = static_cast<int>(stars.size() / 3);
        glGenVertexArrays(1, &starVao_);
        glGenBuffers(1, &starVbo_);
        glBindVertexArray(starVao_);
        glBindBuffer(GL_ARRAY_BUFFER, starVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(stars.size() * sizeof(float)), stars.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    }

    // Auswahlrahmen (Wuerfelkanten)
    {
        const float e = 0.003f, a = -e, b = 1.0f + e;
        const float v[8][3] = {{a, a, a}, {b, a, a}, {b, a, b}, {a, a, b}, {a, b, a}, {b, b, a}, {b, b, b}, {a, b, b}};
        const int idx[24] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
        std::vector<float> lines;
        for (int i : idx) lines.insert(lines.end(), v[i], v[i] + 3);
        glGenVertexArrays(1, &lineVao_);
        glGenBuffers(1, &lineVbo_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(lines.size() * sizeof(float)), lines.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    }

    // HUD
    glGenVertexArrays(1, &hudVao_);
    glGenBuffers(1, &hudVbo_);
    glBindVertexArray(hudVao_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex), reinterpret_cast<void*>(offsetof(HudVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex), reinterpret_cast<void*>(offsetof(HudVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(HudVertex), reinterpret_cast<void*>(offsetof(HudVertex, r)));
    glBindVertexArray(0);

    sharedQuadIndexBuffer();
    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    destroySharedQuadIndexBuffer();
    if (atlas_) glDeleteTextures(1, &atlas_);
    if (guiTex_) glDeleteTextures(1, &guiTex_);
    entityMesh_.release();
    entityHurtMesh_.release();
    GLuint vaos[] = {skyVao_, starVao_, lineVao_, hudVao_};
    GLuint vbos[] = {skyVbo_, starVbo_, lineVbo_, hudVbo_};
    glDeleteVertexArrays(4, vaos);
    glDeleteBuffers(4, vbos);
    chunkShader_.release();
    colorShader_.release();
    hudShader_.release();
}

void Renderer::render(const World& world, const FrameInfo& f) {
    glViewport(0, 0, f.width, f.height);

    const float day = daylight(f.timeOfDay);
    const float sh = sunHeight(f.timeOfDay);

    glm::vec3 skyDay(0.52f, 0.74f, 0.96f), skyNight(0.012f, 0.018f, 0.05f), dusk(0.95f, 0.50f, 0.28f);
    glm::vec3 sky = glm::mix(skyNight, skyDay, day);
    float duskAmt = std::exp(-(sh / 0.20f) * (sh / 0.20f)) * 0.65f;
    sky = glm::mix(sky, dusk * (0.35f + 0.65f * day), duskAmt);

    glm::vec3 fogColor = sky;
    float sunLevel = 0.22f + 0.78f * day;
    if (f.underLava) {
        fogColor = glm::vec3(0.55f, 0.14f, 0.02f);
        sunLevel = 1.0f;
    } else if (f.underwater) {
        fogColor = glm::vec3(0.04f, 0.18f, 0.34f) * (0.35f + 0.65f * day);
        sunLevel = std::max(0.3f, sunLevel * 0.9f);
    }

    glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    if (!f.underwater && !f.underLava) drawSky(f, sky);
    drawWorld(world, f, fogColor, sunLevel);
    if (f.hud.show) {
        drawSelection(f);
    }
    drawHud(f);
}

void Renderer::drawSky(const FrameInfo& f, const glm::vec3& skyColor) {
    glm::mat4 viewRot = glm::mat4(glm::mat3(f.view));
    glm::mat4 vp = f.proj * viewRot;
    const float angle = f.timeOfDay * 2.0f * PI;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    colorShader_.use();

    // Sterne (nachts)
    float starAlpha = 1.0f - smooth01(-0.25f, 0.05f, sunHeight(f.timeOfDay));
    if (starAlpha > 0.01f) {
        glEnable(GL_PROGRAM_POINT_SIZE);
        glm::mat4 rot = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));
        colorShader_.setMat4("uMVP", vp * rot);
        colorShader_.setVec4("uColor", glm::vec4(1.0f, 1.0f, 1.0f, starAlpha * 0.9f));
        colorShader_.setFloat("uPointSize", 2.0f);
        glBindVertexArray(starVao_);
        glDrawArrays(GL_POINTS, 0, starCount_);
    }

    auto drawDisc = [&](glm::vec3 dir, float size, glm::vec4 color) {
        dir = glm::normalize(dir);
        glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::cross(right, dir);
        glm::vec3 c = dir * 100.0f;
        float q[12];
        glm::vec3 p0 = c - right * size - up * size, p1 = c + right * size - up * size;
        glm::vec3 p2 = c + right * size + up * size, p3 = c - right * size + up * size;
        const glm::vec3 pts[4] = {p0, p1, p3, p2};  // Triangle-Strip
        for (int i = 0; i < 4; ++i) {
            q[i * 3 + 0] = pts[i].x;
            q[i * 3 + 1] = pts[i].y;
            q[i * 3 + 2] = pts[i].z;
        }
        glBindVertexArray(skyVao_);
        glBindBuffer(GL_ARRAY_BUFFER, skyVbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(q), q);
        colorShader_.setMat4("uMVP", vp);
        colorShader_.setVec4("uColor", color);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    };

    glm::vec3 sunDir(std::cos(angle), std::sin(angle), 0.30f);
    drawDisc(sunDir, 14.0f, glm::vec4(1.0f, 0.97f, 0.80f, 1.0f));
    drawDisc(-sunDir, 10.0f, glm::vec4(0.86f, 0.90f, 0.98f, 1.0f));
    (void)skyColor;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void Renderer::drawWorld(const World& world, const FrameInfo& f, const glm::vec3& fogColor, float sunLevel) {
    const glm::mat4 viewRot = glm::mat4(glm::mat3(f.view));
    const glm::mat4 projView = f.proj * viewRot;
    glm::vec4 planes[6];
    extractFrustum(projView, planes);

    const float viewDist = static_cast<float>(f.renderDistance * CHUNK_W);
    float fogStart = viewDist * 0.55f, fogEnd = viewDist * 0.97f;
    if (f.underLava) {
        fogStart = 0.0f;
        fogEnd = 3.5f;
    } else if (f.underwater) {
        fogStart = 1.0f;
        fogEnd = 24.0f;
    }

    chunkShader_.use();
    chunkShader_.setMat4("uProjView", projView);
    chunkShader_.setVec3("uFogColor", fogColor);
    chunkShader_.setFloat("uFogStart", fogStart);
    chunkShader_.setFloat("uFogEnd", fogEnd);
    chunkShader_.setFloat("uSun", sunLevel);
    chunkShader_.setInt("uAtlas", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);

    struct Item {
        float dist2;
        const Chunk* c;
        glm::vec3 offset;
    };
    std::vector<Item> visible;
    visible.reserve(world.chunks().size());
    for (const auto& kv : world.chunks()) {
        const Chunk& c = *kv.second;
        if (!c.meshed) continue;
        if (c.opaque.empty() && c.water.empty()) continue;
        glm::vec3 org(static_cast<float>(c.cx * CHUNK_W) - f.camPos.x, 0.0f - f.camPos.y,
                      static_cast<float>(c.cz * CHUNK_W) - f.camPos.z);
        // Exakter bei grossen Koordinaten: Differenz in double bilden
        org.x = static_cast<float>(static_cast<double>(c.cx) * CHUNK_W - static_cast<double>(f.camPos.x));
        org.z = static_cast<float>(static_cast<double>(c.cz) * CHUNK_W - static_cast<double>(f.camPos.z));
        glm::vec3 mn = org, mx = org + glm::vec3(CHUNK_W, CHUNK_H, CHUNK_W);
        if (!aabbVisible(planes, mn, mx)) continue;
        glm::vec3 center = org + glm::vec3(CHUNK_W * 0.5f, 0.0f, CHUNK_W * 0.5f);
        visible.push_back({center.x * center.x + center.z * center.z, &c, org});
    }
    std::sort(visible.begin(), visible.end(), [](const Item& a, const Item& b) { return a.dist2 < b.dist2; });

    drawnChunks_ = static_cast<int>(visible.size());
    drawnQuads_ = 0;

    // Undurchsichtig (nah nach fern)
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    chunkShader_.setInt("uCutout", 1);
    chunkShader_.setVec3("uTint", glm::vec3(1.0f));
    for (const Item& it : visible) {
        if (it.c->opaque.empty()) continue;
        chunkShader_.setVec3("uOffset", it.offset);
        it.c->opaque.draw();
        drawnQuads_ += it.c->opaque.quadCount();
    }

    drawEntities(f, projView);

    // Wasser (fern nach nah)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    chunkShader_.setInt("uCutout", 0);
    chunkShader_.setVec3("uTint", glm::vec3(1.0f));
    for (auto it = visible.rbegin(); it != visible.rend(); ++it) {
        if (it->c->water.empty()) continue;
        chunkShader_.setVec3("uOffset", it->offset);
        it->c->water.draw();
        drawnQuads_ += it->c->water.quadCount();
    }
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void Renderer::drawSelection(const FrameInfo& f) {
    if (!f.hasTarget) return;
    glm::mat4 viewRot = glm::mat4(glm::mat3(f.view));
    glm::vec3 rel(static_cast<float>(static_cast<double>(f.target.x) - f.camPos.x),
                  static_cast<float>(static_cast<double>(f.target.y) - f.camPos.y),
                  static_cast<float>(static_cast<double>(f.target.z) - f.camPos.z));
    glm::mat4 mvp = f.proj * viewRot * glm::translate(glm::mat4(1.0f), rel);
    colorShader_.use();
    colorShader_.setMat4("uMVP", mvp);
    colorShader_.setVec4("uColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.75f));
    colorShader_.setFloat("uPointSize", 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(lineVao_);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void Renderer::drawEntities(const FrameInfo& f, const glm::mat4& projView) {
    (void)projView;
    if (!f.entityVerts && !f.entityHurtVerts) return;
    chunkShader_.setVec3("uOffset", glm::vec3(0.0f));
    chunkShader_.setInt("uCutout", 1);
    if (f.entityVerts) {
        entityMesh_.upload(*f.entityVerts);
        chunkShader_.setVec3("uTint", glm::vec3(1.0f));
        entityMesh_.draw();
        drawnQuads_ += entityMesh_.quadCount();
    }
    if (f.entityHurtVerts) {
        entityHurtMesh_.upload(*f.entityHurtVerts);
        chunkShader_.setVec3("uTint", glm::vec3(1.0f, 0.38f, 0.38f));
        entityHurtMesh_.draw();
        drawnQuads_ += entityHurtMesh_.quadCount();
    }
    chunkShader_.setVec3("uTint", glm::vec3(1.0f));
    glBindVertexArray(0);
}

void Renderer::flushHud(const FrameInfo& f) {
    if (hudBatch_.verts.empty()) return;
    hudShader_.use();
    hudShader_.setVec2("uScreen", glm::vec2(static_cast<float>(f.width), static_cast<float>(f.height)));
    hudShader_.setInt("uAtlas", 0);
    hudShader_.setInt("uGui", 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, guiTex_);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(hudVao_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(hudBatch_.verts.size() * sizeof(HudVertex)),
                 hudBatch_.verts.data(), GL_STREAM_DRAW);
    for (const HudBatch::Cmd& cmd : hudBatch_.cmds) {
        hudShader_.setInt("uUseAtlas", cmd.tex == HUD_TEX_ATLAS ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(cmd.first), static_cast<GLsizei>(cmd.count));
    }
    glBindVertexArray(0);
}

void Renderer::drawHud(const FrameInfo& f) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    HudState st = f.hud;
    st.underwater = f.underwater;
    st.underLava = f.underLava;
    buildHud(hudBatch_, st, f.width, f.height);
    flushHud(f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

bool Renderer::saveScreenshot(const std::string& path, int width, int height) const {
    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

    const int rowBytes = (width * 3 + 3) & ~3;
    const uint32_t dataSize = static_cast<uint32_t>(rowBytes) * static_cast<uint32_t>(height);
    const uint32_t fileSize = 54 + dataSize;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    auto w16 = [&](uint16_t v) { f.put(char(v & 0xFF)); f.put(char(v >> 8)); };
    auto w32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) f.put(char((v >> (8 * i)) & 0xFF)); };
    f.put('B');
    f.put('M');
    w32(fileSize);
    w32(0);
    w32(54);
    w32(40);
    w32(static_cast<uint32_t>(width));
    w32(static_cast<uint32_t>(height));  // positiv = Zeilen von unten nach oben (wie glReadPixels)
    w16(1);
    w16(24);
    w32(0);
    w32(dataSize);
    w32(2835);
    w32(2835);
    w32(0);
    w32(0);
    std::vector<uint8_t> row(static_cast<size_t>(rowBytes), 0);
    for (int y = 0; y < height; ++y) {
        const uint8_t* src = &rgb[static_cast<size_t>(y) * width * 3];
        for (int x = 0; x < width; ++x) {
            row[x * 3 + 0] = src[x * 3 + 2];
            row[x * 3 + 1] = src[x * 3 + 1];
            row[x * 3 + 2] = src[x * 3 + 0];
        }
        f.write(reinterpret_cast<const char*>(row.data()), rowBytes);
    }
    return static_cast<bool>(f);
}

}  // namespace oc
