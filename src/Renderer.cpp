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
#include "Skins.h"
#include "Noise.h"

namespace oc {

namespace {

const char* CHUNK_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aShade;
layout(location = 3) in float aSky;
layout(location = 4) in float aLight;
uniform mat4 uProjView;
uniform vec3 uOffset;
out vec2 vUV;
out float vShade;
out float vSky;
out float vLight;
out float vDist;
void main() {
    vec3 p = aPos + uOffset;
    gl_Position = uProjView * vec4(p, 1.0);
    vUV = aUV;
    vShade = aShade;
    vSky = aSky;
    vLight = aLight;
    vDist = length(p);
}
)";

const char* CHUNK_FS = R"(#version 330 core
in vec2 vUV;
in float vShade;
in float vSky;
in float vLight;
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
    float cave = min(0.40, uSun);
    float sunPart = vSky > 1.5 ? 1.0 : mix(cave, uSun, vSky);
    float blk = pow(clamp(vLight, 0.0, 1.0), 1.25);
    vec3 light = max(vec3(sunPart), blk * vec3(1.0, 0.83, 0.60));  // Fackellicht ist warm
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

const char* DOME_VS = R"(#version 330 core
uniform mat4 uInvVP;
out vec3 vDir;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);  // Dreieck, das den Bildschirm deckt
    vec4 w = uInvVP * vec4(p * 2.0 - 1.0, 1.0, 1.0);
    vDir = w.xyz / w.w;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* DOME_FS = R"(#version 330 core
in vec3 vDir;
uniform vec3 uHorizon;
uniform vec3 uZenith;
uniform vec3 uSunDir;
uniform vec3 uMoonDir;
uniform vec3 uDuskCol;
uniform float uDusk;
uniform float uSunGlow;
uniform float uNight;
out vec4 FragColor;
void main() {
    vec3 d = normalize(vDir);
    float h = d.y;
    vec3 col = mix(uHorizon, uZenith, pow(clamp(h, 0.0, 1.0), 0.55));
    float s = max(dot(d, uSunDir), 0.0);
    col += vec3(1.0, 0.85, 0.55) * (pow(s, 5.0) * 0.18 + pow(s, 48.0) * 0.45) * uSunGlow;
    vec2 hd = normalize(d.xz + vec2(1e-4)), sd = normalize(uSunDir.xz + vec2(1e-4));
    float az = max(dot(hd, sd), 0.0);
    float band = exp(-pow((h - 0.02) / 0.26, 2.0)) * pow(az, 1.6);
    col = mix(col, uDuskCol, clamp(uDusk * band, 0.0, 1.0) * 0.85);
    float m = max(dot(d, uMoonDir), 0.0);
    col += vec3(0.30, 0.38, 0.62) * pow(m, 20.0) * 0.30 * uNight;
    FragColor = vec4(col, 1.0);
}
)";

const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* SPRITE_FS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec4 uTint;
out vec4 FragColor;
void main() {
    vec4 t = texture(uTex, vUV);
    FragColor = vec4(t.rgb * uTint.rgb, t.a * uTint.a);
}
)";

const char* STAR_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aInfo;  // Groesse, Helligkeit, Phase
uniform mat4 uMVP;
uniform float uTime;
uniform float uScale;
uniform float uAlpha;
out vec4 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = aInfo.x * uScale;
    float tw = 0.78 + 0.22 * sin(uTime * (1.3 + aInfo.z * 2.2) + aInfo.z * 40.0);
    vec3 tint = aInfo.z < 0.15 ? vec3(0.75, 0.82, 1.0) : (aInfo.z > 0.88 ? vec3(1.0, 0.86, 0.68) : vec3(1.0));
    vColor = vec4(tint, aInfo.y * tw * uAlpha);
}
)";

