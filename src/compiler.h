#pragma once

#include "object.h"
#include "vm.h"

[[nodiscard]] ObjFunction* compile(char const* source);
