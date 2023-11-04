#include "engine/service/freetype.h"
#include "engine/log/service.h"

using namespace simcoe;

// internal data

namespace {
    FT_Library library = nullptr;
}

// freetype

FreeTypeService::FreeTypeService() {

}

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
    return library;
}
