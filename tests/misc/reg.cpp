#include <common.h>
#include <stdlib.h>
#include <malloc.h>
#include <pspthreadman.h>
#include <pspreg.h>
#include <string>
#include <vector>

#define SCE_REG_ERROR_CATEGORY_EMPTY_MAYBE 0x80082712

/**
 * Helper to check if a character should be preserved unescaped.
 */
static inline bool is_preserved_char(uint8_t c) {
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
        return true;
    }
    switch (c) {
        case ' ': case '-': case '_': case '.':
        case ',': case ':': case ';':
            return true;
        default:
            return false;
    }
}

/**
 * Takes a uint8_t buffer and converts it into a heap-allocated,
 * escaped C string suitable for embedding directly in C code.
 *
 * Note: The caller is responsible for freeing the returned pointer.
 */
char *escape_to_c_string(const uint8_t *data, size_t len) {
    if (data == NULL) return NULL;

    // In the worst case, a byte becomes a 4-char octal escape like "\377".
    // Allocate max possible size: (len * 4) + 1 null terminator.
    size_t max_out_len = (len * 4) + 1;
    char *out = (char *)malloc(max_out_len);
    if (out == NULL) return NULL;

    size_t pos = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];

        if (is_preserved_char(c)) {
            out[pos++] = (char)c;
        } else {
            // Handle common formatting escapes first for cleaner output
            switch (c) {
                case '\n': out[pos++] = '\\'; out[pos++] = 'n'; break;
                case '\r': out[pos++] = '\\'; out[pos++] = 'r'; break;
                case '\t': out[pos++] = '\\'; out[pos++] = 't'; break;
                case '\\': out[pos++] = '\\'; out[pos++] = '\\'; break;
                case '"':  out[pos++] = '\\'; out[pos++] = '"'; break;
                default:
                    // Octal escapes (\ooo) prevent ambiguity issues if followed by digit characters
                    pos += snprintf(&out[pos], max_out_len - pos, "\\%03o", c);
                    break;
            }
        }
    }

    out[pos] = '\0';
    return out;
}

// This dumps a category of the registry to C code compatible with sceReg.cpp in PPSSPP, the PSP emulator.

