#pragma once
#include <vector>

#include <glm/glm.hpp>

#include "Atlas.h"
#include "Chunk.h"
#include "Faces.h"

namespace oc {

// Quader fuer Gegenstaende am Boden (Texturen aus dem Blockatlas); xf wandelt lokale Koordinaten in Kamera-Weltkoordinaten
template <class XF>
inline void addBox(std::vector<Vertex>& out, const glm::vec3& center, const glm::vec3& size, Tile tile, float sky, float light, XF&& xf) {
    const UVRect uv = tileUV(tile);
    for (int f = 0; f < 6; ++f) {
        const FaceDef& fd = faceDef(f);
        for (int k = 0; k < 4; ++k) {
            const int i = FACE_CORNER_I[k], j = FACE_CORNER_J[k];
            glm::vec3 local = center;
            for (int a = 0; a < 3; ++a) {
                float s = static_cast<float>(fd.n[a]) + (i ? 1.0f : -1.0f) * static_cast<float>(fd.t1[a]) +
                          (j ? 1.0f : -1.0f) * static_cast<float>(fd.t2[a]);
                local[a] += 0.5f * size[a] * s;
            }
            glm::vec3 p = xf(local);
            float uu, vv;
            faceUV(f, i, j, uu, vv);
            Vertex v;
            v.x = p.x;
            v.y = p.y;
            v.z = p.z;
            v.u = uv.u0 + (uv.u1 - uv.u0) * uu;
            v.v = uv.v0 + (uv.v1 - uv.v0) * vv;
            v.shade = fd.shade;
            v.sky = sky;
            v.light = light;
            out.push_back(v);
        }
    }
}

}  // namespace oc
