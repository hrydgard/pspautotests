#include <common.h>
#include <stdlib.h>
#include <malloc.h>
#include <pspthreadman.h>
#include <pspreg.h>
#include <string>
#include <vector>

#define SCE_REG_ERROR_CATEGORY_EMPTY_MAYBE 0x80082712

void DumpCategory(REGHANDLE regHandle, const std::string &path, const std::string &concat_name, const std::string &name) {
    REGHANDLE category = 0xcccccccc;

    int retval = sceRegOpenCategory(regHandle, path.c_str(), 2, &category);
    if (retval == SCE_REG_ERROR_CATEGORY_EMPTY_MAYBE) {
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
        schedf("%08x = sceRegGetKeysNum(%s) -> %d\n", result, numKeys);
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
        unsigned char data[1024];
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
                        schedf("\t{ \"%s\", ValueType::INT, \"\", (int)0x%08x },\n", keyName, ivalue);
                        break;
                    }
                    case REG_TYPE_STR:
                        schedf("\t{ \"%s\", ValueType::STR, \"%s\" },\n", keyName, (char *)data);
                        break;
                    case REG_TYPE_BIN:
                        // Not yet supported
                        schedf("\t // Skipping %s (binary)\n", keyName);
                        break;
                    };
                }
            }
        }
    }

    schedf("};\n\n");

    free(keyData);
}

void DumpRegistry(REGHANDLE regHandle) {
    schedf("\n\n// Dump of the PSP registry using tests/misc/reg.prx in pspautotests\n\n");

    static const char *topLevel[] = {
        "DATA",
        "SYSPROFILE",
        // probably more...
    };

    for (int i = 0; i < ARRAY_SIZE(topLevel); i++) {
        std::string concat = "tree_" + std::string(topLevel[i]);
        for (int j = 0; j < concat.size(); j++) {
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
	int ret = 0;

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
                            for (i = 0; i < size-1; i++) {
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