const char* STAR_FS = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
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
uniform sampler2D uView;
uniform sampler2D uSkin;
uniform int uTexMode;
out vec4 FragColor;
void main() {
    if (vUV.x < 0.0) { FragColor = vColor; return; }
    vec4 t;
    if (uTexMode == 1) t = texture(uAtlas, vUV);
    else if (uTexMode == 2) t = texture(uView, vUV);
    else if (uTexMode == 3) t = texture(uSkin, vUV);
    else t = texture(uGui, vUV);
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

bool Renderer::init(const TextureStore& textures) {
    if (!chunkShader_.build(CHUNK_VS, CHUNK_FS, "chunk")) return false;
    if (!colorShader_.build(COLOR_VS, COLOR_FS, "color")) return false;
    if (!hudShader_.build(HUD_VS, HUD_FS, "hud")) return false;
    if (!domeShader_.build(DOME_VS, DOME_FS, "dome")) return false;
    if (!spriteShader_.build(SPRITE_VS, SPRITE_FS, "sprite")) return false;
    if (!starShader_.build(STAR_VS, STAR_FS, "star")) return false;

    // Texturatlas
    const std::vector<uint8_t>& px = textures.atlasPixels();
    glGenTextures(1, &atlas_);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_PX, ATLAS_PX, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);  // Stufen bluten nicht ueber Kachelgrenzen (8px-Bloecke)
    glGenerateMipmap(GL_TEXTURE_2D);

    // Skins der Figuren
    for (int i = 0; i < SKIN_COUNT; ++i) {
        const SkinInfo& si = skinInfo(static_cast<SkinId>(i));
        const std::vector<uint8_t>& sp = textures.skin(static_cast<SkinId>(i)).rgba;
        glGenTextures(1, &skinTex_[i]);
        glBindTexture(GL_TEXTURE_2D, skinTex_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, si.width, si.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, sp.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

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

    // Himmelstexturen: Sonne, Mondphasen, Lichtkranz
    {
        auto makeTex = [](GLuint& tex, int w, int h, const std::vector<uint8_t>& px, GLint filter) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };
        makeTex(sunTex_, SUN_PX, SUN_PX, sunPixels(), GL_NEAREST);
        makeTex(moonTex_, MOON_PX * MOON_PHASES, MOON_PX, moonPixels(), GL_NEAREST);
        makeTex(glowTex_, GLOW_PX, GLOW_PX, glowPixels(), GL_LINEAR);
        glGenVertexArrays(1, &spriteVao_);
        glGenBuffers(1, &spriteVbo_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    }

    // Sonne/Mond (2 Quads), dynamisch
    glGenVertexArrays(1, &skyVao_);
    glGenBuffers(1, &skyVbo_);
    glBindVertexArray(skyVao_);
    glBindBuffer(GL_ARRAY_BUFFER, skyVbo_);
    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    // Sterne: Position, Groesse, Helligkeit, Phase
    {
        std::vector<float> stars;
        for (const StarDef& st : generateStars(900)) {
            stars.insert(stars.end(), {st.dir.x * 90.0f, st.dir.y * 90.0f, st.dir.z * 90.0f, st.size, st.brightness, st.phase});
        }
        starCount_ = static_cast<int>(stars.size() / 6);
        glGenVertexArrays(1, &starVao_);
        glGenBuffers(1, &starVbo_);
        glBindVertexArray(starVao_);
        glBindBuffer(GL_ARRAY_BUFFER, starVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(stars.size() * sizeof(float)), stars.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
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

void Renderer::syncTextures(TextureStore& textures) {
    if (!initialized_) return;
    if (textures.atlasDirty()) {
        glBindTexture(GL_TEXTURE_2D, atlas_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ATLAS_PX, ATLAS_PX, GL_RGBA, GL_UNSIGNED_BYTE, textures.atlasPixels().data());
        glGenerateMipmap(GL_TEXTURE_2D);
        textures.clearAtlasDirty();
    }
    for (int i = 0; i < SKIN_COUNT; ++i) {
        const SkinId id = static_cast<SkinId>(i);
        if (!textures.skinDirty(id)) continue;
        const Image& img = textures.skin(id);
        glBindTexture(GL_TEXTURE_2D, skinTex_[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.w, img.h, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
        textures.clearSkinDirty(id);
    }
}

void Renderer::shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    destroySharedQuadIndexBuffer();
    if (atlas_) glDeleteTextures(1, &atlas_);
    if (guiTex_) glDeleteTextures(1, &guiTex_);
    if (viewTex_) glDeleteTextures(1, &viewTex_);
    itemMesh_.release();
    for (int i = 0; i < SKIN_COUNT; ++i) {
        skinMesh_[i].release();
        skinHurtMesh_[i].release();
        if (skinTex_[i]) glDeleteTextures(1, &skinTex_[i]);
    }
    GLuint vaos[] = {skyVao_, starVao_, lineVao_, hudVao_, spriteVao_};
    GLuint vbos[] = {skyVbo_, starVbo_, lineVbo_, hudVbo_, spriteVbo_};
    glDeleteVertexArrays(5, vaos);
    glDeleteBuffers(5, vbos);
    GLuint texs[] = {sunTex_, moonTex_, glowTex_};
    glDeleteTextures(3, texs);
    domeShader_.release();
    spriteShader_.release();
    starShader_.release();
    chunkShader_.release();
    colorShader_.release();
    hudShader_.release();
}

void Renderer::render(const World& world, const FrameInfo& f) {
    glViewport(0, 0, f.width, f.height);

    const float day = daylight(f.timeOfDay);
    const float sh = sunHeight(f.timeOfDay);

    // Verlauf: Horizont hell und blass, Zenit tiefer; nachts fast schwarz mit blauem Hauch
    const glm::vec3 horDay(0.70f, 0.83f, 0.98f), zenDay(0.27f, 0.52f, 0.90f);
    const glm::vec3 horNight(0.025f, 0.035f, 0.085f), zenNight(0.004f, 0.007f, 0.028f);
    SkyColors sc;
    sc.horizon = glm::mix(horNight, horDay, day);
    sc.zenith = glm::mix(zenNight, zenDay, day);
    sc.dusk = glm::vec3(1.00f, 0.52f, 0.26f) * (0.45f + 0.55f * day);
    sc.duskAmt = std::exp(-(sh / 0.30f) * (sh / 0.30f));
    // Rundum leichte Roetung am Horizont (Nebel passt dazu), stark nur in Richtung der Sonne (im Himmelsshader)
    glm::vec3 sky = glm::mix(sc.horizon, sc.dusk, sc.duskAmt * 0.30f);

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

    if (!f.underwater && !f.underLava) drawSky(f, sc);
    drawWorld(world, f, fogColor, sunLevel);
    if (f.hud.show) {
        drawSelection(f);
    }
    drawHud(f);
}

void Renderer::drawSky(const FrameInfo& f, const SkyColors& sc) {
    const glm::mat4 viewRot = glm::mat4(glm::mat3(f.view));
    const glm::mat4 vp = f.proj * viewRot;
    const float angle = f.timeOfDay * 2.0f * PI;
    const float sh = sunHeight(f.timeOfDay);
    const glm::vec3 sunDir = glm::normalize(glm::vec3(std::cos(angle), std::sin(angle), 0.30f));
    const glm::vec3 moonDir = -sunDir;
    const float night = 1.0f - smooth01(-0.25f, 0.05f, sh);
    const float sunUp = smooth01(-0.18f, 0.10f, sh);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // Himmelsverlauf mit Abendrot und Glanz um die Sonne
    domeShader_.use();
    domeShader_.setMat4("uInvVP", glm::inverse(vp));
    domeShader_.setVec3("uHorizon", sc.horizon);
    domeShader_.setVec3("uZenith", sc.zenith);
    domeShader_.setVec3("uSunDir", sunDir);
    domeShader_.setVec3("uMoonDir", moonDir);
    domeShader_.setVec3("uDuskCol", sc.dusk);
    domeShader_.setFloat("uDusk", sc.duskAmt);
    domeShader_.setFloat("uSunGlow", sunUp);
    domeShader_.setFloat("uNight", night);
    glBindVertexArray(skyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_BLEND);

    // Sterne: funkeln, nur nachts
    if (night > 0.01f) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_PROGRAM_POINT_SIZE);
        const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));
        starShader_.use();
        starShader_.setMat4("uMVP", vp * rot);
        starShader_.setFloat("uTime", f.time);
        starShader_.setFloat("uScale", std::max(1.0f, std::floor(static_cast<float>(f.height) / 540.0f) + 0.0f));
        starShader_.setFloat("uAlpha", night);
        glBindVertexArray(starVao_);
        glDrawArrays(GL_POINTS, 0, starCount_);
    }

    // Quad zur Himmelsrichtung dir mit halber Kantenlaenge size, UV-Ausschnitt u0..u1
    auto drawSprite = [&](GLuint tex, const glm::vec3& dir, float size, float u0, float u1, const glm::vec4& tint) {
        const glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up = glm::cross(right, dir);
        const glm::vec3 c = dir * 100.0f;
        const glm::vec3 p[4] = {c - right * size - up * size, c + right * size - up * size, c - right * size + up * size,
                                c + right * size + up * size};
        const float uv[4][2] = {{u0, 1.0f}, {u1, 1.0f}, {u0, 0.0f}, {u1, 0.0f}};
        float q[20];
        for (int i = 0; i < 4; ++i) {
            q[i * 5 + 0] = p[i].x;
            q[i * 5 + 1] = p[i].y;
            q[i * 5 + 2] = p[i].z;
            q[i * 5 + 3] = uv[i][0];
            q[i * 5 + 4] = uv[i][1];
        }
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(q), q);
        spriteShader_.setMat4("uMVP", vp);
        spriteShader_.setVec4("uTint", tint);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        spriteShader_.setInt("uTex", 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    };

    spriteShader_.use();
    // Sonne: tief am Horizont groesser und orange
    {
        const float low = 1.0f - smooth01(0.0f, 0.45f, sh);
        const glm::vec3 warm = glm::mix(glm::vec3(1.0f), glm::vec3(1.0f, 0.60f, 0.32f), low);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        drawSprite(glowTex_, sunDir, 46.0f * (1.0f + 0.5f * low), 0.0f, 1.0f, glm::vec4(warm * glm::vec3(1.0f, 0.88f, 0.60f), 0.55f * sunUp));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawSprite(sunTex_, sunDir, 13.0f * (1.0f + 0.25f * low), 0.0f, 1.0f, glm::vec4(warm, 1.0f));
    }
    // Mond mit Phase und blassem Schein
    {
        const int ph = ((f.moonPhase % MOON_PHASES) + MOON_PHASES) % MOON_PHASES;
        const float fullness = ph == 0 ? 1.0f : (ph == 1 || ph == 7) ? 0.7f : (ph == 2 || ph == 6) ? 0.4f : (ph == 3 || ph == 5) ? 0.2f : 0.0f;
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        drawSprite(glowTex_, moonDir, 30.0f, 0.0f, 1.0f, glm::vec4(0.55f, 0.65f, 0.95f, 0.30f * fullness * night));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawSprite(moonTex_, moonDir, 10.0f, static_cast<float>(ph) / MOON_PHASES, static_cast<float>(ph + 1) / MOON_PHASES,
                   glm::vec4(1.0f));
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_PROGRAM_POINT_SIZE);
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
    if (!f.entities) return;
    chunkShader_.setVec3("uOffset", glm::vec3(0.0f));
    chunkShader_.setInt("uCutout", 1);
    glActiveTexture(GL_TEXTURE0);

    // Gegenstaende am Boden: Blockatlas
    if (!f.entities->items.empty()) {
        itemMesh_.upload(f.entities->items);
        chunkShader_.setVec3("uTint", glm::vec3(1.0f));
        glBindTexture(GL_TEXTURE_2D, atlas_);
        itemMesh_.draw();
        drawnQuads_ += itemMesh_.quadCount();
    }
    // Figuren: je Skin eine Textur, getroffene rot getoent
    for (int i = 0; i < SKIN_COUNT; ++i) {
        glBindTexture(GL_TEXTURE_2D, skinTex_[i]);
        if (!f.entities->skin[i].empty()) {
            skinMesh_[i].upload(f.entities->skin[i]);
            chunkShader_.setVec3("uTint", glm::vec3(1.0f));
            skinMesh_[i].draw();
            drawnQuads_ += skinMesh_[i].quadCount();
        }
        if (!f.entities->skinHurt[i].empty()) {
            skinHurtMesh_[i].upload(f.entities->skinHurt[i]);
            chunkShader_.setVec3("uTint", glm::vec3(1.0f, 0.38f, 0.38f));
            skinHurtMesh_[i].draw();
            drawnQuads_ += skinHurtMesh_[i].quadCount();
        }
    }
    chunkShader_.setVec3("uTint", glm::vec3(1.0f));
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glBindVertexArray(0);
}

void Renderer::flushHud(const FrameInfo& f) {
    if (hudBatch_.verts.empty()) return;
    hudShader_.use();
    hudShader_.setVec2("uScreen", glm::vec2(static_cast<float>(f.width), static_cast<float>(f.height)));
    hudShader_.setInt("uAtlas", 0);
    hudShader_.setInt("uGui", 1);
    hudShader_.setInt("uView", 2);
    hudShader_.setInt("uSkin", 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, guiTex_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, viewTex_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, previewSkin_ >= 0 ? skinTex_[previewSkin_] : skinTex_[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(hudVao_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(hudBatch_.verts.size() * sizeof(HudVertex)),
                 hudBatch_.verts.data(), GL_STREAM_DRAW);
    for (const HudBatch::Cmd& cmd : hudBatch_.cmds) {
        hudShader_.setInt("uTexMode", cmd.tex);
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
    if (st.editor.open && st.editor.view && st.editor.view->valid()) {
        const Image& img = *st.editor.view;
        if (!viewTex_) {
            glGenTextures(1, &viewTex_);
            glBindTexture(GL_TEXTURE_2D, viewTex_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glBindTexture(GL_TEXTURE_2D, viewTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    }
    previewSkin_ = st.editor.open ? st.editor.previewSkin : -1;
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
