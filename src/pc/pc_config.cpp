// Run-time configuration of the PC port (see pc_config.h).

#include "pc_config.h"

#include <pc/enhance.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include <revolution/sc.h>
#include <revolution/vi.h>

namespace {

struct Name {
    const char* text;
    int value;
};

const Name kLanguages[] = {
    {"ja", SC_LANG_JAPANESE}, {"jp", SC_LANG_JAPANESE}, {"japanese", SC_LANG_JAPANESE},
    {"en", SC_LANG_ENGLISH},  {"english", SC_LANG_ENGLISH},
    {"de", SC_LANG_GERMAN},   {"german", SC_LANG_GERMAN},
    {"fr", SC_LANG_FRENCH},   {"french", SC_LANG_FRENCH},
    {"es", SC_LANG_SPANISH},  {"spanish", SC_LANG_SPANISH},
    {"it", SC_LANG_ITALIAN},  {"italian", SC_LANG_ITALIAN},
    {"nl", SC_LANG_DUTCH},    {"dutch", SC_LANG_DUTCH},
    {"zh-cn", SC_LANG_SIMP_CHINESE}, {"zh-tw", SC_LANG_TRAD_CHINESE}, {"ko", SC_LANG_KOREAN},
};

const Name kAreas[] = {
    {"jpn", SC_AREA_JPN}, {"usa", SC_AREA_USA}, {"eur", SC_AREA_EUR}, {"aus", SC_AREA_AUS},
    {"bra", SC_AREA_BRA}, {"twn", SC_AREA_TWN}, {"kor", SC_AREA_KOR}, {"hkg", SC_AREA_HKG},
    {"asi", SC_AREA_ASI}, {"ltn", SC_AREA_LTN}, {"saf", SC_AREA_SAF},
};

const Name kAspects[] = {
    {"4:3", SC_ASPECT_RATIO_4x3},   {"4x3", SC_ASPECT_RATIO_4x3},   {"0", SC_ASPECT_RATIO_4x3},
    {"16:9", SC_ASPECT_RATIO_16x9}, {"16x9", SC_ASPECT_RATIO_16x9}, {"wide", SC_ASPECT_RATIO_16x9},
    {"1", SC_ASPECT_RATIO_16x9},
};

const Name kSounds[] = {
    {"mono", SC_SOUND_MODE_MONO}, {"stereo", SC_SOUND_MODE_STEREO}, {"surround", SC_SOUND_MODE_SURROUND},
    {"0", SC_SOUND_MODE_MONO},    {"1", SC_SOUND_MODE_STEREO},      {"2", SC_SOUND_MODE_SURROUND},
};

const Name kTvFormats[] = {
    {"ntsc", VI_NTSC}, {"pal", VI_PAL}, {"mpal", VI_MPAL}, {"eurgb60", VI_EURGB60},
};

const Name kBools[] = {
    {"0", 0}, {"off", 0}, {"no", 0}, {"false", 0}, {"1", 1}, {"on", 1}, {"yes", 1}, {"true", 1},
};

template <size_t N> bool Lookup(const Name (&table)[N], const char* text, int* out) {
    for (const Name& entry : table) {
        if (strcasecmp(entry.text, text) == 0) {
            *out = entry.value;
            return true;
        }
    }
    return false;
}

template <size_t N> const char* NameOf(const Name (&table)[N], int value) {
    for (const Name& entry : table) {
        if (entry.value == value) {
            return entry.text;
        }
    }
    return "?";
}

void CopyPath(char* dst, size_t size, const char* src) {
    std::snprintf(dst, size, "%s", src);
    size_t length = std::strlen(dst);
    while (length > 1 && dst[length - 1] == '/') {
        dst[--length] = '\0';
    }
}

PCConfig sConfig;
bool sLoaded;
bool sLoading;

void SetDefaults(PCConfig* config) {
    std::memset(config, 0, sizeof(*config));
    config->language = SC_LANG_ENGLISH;
    config->aspectRatio = SC_ASPECT_RATIO_4x3;
    config->progressive = SC_PROGRESSIVE_MODE_ON;
    config->soundMode = SC_SOUND_MODE_STEREO;
    config->productArea = SC_AREA_USA;
    config->euRgb60 = SC_EURGB60_MODE_ON;
    config->wc24Standby = 1;
    config->tvFormat = VI_NTSC;
    config->simpleAddress = 0xFFFFFFFFu;
    // "" = not set: cnt.cpp and nand.cpp choose their defaults (<pc/files.h>).
    config->contentsDir[0] = '\0';
    config->nandDir[0] = '\0';
}

void ApplyEnvironment() {
    static const char* const kVariables[][2] = {
        {"NEWSCHANNEL_LANG", "language"},       {"NEWSCHANNEL_ASPECT", "aspect"},
        {"NEWSCHANNEL_PROGRESSIVE", "progressive"}, {"NEWSCHANNEL_SOUND", "sound"},
        {"NEWSCHANNEL_AREA", "area"},           {"NEWSCHANNEL_COUNTRY", "country"},
        {"NEWSCHANNEL_EURGB60", "eurgb60"},     {"NEWSCHANNEL_WC24", "wc24"},
        {"NEWSCHANNEL_TV", "tv"},               {"NEWSCHANNEL_CONTENTS", "contents"},
        {"NEWSCHANNEL_NAND", "nand"},           {"NEWSCHANNEL_PURIST", "purist"},
    };
    for (const auto& variable : kVariables) {
        const char* value = std::getenv(variable[0]);
        if (value != nullptr && value[0] != '\0' && !PCConfigSet(variable[1], value)) {
            std::fprintf(stderr, "newschannel: %s: bad value '%s'\n", variable[0], value);
        }
    }
}

void EnsureLoaded() {
    if (sLoaded || sLoading) {
        return;
    }
    sLoading = true;
    SetDefaults(&sConfig);

    const char* file = std::getenv("NEWSCHANNEL_CONFIG");
    if (file != nullptr && file[0] != '\0') {
        if (!PCConfigLoad(file)) {
            std::fprintf(stderr, "newschannel: cannot read config file '%s'\n", file);
        }
    } else {
        PCConfigLoad("newschannel.ini"); // optional
    }
    ApplyEnvironment();
    sLoading = false;
    sLoaded = true;
}

char* Trim(char* text) {
    while (std::isspace(static_cast<unsigned char>(*text))) {
        text++;
    }
    char* end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) {
        *--end = '\0';
    }
    return text;
}

} // namespace

