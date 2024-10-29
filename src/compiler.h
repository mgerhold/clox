#pragma once

#include "vm.h"

[[nodiscard]] bool compile(char const* source, Chunk* chunk);
