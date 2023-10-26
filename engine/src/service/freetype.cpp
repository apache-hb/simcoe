#include "engine/service/freetype.h"

using namespace simcoe;

// freetype

void FreeTypeService::createService() {
    if (FT_Error error = FT_Init_FreeType(&library); error) {
        logAssert("failed to initialize FreeType library (fterr={})", FT_Error_String(error));
    }
}

void FreeTypeService::destroyService() {
    FT_Done_FreeType(library);
}
