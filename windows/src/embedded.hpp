// Derleme oncesi tools/gen_embedded.py ile uretilen gomulu dosyalar (diller/*.json, web.html)
#pragma once
#include <cstddef>
struct Embedded { const char* name; const unsigned char* data; size_t size; };
const Embedded* embedded_list(size_t& count);
const Embedded* embedded_find(const char* name);
