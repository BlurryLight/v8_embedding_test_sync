/*
 * Tencent is pleased to support the open source community by making Puerts available.
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.  All rights reserved.
 * Puerts is licensed under the BSD 3-Clause License, except for the third-party components listed in the file 'LICENSE' which may
 * be subject to their corresponding license terms. This file is subject to the terms and conditions defined in file 'LICENSE',
 * which is part of this source code package.
 */

#include "JSClassRegister.h"
#if USING_IN_UNREAL_ENGINE
#include "UObject/Class.h"
#endif
#include <cstring>
#include <map>

namespace PUERTS_NAMESPACE
{
template <class T>
static T* PropertyInfoDuplicate(T* Arr)
{
    if (Arr == nullptr)
        return nullptr;
    int Count = 0;
    while (true)
    {
        if (Arr[Count++].Name == nullptr)
            break;
    }
    T* Ret = new T[Count];
    ::memcpy(Ret, Arr, sizeof(T) * Count);
    return Ret;
}

static JSClassDefinition* JSClassDefinitionDuplicate(const JSClassDefinition* ClassDefinition)
{
    auto Ret = new JSClassDefinition;
    ::memcpy(Ret, ClassDefinition, sizeof(JSClassDefinition));
    Ret->Methods = PropertyInfoDuplicate(ClassDefinition->Methods);
    Ret->Functions = PropertyInfoDuplicate(ClassDefinition->Functions);
    Ret->Properties = PropertyInfoDuplicate(ClassDefinition->Properties);
    Ret->Variables = PropertyInfoDuplicate(ClassDefinition->Variables);
    Ret->ConstructorInfos = PropertyInfoDuplicate(ClassDefinition->ConstructorInfos);
    Ret->MethodInfos = PropertyInfoDuplicate(ClassDefinition->MethodInfos);
    Ret->FunctionInfos = PropertyInfoDuplicate(ClassDefinition->FunctionInfos);
    Ret->PropertyInfos = PropertyInfoDuplicate(ClassDefinition->PropertyInfos);
    Ret->VariableInfos = PropertyInfoDuplicate(ClassDefinition->VariableInfos);
    return Ret;
}

static void JSClassDefinitionDelete(JSClassDefinition* ClassDefinition)
{
    delete[] ClassDefinition->Methods;
    delete[] ClassDefinition->Functions;
    delete[] ClassDefinition->Properties;
    delete[] ClassDefinition->Variables;
    delete[] ClassDefinition->ConstructorInfos;
    delete[] ClassDefinition->MethodInfos;
    delete[] ClassDefinition->FunctionInfos;
    delete[] ClassDefinition->PropertyInfos;
    delete[] ClassDefinition->VariableInfos;
    delete ClassDefinition;
}

class JSClassRegister
{
public:
    ~JSClassRegister()
    {
        for (auto& KV : CDataIdToClassDefinition)
        {
            JSClassDefinitionDelete(KV.second);
        }
        CDataIdToClassDefinition.clear();
#if USING_IN_UNREAL_ENGINE
        for (auto& KV : StructNameToClassDefinition)
        {
            JSClassDefinitionDelete(KV.second);
        }
        StructNameToClassDefinition.clear();
#endif
    }

    void RegisterClass(const JSClassDefinition& ClassDefinition)
    {
        if (ClassDefinition.TypeId && ClassDefinition.ScriptName)
        {
            auto Iter = CDataIdToClassDefinition.find(ClassDefinition.TypeId);
            if (Iter != CDataIdToClassDefinition.end())
            {
                JSClassDefinitionDelete(Iter->second);
            }
            CDataIdToClassDefinition[ClassDefinition.TypeId] = JSClassDefinitionDuplicate(&ClassDefinition);
            PString SN = ClassDefinition.ScriptName;
            CDataNameToClassDefinition[SN] = CDataIdToClassDefinition[ClassDefinition.TypeId];
            CDataIdToClassDefinition[ClassDefinition.TypeId]->ScriptName = CDataNameToClassDefinition.find(SN)->first.c_str();
        }
#if USING_IN_UNREAL_ENGINE
        else if (ClassDefinition.UETypeName)
        {
            FString SN = UTF8_TO_TCHAR(ClassDefinition.UETypeName);
            auto Iter = StructNameToClassDefinition.find(SN);
            if (Iter != StructNameToClassDefinition.end())
            {
                JSClassDefinitionDelete(Iter->second);
            }
            StructNameToClassDefinition[SN] = JSClassDefinitionDuplicate(&ClassDefinition);
        }
#endif
    }

    void SetClassTypeInfo(const void* TypeId, const NamedFunctionInfo* ConstructorInfos, const NamedFunctionInfo* MethodInfos,
        const NamedFunctionInfo* FunctionInfos, const NamedPropertyInfo* PropertyInfos, const NamedPropertyInfo* VariableInfos)
    {
        auto ClassDef = const_cast<JSClassDefinition*>(FindClassByID(TypeId));
        if (!ClassDef)
        {
            return;
        }
        delete[] ClassDef->ConstructorInfos;
        delete[] ClassDef->MethodInfos;
        delete[] ClassDef->FunctionInfos;
        delete[] ClassDef->PropertyInfos;
        delete[] ClassDef->VariableInfos;
        ClassDef->ConstructorInfos = PropertyInfoDuplicate(const_cast<NamedFunctionInfo*>(ConstructorInfos));
        ClassDef->MethodInfos = PropertyInfoDuplicate(const_cast<NamedFunctionInfo*>(MethodInfos));
        ClassDef->FunctionInfos = PropertyInfoDuplicate(const_cast<NamedFunctionInfo*>(FunctionInfos));
        ClassDef->PropertyInfos = PropertyInfoDuplicate(const_cast<NamedPropertyInfo*>(PropertyInfos));
        ClassDef->VariableInfos = PropertyInfoDuplicate(const_cast<NamedPropertyInfo*>(VariableInfos));
    }

