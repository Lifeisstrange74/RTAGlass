#include "messages.h"

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <utility>

namespace game {

namespace {

std::map<std::pair<int, int>, MessageTable> g_tables;
std::vector<MessageTable> g_mail;

// A message line's text with its \xHH escapes turned back into bytes.
std::string Unescape(const char* s) {
    std::string out;
    while (*s && *s != '\n' && *s != '\r') {
        if (s[0] == '\\' && s[1] == 'x' && s[2] && s[3]) {
            char hex[3] = {s[2], s[3], 0};
            out += (char)strtol(hex, nullptr, 16);
            s += 4;
        } else {
            out += *s++;
        }
    }
    return out;
}

}  // namespace

bool Messages_Load(const std::string& dataDir) {
    g_tables.clear();
    FILE* fp = fopen((dataDir + "\\story\\messages.txt").c_str(), "rb");
    if (!fp) return false;
    static char line[4096];
    MessageTable* table = nullptr;
    while (fgets(line, sizeof line, fp)) {
        int town, place, n, used;
        if (line[0] == '#') continue;
        if (sscanf(line, "table %d %d", &town, &place) == 2) {
            table = &g_tables[std::make_pair(town, place)];
        } else if (table && sscanf(line, "%d %n", &n, &used) == 1 && n >= 0 && n < 256) {
            if ((int)table->size() <= n) table->resize(n + 1);
            (*table)[n] = Unescape(line + used);
        }
    }
    fclose(fp);
    return !g_tables.empty();
}

bool Mail_Load(const std::string& dataDir) {
    g_mail.clear();
    FILE* fp = fopen((dataDir + "\\story\\mail.txt").c_str(), "rb");
    if (!fp) return false;
    static char line[1024];
    MessageTable* mail = nullptr;
    while (fgets(line, sizeof line, fp)) {
        int m;
        if (line[0] == '#') continue;
        if (sscanf(line, "mail %d", &m) == 1 && m >= 0 && m < 128) {
            if ((int)g_mail.size() <= m) g_mail.resize(m + 1);
            mail = &g_mail[m];
            continue;
        }
        // "K TEXT": exactly one space after the number, as some lines start with spaces (mail 61's heart).
        char* end;
        long k = strtol(line, &end, 10);
        if (!mail || end == line || k < 0 || k > 16) continue;
        if (*end == ' ') ++end;
        if ((int)mail->size() <= k) mail->resize(k + 1);
        (*mail)[k] = Unescape(end);
    }
    fclose(fp);
    return !g_mail.empty();
}

const MessageTable* Mail_Text(int m) {
    return m >= 0 && m < (int)g_mail.size() && !g_mail[m].empty() ? &g_mail[m] : nullptr;
}

const MessageTable* Messages_Table(int town, int place) {
    auto it = g_tables.find(std::make_pair(town, place));
    return it == g_tables.end() ? nullptr : &it->second;
}

}  // namespace game