void DumpCategory(REGHANDLE regHandle, const std::string &path, const std::string &concat_name, const std::string &name) {
    REGHANDLE category = 0xcccccccc;

    int retval = sceRegOpenCategory(regHandle, path.c_str(), 2, &category);
    if (retval == (int)SCE_REG_ERROR_CATEGORY_EMPTY_MAYBE) {
        schedf("// %s was not accessible (returned %08x)\n", path.c_str(), retval);
        schedf("static const KeyValue %s[1] = { \"\", ValueType::FAIL, \"\", (int)0x%08x };\n\n", concat_name.c_str(), retval);
        return;
    }

    if (retval < 0) {
        schedf("%08x = sceRegOpenCategory(%s)\n", retval, path.c_str());
        return;
    }

    int numKeys;
    int result = sceRegGetKeysNum(category, &numKeys);
    if (result < 0) {
        schedf("%08x = sceRegGetKeysNum(%s) -> %d\n", result, path.c_str(), numKeys);
        return;
    }
    // schedf("%08x = sceRegGetKeysNum(fontCategory) -> %d\n", result, numKeys);

    char *keyData = (char *)malloc(numKeys * 27);  // ??? 27 bytes per key?
    result = sceRegGetKeys(category, keyData, numKeys);

    // First, dump all subcategories.
    for (int i = 0; i < numKeys; i++) {
        char *keyName = keyData + i * 27;
        unsigned int type;
        SceSize size = 0xcccccccc;
        // unsigned char data[1024];
        REGHANDLE keyHandle;
        if (!sceRegGetKeyInfo(category, keyName, &keyHandle, &type, &size)) {
            if (type == REG_TYPE_DIR) {
                DumpCategory(regHandle, path + "/" + keyName, concat_name + "_" + keyName, keyName);
            }
        }
    }

    // Then, actually list the contents of this category, in the struct form.
    schedf("// Dump of %s\n", path.c_str());
    schedf("static const KeyValue %s[] = {\n", concat_name.c_str());
    for (int i = 0; i < numKeys; i++) {
        char *keyName = keyData + i * 27;
        unsigned int type;
        SceSize size = 0xcccccccc;
        unsigned char data[1024];
        REGHANDLE keyHandle;
        if (!sceRegGetKeyInfo(category, keyName, &keyHandle, &type, &size)) {
            if (type == REG_TYPE_DIR) {
                std::string subdirName = concat_name + "_" + keyName;
                schedf("\t{ \"%s\", ValueType::DIR, \"\", ARRAY_SIZE(%s), %s },\n", keyName, subdirName.c_str(), subdirName.c_str());
            } else {
                if (!sceRegGetKeyValue(category, keyHandle, data, size)) {
                    switch(type) {
                    case REG_TYPE_INT:
                    {
                        int ivalue = *((int*) data);
                        schedf("\t{ \"%s\", ValueType::INT, \"\", (int)0x%x },  // decimal: %d\n", keyName, ivalue, ivalue);
                        break;
                    }
                    case REG_TYPE_STR:
                        schedf("\t{ \"%s\", ValueType::STR, \"%s\" },  // size: %d\n", keyName, (char *)data, size);
                        break;
                    case REG_TYPE_BIN:
                        if (size > 512) {
                            schedf("\t // Skipping %s (large binary: %d)\n", keyName, (int)size);
                        } else {
                            // First check if data is all zeroes, in that case we emit null, this will be handled in the emulator.
                            bool allZero = true;
                            for (size_t j = 0; j < size; j++) {
                                if (data[j] != 0) {
                                    allZero = false;
                                    break;
                                }
                            }
                            if (allZero) {
                                schedf("\t{ \"%s\", ValueType::BIN, \"\", %d },  // (all zero)\n", keyName, (int)size);
                            } else {
                                char *escaped = escape_to_c_string(data, size);
                                schedf("\t{ \"%s\", ValueType::BIN, \"%s\", %d },\n", keyName, escaped, (int)size);
                                free(escaped);
                            }
                        }
                        break;
                    };
                }
            }
        }
    }

    schedf("};\n\n");

    sceRegCloseCategory(category);

    free(keyData);
}

void DumpRegistry(REGHANDLE regHandle) {
    schedf("\n\n// Dump of the PSP registry using tests/misc/reg.prx in pspautotests\n\n");

    static const char *topLevel[] = {
        "DATA",
        "SYSPROFILE",
        "CONFIG",
        "REGISTRY",
        // probably more...
    };

    for (int i = 0; i < (int)ARRAY_SIZE(topLevel); i++) {
        std::string concat = "tree_" + std::string(topLevel[i]);
        for (int j = 0; j < (int)concat.size(); j++) {
            if (concat[j] == '/') {
                concat[j] = '_';
            }
        }
        DumpCategory(regHandle, std::string("/") + topLevel[i], concat, topLevel[i]);
    }
}

