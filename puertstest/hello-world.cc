// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <exception>
#include <iostream>
#include <string>

#include "JsEnv.h"
#include "hello-world-bindings.h"

int main(int argc, char* argv[])
{
    puerts::ProgramOptions Options;
    try
    {
        Options = puerts::ParseProgramOptions(argc, argv);
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    Options.ScriptPath = "hello-world.js";
    Options.bSingleThreadPlatform = false;

    return puerts::RunScriptProgram(Options, &puerts::RegisterExampleBindings);
}
