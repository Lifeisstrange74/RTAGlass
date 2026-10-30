// Meshes in the PC data set (data/models, made by tools/modelconv): Wavefront OBJ with per-vertex colour
// ("v x y z r g b", colours 0..1, optionally followed by an alpha "a" where it isn't 1), optional normals and texture
// coordinates, and triangles.
//
// RTAGlass extension: "billboard x y z" before vertex lines makes the following vertices a sprite that turns to
// face the camera about the vertical through (x, y, z), until "billboard off". The vertices are stored as seen
// from -z, so other tools show the sprite facing that way.
// RTAGlass extension: "vs s" lines, one per "v" line and in the same order, give each vertex a specular strength
// (the cars' lighting, see model.h; 0-255 scale).
// RTAGlass extension: "vdusk r g b a" and "vnight r g b a" lines, one per "v" line and in the same order, give each
// vertex the course's dusk and night colours beside the day's on its "v" line (programs 0x30/0x40 blend the three by
// the race's time of day, DrawStyle::weights).
// RTAGlass extension: "cuefog" before vertex lines makes the following vertices fog by the distance cue's numbers
// instead of the fog's, until "cuefog off" (programs 0x30/0x40 take vf11 for a strip whose header has y bit 1 set,
// vf10 otherwise; DrawStyle::cue).
//
// Textured meshes come with a material library (.mtl, next to the .obj). Each material is one texture, or none:
//   newmtl NAME
//   map_Kd [-clamp on] PATH     the texture, relative to the .mtl (the data set's DDS files)
//   wrap U V                    RTAGlass extension: repeat or clamp per axis (other tools ignore the line)
//   paint N                     RTAGlass extension: the part takes paint colour N (1 or 2) of the car it belongs to
// A textured material modulates the texture with the vertex colour on the PS2's scale: colour 0.5 (128 of 255)
// leaves the texture as it is, 1.0 doubles it. An untextured material shows the vertex colour as is.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace gfx {

struct MeshVertex {
    float pos[3];
    float normal[3];
    float color[4];   // 0..1; alpha 1 = as the material has it
    float uv[2];
    bool billboard;   // turns about `anchor` to face the camera
    float anchor[3];
    float specular;   // highlight strength (0 = none)
    float dusk[4], night[4];   // the course's other colour sets (Mesh::hasTimeColors)
    bool cueFog;      // fogs by the distance cue's numbers ("cuefog")
};

// A run of triangles drawn with one material.
struct MeshPart {
    std::string material;
    std::string texture;   // DDS file; empty = untextured. As written in the .mtl when saving; resolved (relative
                           // to the working directory, like the .obj path) when loading
    bool clampU = false, clampV = false;
    int paint = 0;   // 0 = none; 1 or 2 = the car's paint colour multiplies the vertex colour
    uint32_t first = 0, count = 0;   // range of `indices`
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;   // triangle list
    std::vector<MeshPart> parts;     // empty = one untextured part covering everything
    bool hasNormals = true, hasUvs = false, hasSpecular = false, hasTimeColors = false;
};

bool LoadObj(const std::string& path, Mesh& mesh);
// Writes the .obj and, if the mesh has parts, a .mtl with the same base name.
bool SaveObj(const std::string& path, const Mesh& mesh, const char* comment);

}  // namespace gfx
