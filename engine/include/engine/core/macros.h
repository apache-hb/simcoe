#pragma once

#define SM_DEPRECATED(msg) [[deprecated(msg)]]
#define SM_UNUSED [[maybe_unused]]

#define SM_NOCOPY(TYPE) \
    TYPE(const TYPE&) = delete; \
    TYPE& operator=(const TYPE&) = delete;
