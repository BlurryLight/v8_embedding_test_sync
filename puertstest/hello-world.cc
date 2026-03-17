// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "Binding.hpp"
#include "CppObjectMapper.h"

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

static std::string ResolveScriptPath(int argc, char* argv[])
{
    std::vector<std::string> candidates;
    if (argc > 1 && argv[1] && argv[1][0] != '\0')
    {
        candidates.emplace_back(argv[1]);
    }

    candidates.emplace_back("hello-world.ts");
    candidates.emplace_back(".\\hello-world.ts");
    candidates.emplace_back("..\\hello-world.ts");
    candidates.emplace_back("..\\..\\hello-world.ts");

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
    // Initialize V8.
    v8::Platform* platform = v8::platform::NewDefaultPlatform_Without_Stl();
    v8::V8::InitializePlatform(platform);
    v8::V8::Initialize();

    // Create a new Isolate and make it the current one.
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    
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

        try
        {
            const std::string script_path = ResolveScriptPath(argc, argv);
            const std::string script_source = ReadTextFile(script_path);
            if (script_source.empty())
            {
                std::cerr << "script file is empty: " << script_path << std::endl;
                cppObjectMapper.UnInitialize(isolate);
                isolate->Dispose();
                v8::V8::Dispose();
                v8::V8::DisposePlatform();
                v8::platform::DeletePlatform_Without_Stl(platform);
                delete create_params.array_buffer_allocator;
                return 1;
            }

            v8::TryCatch try_catch(isolate);
            auto source = v8::String::NewFromUtf8(isolate, script_source.c_str()).ToLocalChecked();
            auto name = v8::String::NewFromUtf8(isolate, script_path.c_str()).ToLocalChecked();
            v8::ScriptOrigin origin(isolate, name);
            v8::Local<v8::Script> script;
            if (!v8::Script::Compile(context, source, &origin).ToLocal(&script))
            {
                ReportException(isolate, try_catch);
                cppObjectMapper.UnInitialize(isolate);
                isolate->Dispose();
                v8::V8::Dispose();
                v8::V8::DisposePlatform();
                v8::platform::DeletePlatform_Without_Stl(platform);
                delete create_params.array_buffer_allocator;
                return 1;
            }

            v8::MaybeLocal<v8::Value> run_result = script->Run(context);
            if (run_result.IsEmpty())
            {
                ReportException(isolate, try_catch);
                cppObjectMapper.UnInitialize(isolate);
                isolate->Dispose();
                v8::V8::Dispose();
                v8::V8::DisposePlatform();
                v8::platform::DeletePlatform_Without_Stl(platform);
                delete create_params.array_buffer_allocator;
                return 1;
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << ex.what() << std::endl;
            cppObjectMapper.UnInitialize(isolate);
            isolate->Dispose();
            v8::V8::Dispose();
            v8::V8::DisposePlatform();
            v8::platform::DeletePlatform_Without_Stl(platform);
            delete create_params.array_buffer_allocator;
            return 1;
        }

        cppObjectMapper.UnInitialize(isolate);
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    v8::platform::DeletePlatform_Without_Stl(platform);
    delete create_params.array_buffer_allocator;
    return 0;
}
