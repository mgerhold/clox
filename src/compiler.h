#pragma once

#include "object.h"
#include "vm.h"

[[nodiscard]] bool compile(char const* source, Chunk* chunk);
