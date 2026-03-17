// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Binding.hpp"
#include "CppObjectMapper.h"
#include "DeclarationGenerator.h"

#include "libplatform/libplatform.h"
#include "v8.h"

class TestClass
{
public:
    explicit TestClass(int p)
    {
        std::cout << "TestClass(" << p << ")" << std::endl;
        X = p;
        ++CreatedCount;
    }

    static void Print(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    static int GetCreatedCount()
    {
        return CreatedCount;
    }

    static int AddAll(int a, int b, int c)
    {
        return a + b + c;
    }

    int Add(int a, int b)
    {
        std::cout << "Add(" << a << "," << b << ")" << std::endl;
        return a + b;
    }

    int SetX(int value)
    {
        X = value;
        return X;
    }

    int Scale(int factor)
    {
        X *= factor;
        return X;
    }

    std::string Describe() const
    {
        return std::string("TestClass(") + std::to_string(X) + ")";
    }

    int X;
    static int CreatedCount;
};

int TestClass::CreatedCount = 0;

UsingCppType(TestClass);

static void RegisterExampleBindings()
{
    puerts::DefineClass<TestClass>()
        .Constructor<int>()
        .Function("Print", MakeFunction(&TestClass::Print))
        .Function("GetCreatedCount", MakeFunction(&TestClass::GetCreatedCount))
        .Function("AddAll", MakeFunction(&TestClass::AddAll))
        .Property("X", MakeProperty(&TestClass::X))
        .Method("Add", MakeFunction(&TestClass::Add))
        .Method("SetX", MakeFunction(&TestClass::SetX))
        .Method("Scale", MakeFunction(&TestClass::Scale))
        .Method("Describe", MakeFunction(&TestClass::Describe))
        .Register();
}

static std::string ReadTextFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return std::string();
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

static std::string ResolveScriptPath()
{
    namespace fs = std::filesystem;

    const std::vector<fs::path> script_candidates = {
        fs::path("JavaScript") / "hello-world.js",
    };

    auto is_project_root = [](const fs::path& dir) {
        return fs::exists(dir / "CMakeLists.txt") && fs::exists(dir / "puerts") &&
               (fs::exists(dir / "JavaScript") || fs::exists(dir / "Javascript"));
    };

    fs::path project_root;
    fs::path cursor = fs::current_path();
    while (true)
    {
        if (is_project_root(cursor))
        {
            project_root = cursor;
            break;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor)
        {
            break;
        }
        cursor = cursor.parent_path();
    }

    std::vector<std::string> candidates;
    if (!project_root.empty())
    {
        for (const auto& relative_path : script_candidates)
        {
            candidates.emplace_back((project_root / relative_path).string());
        }
    }
    else
    {
        for (const auto& relative_path : script_candidates)
        {
            candidates.emplace_back(relative_path.string());
        }
    }

    for (const auto& candidate : candidates)
    {
        std::ifstream file(candidate, std::ios::binary);
        if (file.good())
        {
            return candidate;
        }
    }

    std::ostringstream message;
    message << "can not find script file. tried:";
    for (const auto& candidate : candidates)
    {
        message << "\n  - " << candidate;
    }
    throw std::runtime_error(message.str());
}

struct ProgramOptions
{
    std::string ScriptPath;
    std::string DtsOutputPath;
    bool DtsOnly = false;
};

static ProgramOptions ParseProgramOptions(int argc, char* argv[])
{
    ProgramOptions Options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string Arg = argv[i];
        if (Arg == "--script")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--script requires a path");
            }
            Options.ScriptPath = argv[++i];
        }
        else if (Arg == "--dts-out")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("--dts-out requires a path");
            }
            Options.DtsOutputPath = argv[++i];
        }
        else if (Arg == "--dts-only")
        {
            Options.DtsOnly = true;
        }
        else if (!Arg.empty() && Arg[0] == '-')
        {
            throw std::runtime_error("unknown option: " + Arg);
        }
        else if (Options.ScriptPath.empty())
        {
            Options.ScriptPath = Arg;
        }
        else
        {
            throw std::runtime_error("unexpected positional argument: " + Arg);
        }
    }

    return Options;
}

