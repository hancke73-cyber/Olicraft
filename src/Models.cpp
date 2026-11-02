#include "Models.h"

#include <cmath>

#include "Faces.h"

namespace oc {

namespace {

const std::vector<ModelPart>& partsFor(SkinId id) {
    static const std::vector<ModelPart> player = {
        // Kopf, Koerper, Arme (schwingen mit), Beine; die linken Gliedmassen teilen sich das Netz mit den rechten
        {{0, 28, 0}, {8, 8, 8}, {0, 24, 0}, A_HEAD, 0, 0, false},
        {{0, 18, 0}, {4, 12, 8}, {0, 18, 0}, A_NONE, 16, 16, false},
        {{0, 18, 6}, {4, 12, 4}, {0, 24, 6}, A_ARM_SWING_A, 40, 16, false},
        {{0, 18, -6}, {4, 12, 4}, {0, 24, -6}, A_ARM_SWING_B, 40, 16, true},
        {{0, 6, 2}, {4, 12, 4}, {0, 12, 2}, A_LEG_B, 0, 16, false},
        {{0, 6, -2}, {4, 12, 4}, {0, 12, -2}, A_LEG_A, 0, 16, true},
    };
    static const std::vector<ModelPart> zombie = {
        {{0, 28, 0}, {8, 8, 8}, {0, 24, 0}, A_HEAD, 0, 0, false},
        {{0, 18, 0}, {4, 12, 8}, {0, 18, 0}, A_NONE, 16, 16, false},
        {{0, 18, 6}, {4, 12, 4}, {0, 24, 6}, A_ARM_B, 40, 16, false},
        {{0, 18, -6}, {4, 12, 4}, {0, 24, -6}, A_ARM_A, 40, 16, true},
        {{0, 6, 2}, {4, 12, 4}, {0, 12, 2}, A_LEG_B, 0, 16, false},
        {{0, 6, -2}, {4, 12, 4}, {0, 12, -2}, A_LEG_A, 0, 16, true},
    };
    static const std::vector<ModelPart> villager = {
        // Kopf, Koerper, Arme (haengen, schwingen), Beine, Nase
        {{0, 28, 0}, {8, 8, 8}, {0, 24, 0}, A_HEAD, 0, 0, false},
        {{0, 18, 0}, {4, 12, 8}, {0, 18, 0}, A_NONE, 16, 16, false},
        {{0, 18, 6}, {4, 12, 4}, {0, 24, 6}, A_ARM_SWING_A, 40, 16, false},
        {{0, 18, -6}, {4, 12, 4}, {0, 24, -6}, A_ARM_SWING_B, 40, 16, true},
        {{0, 6, 2}, {4, 12, 4}, {0, 12, 2}, A_LEG_B, 0, 16, false},
        {{0, 6, -2}, {4, 12, 4}, {0, 12, -2}, A_LEG_A, 0, 16, true},
        {{5, 26, 0}, {2, 4, 2}, {0, 24, 0}, A_HEAD, 24, 0, false},
    };
    static const std::vector<ModelPart> pig = {
        {{0, 10, 0}, {16, 8, 10}, {0, 10, 0}, A_NONE, 0, 0, false},
        {{11, 11, 0}, {8, 8, 8}, {7, 11, 0}, A_HEAD, 0, 24, false},
        {{5, 3, 3}, {4, 6, 4}, {5, 6, 3}, A_LEG_B, 32, 24, false},
        {{5, 3, -3}, {4, 6, 4}, {5, 6, -3}, A_LEG_A, 32, 24, true},
        {{-5, 3, 3}, {4, 6, 4}, {-5, 6, 3}, A_LEG_A, 32, 24, false},
        {{-5, 3, -3}, {4, 6, 4}, {-5, 6, -3}, A_LEG_B, 32, 24, true},
    };
    static const std::vector<ModelPart> cow = {
        {{0, 16, 0}, {18, 10, 12}, {0, 16, 0}, A_NONE, 0, 0, false},
        {{11, 19, 0}, {6, 8, 8}, {8, 19, 0}, A_HEAD, 0, 28, false},
        {{10.5f, 24.5f, 3.5f}, {1, 2, 1}, {10.5f, 24.5f, 3.5f}, A_NONE, 44, 28, false},
        {{10.5f, 24.5f, -3.5f}, {1, 2, 1}, {10.5f, 24.5f, -3.5f}, A_NONE, 44, 28, true},
        {{6, 5.5f, 4}, {4, 11, 4}, {6, 11, 4}, A_LEG_B, 28, 28, false},
        {{6, 5.5f, -4}, {4, 11, 4}, {6, 11, -4}, A_LEG_A, 28, 28, true},
        {{-6, 5.5f, 4}, {4, 11, 4}, {-6, 11, 4}, A_LEG_A, 28, 28, false},
        {{-6, 5.5f, -4}, {4, 11, 4}, {-6, 11, -4}, A_LEG_B, 28, 28, true},
    };
    static const std::vector<ModelPart> sheep = {
        {{0, 15.5f, 0}, {16, 11, 12}, {0, 15.5f, 0}, A_NONE, 0, 0, false},
        {{10, 18, 0}, {6, 6, 6}, {7, 18, 0}, A_HEAD, 0, 27, false},
        {{9, 21.5f, 0}, {5, 2, 7}, {9, 21.5f, 0}, A_NONE, 24, 27, false},
        {{5.5f, 5, 3.5f}, {4, 10, 4}, {5.5f, 10, 3.5f}, A_LEG_B, 48, 27, false},
        {{5.5f, 5, -3.5f}, {4, 10, 4}, {5.5f, 10, -3.5f}, A_LEG_A, 48, 27, true},
        {{-5.5f, 5, 3.5f}, {4, 10, 4}, {-5.5f, 10, 3.5f}, A_LEG_A, 48, 27, false},
        {{-5.5f, 5, -3.5f}, {4, 10, 4}, {-5.5f, 10, -3.5f}, A_LEG_B, 48, 27, true},
    };
    switch (id) {
        case SKIN_PLAYER: return player;
        case SKIN_PIG: return pig;
        case SKIN_COW: return cow;
        case SKIN_SHEEP: return sheep;
        case SKIN_VILLAGER:
        case SKIN_VILLAGER_LIBRARIAN:
        case SKIN_VILLAGER_SMITH:
        case SKIN_VILLAGER_MASON:
        case SKIN_VILLAGER_SHEPHERD: return villager;
        default: return zombie;
    }
}

}  // namespace

const std::vector<ModelPart>& modelParts(SkinId skin) { return partsFor(skin); }

glm::ivec2 skinNetSize(const ModelPart& p) {
    const int L = static_cast<int>(p.size.x), H = static_cast<int>(p.size.y), W = static_cast<int>(p.size.z);
    return {2 * L + 2 * W, L + H};
}

NetRect skinNetRect(const ModelPart& p, int face) {
    const int L = static_cast<int>(p.size.x), H = static_cast<int>(p.size.y), W = static_cast<int>(p.size.z);
    if (p.mirror) {
        if (face == 4) face = 5;
        else if (face == 5) face = 4;
    }
    switch (face) {
        case 2: return {p.u + L, p.v, W, L};              // oben
        case 3: return {p.u + L + W, p.v, W, L};          // unten
        case 4: return {p.u, p.v + L, L, H};              // rechte Seite (+Z)
        case 0: return {p.u + L, p.v + L, W, H};          // vorn (+X)
        case 5: return {p.u + L + W, p.v + L, L, H};      // linke Seite (-Z)
        default: return {p.u + 2 * L + W, p.v + L, W, H};  // hinten (-X)
    }
}

glm::vec2 skinCornerUV(const ModelPart& p, int face, int i, int j, int texW, int texH) {
    float uu, vv;  // vv zeigt nach oben
    switch (face) {
        case 0: uu = 1.0f - j; vv = static_cast<float>(i); break;
        case 1: uu = static_cast<float>(i); vv = static_cast<float>(j); break;
        case 2: uu = 1.0f - i; vv = 1.0f - j; break;
        case 3: uu = 1.0f - j; vv = static_cast<float>(i); break;
        case 4: uu = static_cast<float>(i); vv = static_cast<float>(j); break;
        default: uu = 1.0f - j; vv = static_cast<float>(i); break;
    }
    if (p.mirror) uu = 1.0f - uu;
    const NetRect r = skinNetRect(p, face);
    const float inset = 0.02f;  // gegen Bluten zwischen Nachbarflaechen des Netzes
    const float x = static_cast<float>(r.x) + inset + uu * (static_cast<float>(r.w) - 2.0f * inset);
    const float y = static_cast<float>(r.y) + inset + (1.0f - vv) * (static_cast<float>(r.h) - 2.0f * inset);
    return {x / static_cast<float>(texW), y / static_cast<float>(texH)};
}

void appendModel(SkinId skin, const Pose& pose, std::vector<Vertex>& out) {
    const SkinInfo& si = skinInfo(skin);
    const float cy = std::cos(pose.yaw), sy = std::sin(pose.yaw);
    const float cr = std::cos(pose.roll), sr = std::sin(pose.roll);
    const float swing = std::sin(pose.walkPhase) * 0.9f * pose.walkAmount;

    for (const ModelPart& p : partsFor(skin)) {
        float angle = 0.0f;
        switch (p.anim) {
            case A_LEG_A: angle = swing; break;
            case A_LEG_B: angle = -swing; break;
            case A_ARM_A: angle = 1.45f + 0.12f * std::sin(pose.time * 2.0f + pose.walkPhase * 0.5f) + 0.15f * swing; break;
            case A_ARM_B: angle = 1.45f + 0.12f * std::sin(pose.time * 2.0f + 1.5f) - 0.15f * swing; break;
            case A_ARM_SWING_A: angle = swing; break;
            case A_ARM_SWING_B: angle = -swing; break;
            case A_HEAD: angle = pose.headPitch; break;
            default: break;
        }
        const float ca = std::cos(angle), sa = std::sin(angle);
        const glm::vec3 pivot = p.pivot / 16.0f;
        auto xf = [&](glm::vec3 l) {
            if (angle != 0.0f) {  // Gliedschwenken um die Querachse am Gelenk
                float dx = l.x - pivot.x, dy = l.y - pivot.y;
                l.x = pivot.x + dx * ca - dy * sa;
                l.y = pivot.y + dx * sa + dy * ca;
            }
            if (pose.roll != 0.0f) {  // Umfallen um die Laengsachse
                float y = l.y * cr - l.z * sr, z = l.y * sr + l.z * cr;
                l.y = y;
                l.z = z;
            }
            float x = l.x * cy - l.z * sy, z = l.x * sy + l.z * cy;  // Blickrichtung
            return glm::vec3(pose.pos.x + x, pose.pos.y + l.y, pose.pos.z + z);
        };
        const glm::vec3 c = p.center / 16.0f, sz = p.size / 16.0f;
        for (int f = 0; f < 6; ++f) {
            const FaceDef& fd = faceDef(f);
            for (int k = 0; k < 4; ++k) {
                const int i = FACE_CORNER_I[k], j = FACE_CORNER_J[k];
                glm::vec3 local = c;
                for (int a = 0; a < 3; ++a) {
                    float s = static_cast<float>(fd.n[a]) + (i ? 1.0f : -1.0f) * static_cast<float>(fd.t1[a]) +
                              (j ? 1.0f : -1.0f) * static_cast<float>(fd.t2[a]);
                    local[a] += 0.5f * sz[a] * s;
                }
                const glm::vec3 w = xf(local);
                const glm::vec2 uv = skinCornerUV(p, f, i, j, si.width, si.height);
                Vertex v;
                v.x = w.x;
                v.y = w.y;
                v.z = w.z;
                v.u = uv.x;
                v.v = uv.y;
                v.shade = fd.shade * pose.tint;
                v.sky = pose.sky;
                v.light = pose.light;
                out.push_back(v);
            }
        }
    }
}

}  // namespace oc
