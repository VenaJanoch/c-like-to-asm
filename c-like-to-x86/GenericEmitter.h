#pragma once

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <string>
#include <functional>

/// <summary>
/// Generic class that emits output to memory buffer
/// </summary>
class GenericEmitter
{

protected:
    uint8_t* AllocateBuffer(uint32_t size);
    uint8_t* AllocateBufferForInstruction(uint32_t size);

    template<typename T>
    T* AllocateBuffer()
    {
        return reinterpret_cast<T*>(AllocateBuffer(sizeof(T)));
    }


    uint8_t * buffer = nullptr;
    uint32_t buffer_offset = 0;
    uint32_t buffer_size = 0;

    int32_t ip_dst = 0;
};