    void ForeachRegisterClass(std::function<void(const JSClassDefinition* ClassDefinition)> Callback)
    {
        for (auto& KV : CDataNameToClassDefinition)
        {
            Callback(KV.second);
        }
#if USING_IN_UNREAL_ENGINE
        for (auto& KV : StructNameToClassDefinition)
        {
            Callback(KV.second);
        }
#endif
    }

    const JSClassDefinition* FindClassByID(const void* TypeId)
    {
        if (!TypeId)
        {
            return nullptr;
        }
        auto Iter = CDataIdToClassDefinition.find(TypeId);
        return Iter == CDataIdToClassDefinition.end() ? nullptr : Iter->second;
    }

    void OnClassNotFound(ClassNotFoundCallback InCallback)
    {
        ClassNotFound = InCallback;
    }

    const JSClassDefinition* LoadClassByID(const void* TypeId)
    {
        auto ClassDef = FindClassByID(TypeId);
        if (!ClassDef && ClassNotFound)
        {
            if (!ClassNotFound(TypeId))
            {
                return nullptr;
            }
            ClassDef = FindClassByID(TypeId);
        }
        return ClassDef;
    }

    const JSClassDefinition* FindCppTypeClassByName(const PString& Name)
    {
        auto Iter = CDataNameToClassDefinition.find(Name);
        return Iter == CDataNameToClassDefinition.end() ? nullptr : Iter->second;
    }

    void RegisterAddon(const PString& Name, AddonRegisterFunc RegisterFunc)
    {
        AddonRegisterInfos[Name] = RegisterFunc;
    }

    AddonRegisterFunc FindAddonRegisterFunc(const PString& Name)
    {
        auto Iter = AddonRegisterInfos.find(Name);
        return Iter == AddonRegisterInfos.end() ? nullptr : Iter->second;
    }

#if USING_IN_UNREAL_ENGINE
    const JSClassDefinition* FindClassByType(UStruct* Type)
    {
        auto Iter = StructNameToClassDefinition.find(Type->GetName());
        return Iter == StructNameToClassDefinition.end() ? nullptr : Iter->second;
    }
#endif

private:
    std::map<const void*, JSClassDefinition*> CDataIdToClassDefinition;
    std::map<PString, JSClassDefinition*> CDataNameToClassDefinition;
    ClassNotFoundCallback ClassNotFound = nullptr;
    std::map<PString, AddonRegisterFunc> AddonRegisterInfos;
#if USING_IN_UNREAL_ENGINE
    std::map<FString, JSClassDefinition*> StructNameToClassDefinition;
#endif
};

static JSClassRegister* GetJSClassRegister()
{
    static JSClassRegister S_JSClassRegister;
    return &S_JSClassRegister;
}

void RegisterJSClass(const JSClassDefinition& ClassDefinition)
{
    GetJSClassRegister()->RegisterClass(ClassDefinition);
}

void SetClassTypeInfo(const void* TypeId, const NamedFunctionInfo* ConstructorInfos, const NamedFunctionInfo* MethodInfos,
    const NamedFunctionInfo* FunctionInfos, const NamedPropertyInfo* PropertyInfos, const NamedPropertyInfo* VariableInfos)
{
    GetJSClassRegister()->SetClassTypeInfo(TypeId, ConstructorInfos, MethodInfos, FunctionInfos, PropertyInfos, VariableInfos);
}

void ForeachRegisterClass(std::function<void(const JSClassDefinition* ClassDefinition)> Callback)
{
    GetJSClassRegister()->ForeachRegisterClass(Callback);
}

const JSClassDefinition* FindClassByID(const void* TypeId)
{
    return GetJSClassRegister()->FindClassByID(TypeId);
}

void OnClassNotFound(ClassNotFoundCallback Callback)
{
    GetJSClassRegister()->OnClassNotFound(Callback);
}

const JSClassDefinition* LoadClassByID(const void* TypeId)
{
    return GetJSClassRegister()->LoadClassByID(TypeId);
}

const JSClassDefinition* FindCppTypeClassByName(const PString& Name)
{
    return GetJSClassRegister()->FindCppTypeClassByName(Name);
}

bool TraceObjectLifecycle(const void* TypeId, ObjectEnterFunc OnEnter, ObjectExitFunc OnExit)
{
    if (auto ClassDef = const_cast<JSClassDefinition*>(GetJSClassRegister()->FindClassByID(TypeId)))
    {
        ClassDef->OnEnter = OnEnter;
        ClassDef->OnExit = OnExit;
        return true;
    }
    return false;
}

void RegisterAddon(const char* Name, AddonRegisterFunc RegisterFunc)
{
    GetJSClassRegister()->RegisterAddon(Name, RegisterFunc);
}

AddonRegisterFunc FindAddonRegisterFunc(const PString& Name)
{
    return GetJSClassRegister()->FindAddonRegisterFunc(Name);
}

#if USING_IN_UNREAL_ENGINE
const JSClassDefinition* FindClassByType(UStruct* Type)
{
    return GetJSClassRegister()->FindClassByType(Type);
}
#endif

}    // namespace PUERTS_NAMESPACE