PCConfig* PCGetConfig() {
    EnsureLoaded();
    return &sConfig;
}

bool PCConfigSet(const char* key, const char* value) {
    EnsureLoaded();
    PCConfig* config = &sConfig;
    int number;

    if (strcasecmp(key, "language") == 0 || strcasecmp(key, "lang") == 0) {
        if (!Lookup(kLanguages, value, &number)) {
            char* end;
            number = static_cast<int>(std::strtol(value, &end, 10));
            if (end == value || *end != '\0' || number < 0 || number > SC_LANG_KOREAN) {
                return false;
            }
        }
        config->language = static_cast<u8>(number);
    } else if (strcasecmp(key, "aspect") == 0) {
        if (!Lookup(kAspects, value, &number)) {
            return false;
        }
        config->aspectRatio = static_cast<u8>(number);
    } else if (strcasecmp(key, "progressive") == 0) {
        if (!Lookup(kBools, value, &number)) {
            return false;
        }
        config->progressive = static_cast<u8>(number);
    } else if (strcasecmp(key, "sound") == 0) {
        if (!Lookup(kSounds, value, &number)) {
            return false;
        }
        config->soundMode = static_cast<u8>(number);
    } else if (strcasecmp(key, "area") == 0) {
        if (!Lookup(kAreas, value, &number)) {
            return false;
        }
        config->productArea = static_cast<s8>(number);
    } else if (strcasecmp(key, "country") == 0) {
        if (strcasecmp(value, "none") == 0) {
            config->simpleAddress = 0xFFFFFFFFu;
        } else {
            char* end;
            unsigned long id = std::strtoul(value, &end, 0);
            if (end == value || *end != '\0') {
                return false;
            }
            config->simpleAddress = static_cast<u32>(id);
        }
    } else if (strcasecmp(key, "eurgb60") == 0) {
        if (!Lookup(kBools, value, &number)) {
            return false;
        }
        config->euRgb60 = static_cast<u8>(number);
    } else if (strcasecmp(key, "wc24") == 0) {
        if (!Lookup(kBools, value, &number)) {
            return false;
        }
        config->wc24Standby = static_cast<u8>(number);
    } else if (strcasecmp(key, "tv") == 0) {
        if (!Lookup(kTvFormats, value, &number)) {
            return false;
        }
        config->tvFormat = static_cast<u8>(number);
    } else if (strcasecmp(key, "purist") == 0) {
        if (!Lookup(kBools, value, &number)) {
            return false;
        }
        PCSetPurist(number != 0);
    } else if (strncasecmp(key, "enhance.", 8) == 0) {
        // enhance.<name> = 1 | 0 (see <pc/enhance.h>)
        if (!Lookup(kBools, value, &number) || !PCEnhancementSet(key + 8, number != 0)) {
            return false;
        }
    } else if (strcasecmp(key, "contents") == 0) {
        CopyPath(config->contentsDir, sizeof(config->contentsDir), value);
    } else if (strcasecmp(key, "nand") == 0) {
        CopyPath(config->nandDir, sizeof(config->nandDir), value);
    } else {
        return false;
    }
    return true;
}

