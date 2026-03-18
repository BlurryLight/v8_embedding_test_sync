#include "JsEnv.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "CppObjectMapper.h"
#include "DataTransfer.h"
#include "DeclarationGenerator.h"

#include "libplatform/libplatform.h"
#include "v8.h"

namespace
{

std::string ReadTextFile(const std::string& path)
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

std::string NormalizePathString(const std::filesystem::path& path)
{
    return path.lexically_normal().make_preferred().string();
}

bool HasDirectorySeparator(const std::string& value)
{
    return value.find('/') != std::string::npos || value.find('\\') != std::string::npos;
}

bool IsFileExists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
}

std::filesystem::path ResolveProjectRoot()
{
    namespace fs = std::filesystem;

    auto is_project_root = [](const fs::path& dir) {
        return fs::exists(dir / "CMakeLists.txt") && fs::exists(dir / "puerts") &&
               (fs::exists(dir / "JavaScript") || fs::exists(dir / "Javascript") || fs::exists(dir / "TypeScript"));
    };

    fs::path cursor = fs::current_path();
    while (true)
    {
        if (is_project_root(cursor))
        {
            return cursor;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor)
        {
            break;
        }
        cursor = cursor.parent_path();
    }

    return fs::current_path();
}

std::optional<std::filesystem::path> ResolveExistingFile(const std::filesystem::path& candidate_without_extension)
{
    static const char* kExtensions[] = {"", ".js", ".json", ".cjs", ".mjs"};

    for (const char* extension : kExtensions)
    {
        std::filesystem::path candidate = candidate_without_extension;
        if (extension[0] != '\0')
        {
            candidate += extension;
        }
        if (IsFileExists(candidate))
        {
            return candidate;
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> ResolveAsFileOrDirectory(const std::filesystem::path& base_path)
{
    namespace fs = std::filesystem;

    if (auto direct_file = ResolveExistingFile(base_path))
    {
        return direct_file;
    }

    std::error_code ec;
    if (fs::exists(base_path, ec) && fs::is_directory(base_path, ec))
    {
        const fs::path package_json = base_path / "package.json";
        if (IsFileExists(package_json))
        {
            return package_json;
        }

        if (auto index_file = ResolveExistingFile(base_path / "index"))
        {
            return index_file;
        }
    }

    return std::nullopt;
}

class ModuleResolver
{
public:
    explicit ModuleResolver(std::filesystem::path InProjectRoot) : ProjectRoot(std::move(InProjectRoot))
    {
        const auto js_root = ProjectRoot / "JavaScript";
        const auto ts_root = ProjectRoot / "TypeScript";
        const auto typing_root = ProjectRoot / "Typing";

        SearchRoots.push_back(js_root);
        if (js_root != ProjectRoot)
        {
            SearchRoots.push_back(ProjectRoot);
        }
        if (ts_root != js_root)
        {
            SearchRoots.push_back(ts_root);
        }
        if (typing_root != js_root)
        {
            SearchRoots.push_back(typing_root);
        }
    }

    std::optional<std::filesystem::path> Resolve(const std::string& module_name, const std::string& requiring_dir) const
    {
        namespace fs = std::filesystem;

        std::vector<fs::path> base_dirs;
        if (module_name.rfind("./", 0) == 0 || module_name.rfind("../", 0) == 0)
        {
            base_dirs.push_back(requiring_dir.empty() ? CurrentScriptDirectory : fs::path(requiring_dir));
        }
        else if (HasDirectorySeparator(module_name) || fs::path(module_name).is_absolute())
        {
            if (fs::path(module_name).is_absolute())
            {
                base_dirs.push_back(fs::path(module_name).parent_path());
            }
            else
            {
                base_dirs.push_back(ProjectRoot);
            }
        }
        else
        {
            base_dirs = SearchRoots;
            base_dirs.push_back(CurrentScriptDirectory);
        }

        for (const auto& base_dir : base_dirs)
        {
            fs::path candidate = fs::path(module_name).is_absolute() ? fs::path(module_name) : base_dir / module_name;
            if (auto resolved = ResolveAsFileOrDirectory(candidate))
            {
                return resolved;
            }
        }

        return std::nullopt;
    }

    void SetCurrentScriptDirectory(const std::filesystem::path& path)
    {
        CurrentScriptDirectory = path;
    }

private:
    std::filesystem::path ProjectRoot;
    std::filesystem::path CurrentScriptDirectory;
    std::vector<std::filesystem::path> SearchRoots;
};

std::string ResolveScriptPath(const std::string& hint_path)
{
    namespace fs = std::filesystem;

    std::vector<fs::path> script_candidates = {
        // fs::path("JavaScript") / "main.js",
    };

    script_candidates.push_back(hint_path);

    fs::path project_root = ResolveProjectRoot();
    project_root /= "JavaScript";

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

v8::MaybeLocal<v8::Value> RunScriptString(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const std::string& script_source,
    const std::string& debug_path)
{
    auto source = v8::String::NewFromUtf8(isolate, script_source.c_str()).ToLocalChecked();
    auto name = v8::String::NewFromUtf8(isolate, debug_path.c_str()).ToLocalChecked();
    v8::ScriptOrigin origin(isolate, name);
    v8::Local<v8::Script> script;
    if (!v8::Script::Compile(context, source, &origin).ToLocal(&script))
    {
        return v8::MaybeLocal<v8::Value>();
    }
    return script->Run(context);
}

void ReportException(v8::Isolate* isolate, v8::TryCatch& try_catch)
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

}    // namespace

namespace PUERTS_NAMESPACE
{

ProgramOptions ParseProgramOptions(int argc, char* argv[])
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

int RunScriptProgram(const ProgramOptions& Options, RegisterBindingsCallback RegisterBindings)
{
    if (RegisterBindings)
    {
        RegisterBindings();
    }

    if (!Options.DtsOutputPath.empty())
    {
        std::string ErrorMessage;
        if (!GenerateTypeScriptDeclarationToFile(Options.DtsOutputPath, &ErrorMessage))
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

    v8::Platform* platform = nullptr;
    if(Options.bSingleThreadPlatform)
    {
        platform = v8::platform::NewSingleThreadedDefaultPlatform_Without_Stl();
    }
    else
    {
      platform = v8::platform::NewDefaultPlatform_Without_Stl();
    }
    v8::V8::InitializePlatform(platform);
    v8::V8::Initialize();

    v8::V8::EnableWorkerThreads(platform, 1);

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    int ExitCode = 0;

    FCppObjectMapper cppObjectMapper;
    ModuleResolver module_resolver(ResolveProjectRoot());
    {
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context);

        cppObjectMapper.Initialize(isolate, context);
        isolate->SetData(MAPPER_ISOLATE_DATA_POS, static_cast<ICppObjectMapper*>(&cppObjectMapper));

        context
            ->Global()
            ->Set(context, v8::String::NewFromUtf8(isolate, "loadCppType").ToLocalChecked(),
                v8::FunctionTemplate::New(isolate,
                    [](const v8::FunctionCallbackInfo<v8::Value>& info)
                    {
                        auto pom = static_cast<FCppObjectMapper*>((v8::Local<v8::External>::Cast(info.Data()))->Value());
                        pom->LoadCppType(info);
                    },
                    v8::External::New(isolate, &cppObjectMapper))
                    ->GetFunction(context)
                    .ToLocalChecked())
            .Check();
        context
            ->Global()
            ->Set(context, v8::String::NewFromUtf8(isolate, "loadCPPType").ToLocalChecked(),
                context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "loadCppType").ToLocalChecked()).ToLocalChecked())
            .Check();

        context
            ->Global()
            ->Set(context, v8::String::NewFromUtf8(isolate, "__tgjsEvalScript").ToLocalChecked(),
                v8::FunctionTemplate::New(
                    isolate,
                    [](const v8::FunctionCallbackInfo<v8::Value>& info)
                    {
                        auto isolate = info.GetIsolate();
                        auto context = isolate->GetCurrentContext();
                        if (info.Length() < 1 || !info[0]->IsString())
                        {
                            isolate->ThrowException(v8::Exception::TypeError(
                                v8::String::NewFromUtf8Literal(isolate, "__tgjsEvalScript expects script source")));
                            return;
                        }

                        const std::string script_source = *v8::String::Utf8Value(isolate, info[0]);
                        std::string debug_path = "<eval>";
                        if (info.Length() >= 2 && info[1]->IsString())
                        {
                            debug_path = *v8::String::Utf8Value(isolate, info[1]);
                        }

                        v8::TryCatch try_catch(isolate);
                        auto maybe_value = RunScriptString(isolate, context, script_source, debug_path);
                        if (maybe_value.IsEmpty())
                        {
                            try_catch.ReThrow();
                            return;
                        }

                        info.GetReturnValue().Set(maybe_value.ToLocalChecked());
                    })
                    ->GetFunction(context)
                    .ToLocalChecked())
            .Check();

        context
            ->Global()
            ->Set(context, v8::String::NewFromUtf8(isolate, "__tgjsLoadModule").ToLocalChecked(),
                v8::FunctionTemplate::New(
                    isolate,
                    [](const v8::FunctionCallbackInfo<v8::Value>& info)
                    {
                        auto isolate = info.GetIsolate();
                        if (info.Length() < 1 || !info[0]->IsString())
                        {
                            isolate->ThrowException(v8::Exception::TypeError(
                                v8::String::NewFromUtf8Literal(isolate, "__tgjsLoadModule expects a path")));
                            return;
                        }

                        const std::string full_path = *v8::String::Utf8Value(isolate, info[0]);
                        const std::string script_source = ReadTextFile(full_path);
                        if (script_source.empty() && !IsFileExists(full_path))
                        {
                            isolate->ThrowException(v8::Exception::Error(
                                v8::String::NewFromUtf8(isolate, ("can not load module: " + full_path).c_str()).ToLocalChecked()));
                            return;
                        }

                        info.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, script_source.c_str()).ToLocalChecked());
                    })
                    ->GetFunction(context)
                    .ToLocalChecked())
            .Check();

        context
            ->Global()
            ->Set(context, v8::String::NewFromUtf8(isolate, "__tgjsSearchModule").ToLocalChecked(),
                v8::FunctionTemplate::New(
                    isolate,
                    [](const v8::FunctionCallbackInfo<v8::Value>& info)
                    {
                        auto isolate = info.GetIsolate();
                        auto context = isolate->GetCurrentContext();
                        auto resolver = static_cast<ModuleResolver*>(v8::Local<v8::External>::Cast(info.Data())->Value());
                        if (info.Length() < 1 || !info[0]->IsString())
                        {
                            isolate->ThrowException(v8::Exception::TypeError(
                                v8::String::NewFromUtf8Literal(isolate, "__tgjsSearchModule expects a module name")));
                            return;
                        }

                        const std::string module_name = *v8::String::Utf8Value(isolate, info[0]);
                        std::string requiring_dir;
                        if (info.Length() >= 2 && info[1]->IsString())
                        {
                            requiring_dir = *v8::String::Utf8Value(isolate, info[1]);
                        }

                        auto resolved = resolver->Resolve(module_name, requiring_dir);
                        if (!resolved)
                        {
                            return;
                        }

                        auto result = v8::Array::New(isolate, 2);
                        const std::string normalized = NormalizePathString(*resolved);
                        result->Set(context, 0, v8::String::NewFromUtf8(isolate, normalized.c_str()).ToLocalChecked()).Check();
                        result->Set(context, 1, v8::String::NewFromUtf8(isolate, normalized.c_str()).ToLocalChecked()).Check();
                        info.GetReturnValue().Set(result);
                    },
                    v8::External::New(isolate, &module_resolver))
                    ->GetFunction(context)
                    .ToLocalChecked())
            .Check();

        context
            ->Global()
            ->Set(context, v8::String::NewFromUtf8(isolate, "__tgjsFindModule").ToLocalChecked(),
                v8::FunctionTemplate::New(
                    isolate,
                    [](const v8::FunctionCallbackInfo<v8::Value>& info)
                    {
                        info.GetReturnValue().Set(v8::Undefined(info.GetIsolate()));
                    })
                    ->GetFunction(context)
                    .ToLocalChecked())
            .Check();

        try
        {
            const std::string script_path =  ResolveScriptPath(Options.ScriptPath.empty() ? "JavaScript/main.js" : Options.ScriptPath);
            module_resolver.SetCurrentScriptDirectory(std::filesystem::path(script_path).parent_path());
            const std::string script_source = ReadTextFile(script_path);
            if (script_source.empty())
            {
                std::cerr << "script file is empty: " << script_path << std::endl;
                ExitCode = 1;
            }
            else
            {
                v8::TryCatch try_catch(isolate);
                auto RunInternalScript = [&](std::string_view script_name)
                {
                    auto init_env_path = NormalizePathString(ResolveProjectRoot() / "JavaScript" / script_name);
                    const auto init_env_source = ReadTextFile(init_env_path);
                    if (RunScriptString(isolate, context, init_env_source, init_env_path).IsEmpty())
                    {
                        ReportException(isolate, try_catch);
                        ExitCode = 1;
                    }
                };

                RunInternalScript("puerts/modular.js");
                RunInternalScript("internal/initEnv.js");
                if (RunScriptString(isolate, context, script_source, script_path).IsEmpty())
                {
                    ReportException(isolate, try_catch);
                    ExitCode = 1;
                }
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << ex.what() << std::endl;
            ExitCode = 1;
        }

        cppObjectMapper.UnInitialize(isolate);
    }

    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    v8::platform::DeletePlatform_Without_Stl(platform);
    delete create_params.array_buffer_allocator;

    return ExitCode;
}

}    // namespace PUERTS_NAMESPACE
