#pragma once

#include "JSClassRegister.h"

#include <string>

namespace PUERTS_NAMESPACE
{

struct JSENV_API ProgramOptions
{
    std::string ScriptPath;
    std::string DtsOutputPath;
    bool DtsOnly = false;
};

using RegisterBindingsCallback = void (*)();

JSENV_API ProgramOptions ParseProgramOptions(int argc, char* argv[]);

JSENV_API int RunScriptProgram(const ProgramOptions& Options, RegisterBindingsCallback RegisterBindings);

}    // namespace PUERTS_NAMESPACE
