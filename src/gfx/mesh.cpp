#include "mesh.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>

#include "../util/data_pack.h"

namespace gfx {

namespace {

std::string DirOf(const std::string& path) {
    size_t s = path.find_last_of("\\/");
    return s == std::string::npos ? std::string() : path.substr(0, s + 1);
}

std::string BaseName(const std::string& path) {
    size_t s = path.find_last_of("\\/");
    std::string name = s == std::string::npos ? path : path.substr(s + 1);
    size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string Trim(const char* s) {
    while (*s == ' ' || *s == '\t') ++s;
    std::string r = s;
    while (!r.empty() && (r.back() == '\n' || r.back() == '\r' || r.back() == ' ' || r.back() == '\t')) r.pop_back();
    return r;
}

// Reads a material library: material name -> part settings (texture resolved against the .mtl's directory).
void LoadMtl(const std::string& path, std::map<std::string, MeshPart>& out) {
    std::vector<std::string> lines;
    if (!util::Data_ReadLines(path, lines)) return;
    std::string dir = DirOf(path);
    MeshPart* cur = nullptr;
    for (const std::string& line : lines) {
        std::string l = Trim(line.c_str());
        if (l.compare(0, 7, "newmtl ") == 0) {
            std::string name = Trim(l.c_str() + 7);
            cur = &out[name];
            cur->material = name;
        } else if (cur && l.compare(0, 7, "map_Kd ") == 0) {
            std::string rest = Trim(l.c_str() + 7);
            if (rest.compare(0, 10, "-clamp on ") == 0) {
                cur->clampU = cur->clampV = true;
                rest = Trim(rest.c_str() + 10);
            }
            cur->texture = dir + rest;
        } else if (cur && l.compare(0, 6, "paint ") == 0) {
            cur->paint = atoi(l.c_str() + 6);
        } else if (cur && l.compare(0, 5, "wrap ") == 0) {
            char u[16] = {}, v[16] = {};
            if (sscanf(l.c_str() + 5, "%15s %15s", u, v) == 2) {
                cur->clampU = strcmp(u, "clamp") == 0;
                cur->clampV = strcmp(v, "clamp") == 0;
            }
        }
    }
}

// One face corner: "p", "p/t", "p//n" or "p/t/n" (1-based). Returns false at the end of the line.
bool Corner(const char*& s, unsigned& p, unsigned& t, unsigned& n) {
    while (*s == ' ' || *s == '\t') ++s;
    if (*s < '0' || *s > '9') return false;
    char* e;
    p = strtoul(s, &e, 10);
    t = n = 0;
    s = e;
    if (*s == '/') {
        ++s;
        if (*s != '/') {
            t = strtoul(s, &e, 10);
            s = e;
        }
        if (*s == '/') {
            ++s;
            n = strtoul(s, &e, 10);
            s = e;
        }
    }
    return true;
}

// A decimal number as sscanf's %f reads one (sign, digits, fraction, exponent), at `s` after any blanks; false if
// there is none. sscanf goes through the C library's locale and stream machinery for every number, which made it most
// of a world block's loading time.
bool Number(const char*& s, float& out) {
    static const double kTens[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                   1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    const char* p = s;
    while (*p == ' ' || *p == '\t') ++p;
    bool negative = *p == '-';
    if (*p == '-' || *p == '+') ++p;
    uint64_t mantissa = 0;
    int exponent = 0, digits = 0;
    for (; *p >= '0' && *p <= '9'; ++p, ++digits) {
        if (mantissa < 100000000000000000ull) mantissa = mantissa * 10 + (*p - '0');
        else ++exponent;
    }
    if (*p == '.') {
        for (++p; *p >= '0' && *p <= '9'; ++p, ++digits) {
            if (mantissa < 100000000000000000ull) {
                mantissa = mantissa * 10 + (*p - '0');
                --exponent;
            }
        }
    }
    if (!digits) return false;
    if ((*p == 'e' || *p == 'E') &&
        ((p[1] >= '0' && p[1] <= '9') || ((p[1] == '-' || p[1] == '+') && p[2] >= '0' && p[2] <= '9'))) {
        ++p;
        bool down = *p == '-';
        if (*p == '-' || *p == '+') ++p;
        int e = 0;
        for (; *p >= '0' && *p <= '9'; ++p)
            if (e < 10000) e = e * 10 + (*p - '0');
        exponent += down ? -e : e;
    }
    double v = (double)mantissa;
    int a = exponent < 0 ? -exponent : exponent;
    double scale = a < 23 ? kTens[a] : pow(10.0, a);
    v = exponent < 0 ? v / scale : v * scale;
    out = (float)(negative ? -v : v);
    s = p;
    return true;
}

// Up to `max` numbers in a row, as sscanf("%f %f ...") reads them: how many there were.
int Numbers(const char* s, float* out, int max) {
    int n = 0;
    while (n < max && Number(s, out[n])) ++n;
    return n;
}

}  // namespace

bool LoadObj(const std::string& path, Mesh& mesh) {
    // The whole file at once (loose, or from a mounted pack), its lines cut in place.
    std::vector<uint8_t> bytes;
    if (!util::Data_ReadFile(path, bytes)) return false;
    bytes.push_back(0);
    char* text = (char*)bytes.data();
    std::vector<MeshVertex> pos;   // v lines (with colour)
    std::vector<float> nrm, tex, spec;   // vn, vt, vs lines
    std::vector<float> dusk, night;      // vdusk, vnight lines (4 each)
    std::map<std::string, MeshPart> materials;
    bool billboard = false, cueFog = false;
    float anchor[3] = {};
    mesh = Mesh();
    mesh.hasNormals = false;
    std::vector<MeshVertex> poly;
    char* next = text;
    for (char* line = next; *line; line = next) {
        char* end = strchr(line, '\n');
        next = end ? end + 1 : line + strlen(line);
        if (end) *end = 0;
        float a[7];
        if (line[0] == 'v' && line[1] == ' ') {
            MeshVertex v = {};
            v.color[0] = v.color[1] = v.color[2] = v.color[3] = 1.0f;
            int n = Numbers(line + 2, a, 7);
            for (int k = 0; k < n; ++k) (k < 3 ? v.pos[k] : v.color[k - 3]) = a[k];
            v.billboard = billboard;
            memcpy(v.anchor, anchor, sizeof(anchor));
            v.cueFog = cueFog;
            pos.push_back(v);
        } else if (strncmp(line, "billboard ", 10) == 0) {
            billboard = Numbers(line + 10, a, 3) == 3;
            if (billboard) memcpy(anchor, a, sizeof(anchor));
        } else if (strncmp(line, "cuefog", 6) == 0) {
            cueFog = strncmp(line + 6, " off", 4) != 0;
        } else if (strncmp(line, "vn ", 3) == 0 && Numbers(line + 3, a, 3) == 3) {
            nrm.insert(nrm.end(), a, a + 3);
        } else if (strncmp(line, "vs ", 3) == 0 && Numbers(line + 3, a, 1) == 1) {
            spec.push_back(a[0]);
        } else if (strncmp(line, "vdusk ", 6) == 0 || strncmp(line, "vnight ", 7) == 0) {
            bool isDusk = line[1] == 'd';
            float c[4] = {0, 0, 0, 1};
            Numbers(line + (isDusk ? 6 : 7), c, 4);
            (isDusk ? dusk : night).insert((isDusk ? dusk : night).end(), c, c + 4);
        } else if (strncmp(line, "vt ", 3) == 0 && Numbers(line + 3, a, 2) == 2) {
            tex.push_back(a[0]);
            tex.push_back(1.0f - a[1]);   // OBJ's v runs up; ours runs down the image
        } else if (strncmp(line, "mtllib ", 7) == 0) {
            LoadMtl(DirOf(path) + Trim(line + 7), materials);
        } else if (strncmp(line, "usemtl ", 7) == 0) {
            std::string name = Trim(line + 7);
            if (!mesh.parts.empty() && mesh.parts.back().material == name) continue;
            auto it = materials.find(name);
            MeshPart part = it != materials.end() ? it->second : MeshPart();
            part.material = name;
            part.first = (uint32_t)mesh.indices.size();
            part.count = 0;
            mesh.parts.push_back(part);
        } else if (line[0] == 'f' && line[1] == ' ') {
            // Polygons are split into a fan.
            poly.clear();
            const char* s = line + 2;
            unsigned p, t, n;
            while (Corner(s, p, t, n)) {
                if (!p || p > pos.size()) continue;
                MeshVertex mv = pos[p - 1];
                if (p <= spec.size()) {
                    mv.specular = spec[p - 1];
                    mesh.hasSpecular = true;
                }
                if (p * 4 <= dusk.size() && p * 4 <= night.size()) {
                    memcpy(mv.dusk, &dusk[(p - 1) * 4], sizeof(mv.dusk));
                    memcpy(mv.night, &night[(p - 1) * 4], sizeof(mv.night));
                    mesh.hasTimeColors = true;
                }
                if (n && n * 3 <= nrm.size()) {
                    memcpy(mv.normal, &nrm[(n - 1) * 3], sizeof(mv.normal));
                    mesh.hasNormals = true;
                }
                if (t && t * 2 <= tex.size()) {
                    memcpy(mv.uv, &tex[(t - 1) * 2], sizeof(mv.uv));
                    mesh.hasUvs = true;
                }
                poly.push_back(mv);
            }
            for (size_t i = 2; i < poly.size(); ++i) {
                const MeshVertex* tri[3] = {&poly[0], &poly[i - 1], &poly[i]};
                for (const MeshVertex* c : tri) {
                    mesh.indices.push_back((uint32_t)mesh.vertices.size());
                    mesh.vertices.push_back(*c);
                }
                if (!mesh.parts.empty()) mesh.parts.back().count += 3;
            }
        }
    }
    return !mesh.indices.empty();
}

bool SaveObj(const std::string& path, const Mesh& mesh, const char* comment) {
    FILE* fp = fopen(path.c_str(), "w");
    if (!fp) return false;
    if (comment) fprintf(fp, "# %s\n", comment);
    std::string mtl = BaseName(path) + ".mtl";
    if (!mesh.parts.empty()) fprintf(fp, "mtllib %s\n", mtl.c_str());
    // The alpha goes after the colour only when it isn't 1 (the loader's default): the courses' glass, water and
    // light beams.
    // A billboard's vertices follow its "billboard x y z" line; "billboard off" ends them.
    // Vertices that fog by the distance cue's numbers follow a "cuefog" line; "cuefog off" ends them.
    bool billboard = false, cueFog = false;
    float anchor[3] = {};
    for (const MeshVertex& v : mesh.vertices) {
        if (v.cueFog != cueFog) fprintf(fp, v.cueFog ? "cuefog\n" : "cuefog off\n");
        cueFog = v.cueFog;
        if (v.billboard && (!billboard || memcmp(anchor, v.anchor, sizeof(anchor)))) {
            fprintf(fp, "billboard %.6f %.6f %.6f\n", v.anchor[0], v.anchor[1], v.anchor[2]);
            memcpy(anchor, v.anchor, sizeof(anchor));
        } else if (!v.billboard && billboard) {
            fprintf(fp, "billboard off\n");
        }
        billboard = v.billboard;
        fprintf(fp, "v %.6f %.6f %.6f %.4f %.4f %.4f", v.pos[0], v.pos[1], v.pos[2], v.color[0], v.color[1],
                v.color[2]);
        if (v.color[3] < 1.0f) fprintf(fp, " %.4f", v.color[3]);
        fputc('\n', fp);
    }
    if (mesh.hasSpecular)
        for (const MeshVertex& v : mesh.vertices) fprintf(fp, "vs %g\n", v.specular);
    if (mesh.hasTimeColors) {
        for (const MeshVertex& v : mesh.vertices)
            fprintf(fp, "vdusk %.4f %.4f %.4f %.4f\n", v.dusk[0], v.dusk[1], v.dusk[2], v.dusk[3]);
        for (const MeshVertex& v : mesh.vertices)
            fprintf(fp, "vnight %.4f %.4f %.4f %.4f\n", v.night[0], v.night[1], v.night[2], v.night[3]);
    }
    if (mesh.hasUvs)
        for (const MeshVertex& v : mesh.vertices) fprintf(fp, "vt %.5f %.5f\n", v.uv[0], 1.0f - v.uv[1]);
    if (mesh.hasNormals)
        for (const MeshVertex& v : mesh.vertices)
            fprintf(fp, "vn %.5f %.5f %.5f\n", v.normal[0], v.normal[1], v.normal[2]);
    auto corner = [&](uint32_t i) {
        unsigned k = i + 1;
        if (mesh.hasUvs && mesh.hasNormals) fprintf(fp, " %u/%u/%u", k, k, k);
        else if (mesh.hasUvs) fprintf(fp, " %u/%u", k, k);
        else if (mesh.hasNormals) fprintf(fp, " %u//%u", k, k);
        else fprintf(fp, " %u", k);
    };
    auto faces = [&](uint32_t first, uint32_t count) {
        for (uint32_t i = first; i + 2 < first + count && i + 2 < mesh.indices.size(); i += 3) {
            fprintf(fp, "f");
            corner(mesh.indices[i]);
            corner(mesh.indices[i + 1]);
            corner(mesh.indices[i + 2]);
            fprintf(fp, "\n");
        }
    };
    if (mesh.parts.empty()) {
        faces(0, (uint32_t)mesh.indices.size());
    } else {
        for (const MeshPart& p : mesh.parts) {
            fprintf(fp, "usemtl %s\n", p.material.c_str());
            faces(p.first, p.count);
        }
    }
    fclose(fp);
    if (mesh.parts.empty()) return true;

    // The material library: each material once.
    fp = fopen((DirOf(path) + mtl).c_str(), "w");
    if (!fp) return false;
    if (comment) fprintf(fp, "# %s\n", comment);
    std::map<std::string, bool> written;
    for (const MeshPart& p : mesh.parts) {
        if (written[p.material]) continue;
        written[p.material] = true;
        fprintf(fp, "\nnewmtl %s\nKd 1 1 1\n", p.material.c_str());
        if (p.paint) fprintf(fp, "paint %d\n", p.paint);
        if (p.texture.empty()) continue;
        fprintf(fp, "map_Kd %s%s\n", p.clampU && p.clampV ? "-clamp on " : "", p.texture.c_str());
        fprintf(fp, "wrap %s %s\n", p.clampU ? "clamp" : "repeat", p.clampV ? "clamp" : "repeat");
    }
    fclose(fp);
    return true;
}

}  // namespace gfx
