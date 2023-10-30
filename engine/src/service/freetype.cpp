#include "engine/service/freetype.h"
#include "engine/service/logging.h"

using namespace simcoe;

// freetype

bool FreeTypeService::createService() {
    if (FT_Error error = FT_Init_FreeType(&library); error) {
        LOG_ASSERT("failed to initialize FreeType library (fterr={})", FT_Error_String(error));
    }

    return true;
}

void FreeTypeService::destroyService() {
    FT_Done_FreeType(library);
}

FT_Library FreeTypeService::getLibrary() {
    return get()->library;
}