extern "C" int main(int argc, char *argv[]) {
	checkpointNext("Registry test");

    struct RegParam reg;
	REGHANDLE regHandle;

	memset(&reg, 0, sizeof(reg));
	reg.regtype = 1;
	reg.namelen = strlen("/system");
	reg.unk2 = 1;
	reg.unk3 = 1;
	strcpy(reg.name, "/syst1m");

    int retval = sceRegOpenRegistry(&reg, 2, &regHandle);
    schedf("%08x = sceRegOpenRegistry(bad) -> handle %08x\n", retval, regHandle);

    // OK, trying with the correct name now.
	strcpy(reg.name, "/system");
    retval = sceRegOpenRegistry(&reg, 2, &regHandle);
    schedf("%08x = sceRegOpenRegistry(/system) -> handle %08x\n", retval, regHandle);
    if (retval < 0) {
        schedf("couldn't open registry: %08X\n", retval);
        // OK, we're screwed.
        return 0;
    }


    REGHANDLE rootCategory;
    retval = sceRegOpenCategory(regHandle, "", 2, &rootCategory);
    schedf("%08x = sceRegOpenCategory() -> handle %08x\n", retval, rootCategory);

    if (retval < 0) {
        retval = sceRegOpenCategory(regHandle, "/", 2, &rootCategory);
        schedf("%08x = sceRegOpenCategory(/) -> handle %08x\n", retval, rootCategory);
    }

    REGHANDLE sysProfileCategory;
    retval = sceRegOpenCategory(regHandle, "/SYSPROFILE/RESOLUTION", 2, &sysProfileCategory);
    if (retval < 0) {
        schedf("%08x = sceRegOpenCategory(SYSPROFILE) -> handle %08x\n", retval, sysProfileCategory);
    }

    REGHANDLE fontCategory;

    retval = sceRegOpenCategory(-1337, "/DATA/FONT", 2, &fontCategory);
    schedf("%08x = sceRegOpenCategory(badreg, DATA/FONT) -> handle %08x\n", retval, fontCategory);

    retval = sceRegOpenCategory(regHandle, "/DATA/FANT", 2, &fontCategory);
    schedf("%08x = sceRegOpenCategory(DATA/FANT (bad)) -> handle %08x\n", retval, fontCategory);
    // Ignoring retval here, we tried to provoke an error.

    retval = sceRegOpenCategory(regHandle, "/DATA/FONT", 2, &fontCategory);
    schedf("%08x = sceRegOpenCategory(DATA/FONT)\n", retval);
    if (retval < 0) {
        return 0;
    }
    int numKeys;
    int result = sceRegGetKeysNum(fontCategory, &numKeys);
    schedf("%08x = sceRegGetKeysNum(fontCategory) -> %d\n", result, numKeys);

    char *keyData = (char *)malloc(numKeys * 27);  // ??? 27 bytes per key?
    result = sceRegGetKeys(fontCategory, keyData, numKeys);

    for (int i = 0; i < numKeys; i++) {
        char *keyName = keyData + i * 27;
        schedf("keyname %d: %s\n", i, keyName);unsigned int type;
        SceSize size = 0xcccccccc;
        unsigned char data[1024];
        REGHANDLE keyHandle;
        if (!sceRegGetKeyInfo(fontCategory, keyName, &keyHandle, &type, &size)) {
            if (type == REG_TYPE_DIR) {
                schedf("DIR - %-27s size=%08x\n", keyName, size);
                /*
                int retval = sceRegGetKeyValue(fontCategory, keyHandle, data, size);
                if (retval < 0) {
                    schedf("sceRegGetKeyValue on dir failed: %08X\n", retval);
                } else {
                    schedf("DIR - %-27s (%d)\n", keyName, size);
                }*/
            } else {
                if (!sceRegGetKeyValue(fontCategory, keyHandle, data, size)) {
                    switch(type) {
                    case REG_TYPE_INT:
                        schedf("INT - %-27s - %4d : %d\n", keyName, size, *((int*) data));
                        break;
                    case REG_TYPE_STR:
                        schedf("STR - %-27s - %4d : %s\n", keyName, size, (char *)data);
                        break;
                    case REG_TYPE_BIN:
                        {
                            int i;
                            schedf("BIN - %-27s - %4d : ", keyName, size);
                            for (i = 0; i < (int)size-1; i++) {
                                schedf("%02X-", data[i]);
                            }
                            schedf("%02X\n", data[i]);
                        }
                        break;
                    };
                }
            }
        }
    }

    sceRegCloseCategory(fontCategory);

    DumpRegistry(regHandle);

    retval = sceRegCloseRegistry(1337);
    schedf("%08x = sceRegCloseRegistry(1337)\n", retval);
    retval = sceRegCloseRegistry(regHandle);
    schedf("%08x = sceRegCloseRegistry(regHandle)\n", retval);

	return 0;
}