bool PCConfigLoad(const char* path) {
    EnsureLoaded();
    std::FILE* file = std::fopen(path, "r");
    if (file == nullptr) {
        return false;
    }
    char line[1024];
    int number = 0;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        number++;
        char* comment = std::strchr(line, '#');
        if (comment != nullptr) {
            *comment = '\0';
        }
        char* text = Trim(line);
        if (*text == '\0') {
            continue;
        }
        char* equals = std::strchr(text, '=');
        if (equals == nullptr) {
            std::fprintf(stderr, "%s:%d: expected key = value\n", path, number);
            continue;
        }
        *equals = '\0';
        if (!PCConfigSet(Trim(text), Trim(equals + 1))) {
            std::fprintf(stderr, "%s:%d: unknown key or bad value\n", path, number);
        }
    }
    std::fclose(file);
    std::snprintf(sConfig.configFile, sizeof(sConfig.configFile), "%s", path);
    return true;
}

bool PCConfigSave() {
    EnsureLoaded();
    const PCConfig* config = &sConfig;
    if (config->configFile[0] == '\0') {
        return false;
    }
    std::FILE* file = std::fopen(config->configFile, "w");
    if (file == nullptr) {
        return false;
    }
    std::fprintf(file, "# newschannel settings (see src/pc/pc_config.h)\n");
    std::fprintf(file, "language = %s\n", NameOf(kLanguages, config->language));
    std::fprintf(file, "aspect = %s\n", NameOf(kAspects, config->aspectRatio));
    std::fprintf(file, "progressive = %d\n", config->progressive);
    std::fprintf(file, "sound = %s\n", NameOf(kSounds, config->soundMode));
    std::fprintf(file, "area = %s\n", NameOf(kAreas, config->productArea));
    if (config->simpleAddress == 0xFFFFFFFFu) {
        std::fprintf(file, "country = none\n");
    } else {
        std::fprintf(file, "country = 0x%08X\n", config->simpleAddress);
    }
    std::fprintf(file, "eurgb60 = %d\n", config->euRgb60);
    std::fprintf(file, "wc24 = %d\n", config->wc24Standby);
    std::fprintf(file, "purist = %d\n", PCIsPurist() ? 1 : 0);
    for (int i = 0; i < PCEnhancementCount(); i++) {
        std::fprintf(file, "enhance.%s = %d\n", PCEnhancementGetInfo(i)->key, PCEnhancementIsSet(i) ? 1 : 0);
    }
    std::fprintf(file, "tv = %s\n", NameOf(kTvFormats, config->tvFormat));
    if (config->contentsDir[0] != '\0') {
        std::fprintf(file, "contents = %s\n", config->contentsDir);
    }
    if (config->nandDir[0] != '\0') {
        std::fprintf(file, "nand = %s\n", config->nandDir);
    }
    return std::fclose(file) == 0;
}

