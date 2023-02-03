/*
 * Copyright 2023 Marek Rogalski
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the standard MIT license. See LICENSE for more details.
 */

////////////////////////////////////////////////////////////////
// Bridge that exposes C++ functions to the JavaScript world. //
////////////////////////////////////////////////////////////////

#include <cstdio>
#include <emscripten/bind.h>

#include "colony.h"

using namespace emscripten;
using namespace colony;

EMSCRIPTEN_BINDINGS(my_module) {
    value_object<Assignment>("Assignment")
        .field("character", &Assignment::character)
        .field("task", &Assignment::task)
        .field("cost", &Assignment::cost);

    register_vector<Assignment>("C_vector<Assignment>");

    function("C_ComputeCost", &ComputeCost);
    function("C_LimitAssignments", &LimitAssignments);
    function("C_Optimize", &Optimize);
}
