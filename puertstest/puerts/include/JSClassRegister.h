/*
 * Tencent is pleased to support the open source community by making Puerts available.
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.  All rights reserved.
 * Puerts is licensed under the BSD 3-Clause License, except for the third-party components listed in the file 'LICENSE' which may
 * be subject to their corresponding license terms. This file is subject to the terms and conditions defined in file 'LICENSE',
 * which is part of this source code package.
 */

#pragma once

#include "functional"

#if USING_IN_UNREAL_ENGINE
#include "CoreMinimal.h"
#else
#define JSENV_API
#define FORCEINLINE V8_INLINE
#define UPTRINT uintptr_t
#endif

#include <string>

#include "NamespaceDef.h"
#include "PString.h"

#pragma warning(push, 0)
#include "v8.h"
#pragma warning(pop)

#include "TypeInfo.hpp"

namespace PUERTS_NAMESPACE
{
struct JSENV_API JSFunctionInfo
{
    JSFunctionInfo() : Name(nullptr), Callback(nullptr)
    {
    }

    JSFunctionInfo(
        const char* InName, v8::FunctionCallback InCallback, void* InData = nullptr, const CFunctionInfo* InReflectionInfo = nullptr)
        : Name(InName), Callback(InCallback), Data(InData), ReflectionInfo(InReflectionInfo)
    {
    }

    const char* Name;
    v8::FunctionCallback Callback;
    void* Data = nullptr;
    const CFunctionInfo* ReflectionInfo = nullptr;
};

struct JSENV_API JSPropertyInfo
{
    JSPropertyInfo() : Name(nullptr), Getter(nullptr), Setter(nullptr)
    {
    }

    JSPropertyInfo(const char* InName, v8::FunctionCallback InGetter, v8::FunctionCallback InSetter, void* InGetterData = nullptr,
        void* InSetterData = nullptr)
        : Name(InName), Getter(InGetter), Setter(InSetter), GetterData(InGetterData), SetterData(InSetterData)
    {
    }

    const char* Name;
    v8::FunctionCallback Getter;
    v8::FunctionCallback Setter;
    void* GetterData = nullptr;
    void* SetterData = nullptr;
};

typedef void (*FinalizeFunc)(void* Ptr);

typedef void* (*InitializeFunc)(const v8::FunctionCallbackInfo<v8::Value>& Info);

struct NamedFunctionInfo
{
    const char* Name;
    const CFunctionInfo* Type;
};

struct NamedPropertyInfo
{
    const char* Name;
    const CTypeInfo* Type;
};

typedef bool (*ClassNotFoundCallback)(const void* TypeId);
typedef void* (*ObjectEnterFunc)(void* Ptr, void* ClassData, void* EnvPrivateData);
typedef void (*ObjectExitFunc)(void* Ptr, void* ClassData, void* EnvPrivateData, void* UserData);

struct JSENV_API JSClassDefinition
{
    const void* TypeId;
    const void* SuperTypeId;
    const char* ScriptName;
    const char* UETypeName;
    InitializeFunc Initialize;
    JSFunctionInfo* Methods;       //成员方法
    JSFunctionInfo* Functions;     //静态方法
    JSPropertyInfo* Properties;    //成员属性
    JSPropertyInfo* Variables;     //静态属性
    FinalizeFunc Finalize;
    // int InternalFieldCount;
    NamedFunctionInfo* ConstructorInfos;
    NamedFunctionInfo* MethodInfos;
    NamedFunctionInfo* FunctionInfos;
    NamedPropertyInfo* PropertyInfos;
    NamedPropertyInfo* VariableInfos;
    void* Data = nullptr;
    ObjectEnterFunc OnEnter = nullptr;
    ObjectExitFunc OnExit = nullptr;
};

#define JSClassEmptyDefinition                      \
    {                                               \
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 \
    }

void JSENV_API RegisterJSClass(const JSClassDefinition& ClassDefinition);

void JSENV_API SetClassTypeInfo(const void* TypeId, const NamedFunctionInfo* ConstructorInfos, const NamedFunctionInfo* MethodInfos,
    const NamedFunctionInfo* FunctionInfos, const NamedPropertyInfo* PropertyInfos, const NamedPropertyInfo* VariableInfos);

void JSENV_API ForeachRegisterClass(std::function<void(const JSClassDefinition* ClassDefinition)>);

JSENV_API const JSClassDefinition* FindClassByID(const void* TypeId);

JSENV_API void OnClassNotFound(ClassNotFoundCallback Callback);

JSENV_API const JSClassDefinition* LoadClassByID(const void* TypeId);

const JSClassDefinition* FindCppTypeClassByName(const PString& Name);

JSENV_API bool TraceObjectLifecycle(const void* TypeId, ObjectEnterFunc OnEnter, ObjectExitFunc OnExit);

typedef void (*AddonRegisterFunc)(v8::Local<v8::Context> Context, v8::Local<v8::Object> Exports);

AddonRegisterFunc FindAddonRegisterFunc(const PString& Name);

void RegisterAddon(const char* Name, AddonRegisterFunc RegisterFunc);

#if USING_IN_UNREAL_ENGINE
JSENV_API const JSClassDefinition* FindClassByType(UStruct* Type);
#endif

}    // namespace PUERTS_NAMESPACE

#define PUERTS_MODULE(Name, RegFunc)                 \
    static struct FAutoRegisterFor##Name             \
    {                                                \
        FAutoRegisterFor##Name()                     \
        {                                            \
            PUERTS_NAMESPACE::RegisterAddon(#Name, (RegFunc)); \
        }                                            \
    } _AutoRegisterFor##Name
