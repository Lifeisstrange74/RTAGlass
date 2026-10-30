// Files next to the executable (the saves, controls.json): where it is, reading a whole file, and writing one
// safely.
#pragma once

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

namespace util {

// The executable's folder, without a trailing backslash.
inline std::string ExeDir() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    return dir.substr(0, dir.find_last_of("\\/"));
}

inline bool ReadAll(const std::string& path, std::vector<uint8_t>& data) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data.resize(size > 0 ? (size_t)size : 0);
    bool ok = data.empty() || fread(&data[0], 1, data.size(), f) == data.size();
    fclose(f);
    return ok;
}

// Writes through a temporary file, then replaces `path` with it, so a failed write leaves the old file.
template <typename Fn>
bool WriteReplacing(const std::string& path, Fn write) {
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    write(f);
    bool ok = !ferror(f);
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        DeleteFileA(tmp.c_str());
        return false;
    }
    return MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

}  // namespace util