static void ReportException(v8::Isolate* isolate, v8::TryCatch& try_catch)
{
    std::string message = "unknown exception";
    if (!try_catch.Exception().IsEmpty())
    {
        message = *v8::String::Utf8Value(isolate, try_catch.Exception());
    }

    auto context = isolate->GetCurrentContext();
    if (!try_catch.Message().IsEmpty())
    {
        auto script_name = try_catch.Message()->GetScriptOrigin().ResourceName();
        std::string script = script_name.IsEmpty() ? "<script>" : *v8::String::Utf8Value(isolate, script_name);
        int line = try_catch.Message()->GetLineNumber(context).FromMaybe(-1);
        std::cerr << script << ":" << line << ": " << message << std::endl;
    }
    else
    {
        std::cerr << message << std::endl;
    }

    if (!try_catch.StackTrace(context).IsEmpty())
    {
        auto maybe_stack = try_catch.StackTrace(context);
        if (!maybe_stack.IsEmpty() && maybe_stack.ToLocalChecked()->IsString())
        {
            std::cerr << *v8::String::Utf8Value(isolate, maybe_stack.ToLocalChecked()) << std::endl;
        }
    }
}

int main(int argc, char* argv[])
{
    ProgramOptions Options;
    try
    {
        Options = ParseProgramOptions(argc, argv);
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return 1;
    }

    RegisterExampleBindings();

    if (!Options.DtsOutputPath.empty())
    {
        std::string ErrorMessage;
        if (!puerts::GenerateTypeScriptDeclarationToFile(Options.DtsOutputPath, &ErrorMessage))
        {
            std::cerr << ErrorMessage << std::endl;
            return 1;
        }
        std::cout << "generated declaration: " << Options.DtsOutputPath << std::endl;
    }

    if (Options.DtsOnly)
    {
        return 0;
    }

    // Initialize V8.
    v8::Platform* platform = v8::platform::NewDefaultPlatform_Without_Stl();
    v8::V8::InitializePlatform(platform);
    v8::V8::Initialize();

    // Create a new Isolate and make it the current one.
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    int ExitCode = 0;
    bool ShouldRunScript = true;

    puerts::FCppObjectMapper cppObjectMapper;
    {
        v8::Isolate::Scope isolate_scope(isolate);

        // Create a stack-allocated handle scope.
        v8::HandleScope handle_scope(isolate);

        // Create a new context.
        v8::Local<v8::Context> context = v8::Context::New(isolate);

        // Enter the context for compiling and running the hello world script.
        v8::Context::Scope context_scope(context);

        cppObjectMapper.Initialize(isolate, context);
        isolate->SetData(MAPPER_ISOLATE_DATA_POS, static_cast<puerts::ICppObjectMapper*>(&cppObjectMapper));
        context
            ->Global()
            ->Set(context, v8::String::NewFromUtf8(isolate, "loadCppType").ToLocalChecked(),
                v8::FunctionTemplate::New(isolate,
                    [](const v8::FunctionCallbackInfo<v8::Value>& info)
                    {
                        auto pom = static_cast<puerts::FCppObjectMapper*>((v8::Local<v8::External>::Cast(info.Data()))->Value());
                        pom->LoadCppType(info);
                    },
                    v8::External::New(isolate, &cppObjectMapper))
                    ->GetFunction(context)
                    .ToLocalChecked())
            .Check();

        if (ShouldRunScript)
        {
            try
            {
                const std::string script_path = Options.ScriptPath.empty() ? ResolveScriptPath() : Options.ScriptPath;
                const std::string script_source = ReadTextFile(script_path);
                if (script_source.empty())
                {
                    std::cerr << "script file is empty: " << script_path << std::endl;
                    ExitCode = 1;
                }
                else
                {
                    v8::TryCatch try_catch(isolate);
                    auto source = v8::String::NewFromUtf8(isolate, script_source.c_str()).ToLocalChecked();
                    auto name = v8::String::NewFromUtf8(isolate, script_path.c_str()).ToLocalChecked();
                    v8::ScriptOrigin origin(isolate, name);
                    v8::Local<v8::Script> script;
                    if (!v8::Script::Compile(context, source, &origin).ToLocal(&script))
                    {
                        ReportException(isolate, try_catch);
                        ExitCode = 1;
                    }
                    else
                    {
                        v8::MaybeLocal<v8::Value> run_result = script->Run(context);
                        if (run_result.IsEmpty())
                        {
                            ReportException(isolate, try_catch);
                            ExitCode = 1;
                        }
                    }
                }
            }
            catch (const std::exception& ex)
            {
                std::cerr << ex.what() << std::endl;
                ExitCode = 1;
            }
        }

        cppObjectMapper.UnInitialize(isolate);
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    v8::platform::DeletePlatform_Without_Stl(platform);
    delete create_params.array_buffer_allocator;
    return ExitCode;
}
