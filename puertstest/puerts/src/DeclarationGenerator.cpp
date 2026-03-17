/*
 * Tencent is pleased to support the open source community by making Puerts available.
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.  All rights reserved.
 * Puerts is licensed under the BSD 3-Clause License, except for the third-party components listed in the file 'LICENSE' which may
 * be subject to their corresponding license terms. This file is subject to the terms and conditions defined in file 'LICENSE',
 * which is part of this source code package.
 */

#include "DeclarationGenerator.h"
#include "JSClassRegister.h"
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>

namespace PUERTS_NAMESPACE
{
namespace
{
bool HadNamespace(const char* Name)
{
    return std::strncmp(Name, "UE.", 3) == 0 || std::strncmp(Name, "cpp.", 4) == 0;
}

bool HasUENamespace(const char* Name)
{
    return std::strncmp(Name, "UE.", 3) == 0;
}

std::string GetNamePrefix(const CTypeInfo* TypeInfo)
{
    if (TypeInfo->IsUEType() && !HadNamespace(TypeInfo->Name()))
    {
        return "UE.";
    }

    if (TypeInfo->IsObjectType() && !HadNamespace(TypeInfo->Name()))
    {
        return "cpp.";
    }

    return "";
}

std::string GetName(const CTypeInfo* TypeInfo)
{
    return TypeInfo->Name();
}

void GenArguments(const CFunctionInfo* Type, std::ostringstream& Stream)
{
    for (unsigned int i = 0; i < Type->ArgumentCount(); ++i)
    {
        if (i != 0)
        {
            Stream << ", ";
        }

        auto ArgInfo = Type->Argument(i);
        Stream << "p" << i;
        if (i >= Type->ArgumentCount() - Type->DefaultCount())
        {
            Stream << "?";
        }
        Stream << ": ";

        if (std::strcmp(ArgInfo->Name(), "cstring") != 0 && !ArgInfo->IsUEType() && !ArgInfo->IsObjectType() &&
            ArgInfo->IsPointer())
        {
            Stream << "ArrayBuffer";
        }
        else
        {
            const bool IsReference = ArgInfo->IsRef();
            const bool IsNullable = !IsReference && ArgInfo->IsPointer();
            if (IsNullable)
            {
                Stream << "$Nullable<";
            }
            if (IsReference)
            {
                Stream << "$Ref<";
            }

            Stream << GetNamePrefix(ArgInfo) << GetName(ArgInfo);

            if (IsNullable)
            {
                Stream << ">";
            }
            if (IsReference)
            {
                Stream << ">";
            }
        }
    }
}

class FDeclarationBuilder
{
public:
    void Begin()
    {
        Output << "declare module \"cpp\" {\n";
        // Output << "    import * as UE from \"ue\"\n";
        Output << "    import * as cpp from \"cpp\"\n";
        // Output << "    import {$Ref, $Nullable, cstring} from \"puerts\"\n\n";
    }

    void GenClass(const JSClassDefinition* ClassDefinition)
    {
        if (HasUENamespace(ClassDefinition->ScriptName))
        {
            return;
        }

        Output << "    ";

        auto ConstructorInfo = ClassDefinition->ConstructorInfos;
        const bool IsAbstract = !ConstructorInfo || !(ConstructorInfo->Name && ConstructorInfo->Type);
        if (IsAbstract)
        {
            Output << "abstract ";
        }

        Output << "class " << ClassDefinition->ScriptName;
        if (ClassDefinition->SuperTypeId)
        {
            auto SuperDefinition = FindClassByID(ClassDefinition->SuperTypeId);
            if (SuperDefinition && SuperDefinition->ScriptName)
            {
                Output << " extends " << SuperDefinition->ScriptName;
            }
        }
        Output << " {\n";

        std::set<std::string> AddedFunctions;

        while (ConstructorInfo && ConstructorInfo->Name && ConstructorInfo->Type)
        {
            std::ostringstream Tmp;
            Tmp << "        constructor(";
            GenArguments(ConstructorInfo->Type, Tmp);
            Tmp << ");\n";
            if (AddedFunctions.insert(Tmp.str()).second)
            {
                Output << Tmp.str();
            }
            ++ConstructorInfo;
        }

        auto PropertyInfo = ClassDefinition->PropertyInfos;
        while (PropertyInfo && PropertyInfo->Name && PropertyInfo->Type)
        {
            Output << "        " << PropertyInfo->Name << ": " << GetNamePrefix(PropertyInfo->Type) << GetName(PropertyInfo->Type)
                   << ";\n";
            ++PropertyInfo;
        }

        auto VariableInfo = ClassDefinition->VariableInfos;
        while (VariableInfo && VariableInfo->Name && VariableInfo->Type)
        {
            int Pos = static_cast<int>(VariableInfo - ClassDefinition->VariableInfos);
            Output << "        static " << (ClassDefinition->Variables[Pos].Setter ? "" : "readonly ") << VariableInfo->Name << ": "
                   << GetNamePrefix(VariableInfo->Type) << GetName(VariableInfo->Type) << ";\n";
            ++VariableInfo;
        }

        auto FunctionInfo = ClassDefinition->FunctionInfos;
        while (FunctionInfo && FunctionInfo->Name && FunctionInfo->Type)
        {
            std::ostringstream Tmp;
            Tmp << "        static " << FunctionInfo->Name;
            if (FunctionInfo->Type->Return())
            {
                Tmp << "(";
                GenArguments(FunctionInfo->Type, Tmp);
                auto ReturnType = FunctionInfo->Type->Return();
                Tmp << "): " << GetNamePrefix(ReturnType) << GetName(ReturnType) << ";\n";
            }
            else
            {
                Tmp << FunctionInfo->Type->CustomSignature() << ";\n";
            }
            if (AddedFunctions.insert(Tmp.str()).second)
            {
                Output << Tmp.str();
            }
            ++FunctionInfo;
        }

        auto MethodInfo = ClassDefinition->MethodInfos;
        while (MethodInfo && MethodInfo->Name && MethodInfo->Type)
        {
            std::ostringstream Tmp;
            Tmp << "        " << MethodInfo->Name;
            if (MethodInfo->Type->Return())
            {
                Tmp << "(";
                GenArguments(MethodInfo->Type, Tmp);
                auto ReturnType = MethodInfo->Type->Return();
                Tmp << "): " << GetNamePrefix(ReturnType) << GetName(ReturnType) << ";\n";
            }
            else
            {
                Tmp << MethodInfo->Type->CustomSignature() << ";\n";
            }
            if (AddedFunctions.insert(Tmp.str()).second)
            {
                Output << Tmp.str();
            }
            ++MethodInfo;
        }

        Output << "    }\n\n";
    }

    void End()
    {
        Output << "}\n";
    }

    std::string ToString() const
    {
        return Output.str();
    }

private:
    std::ostringstream Output;
};
}    // namespace

std::string GenerateTypeScriptDeclaration()
{
    FDeclarationBuilder Builder;
    Builder.Begin();

    ForeachRegisterClass(
        [&](const JSClassDefinition* ClassDefinition)
        {
            if (ClassDefinition->TypeId && ClassDefinition->ScriptName)
            {
                Builder.GenClass(ClassDefinition);
            }
        });

    Builder.End();
    return Builder.ToString();
}

bool GenerateTypeScriptDeclarationToFile(const std::string& OutputPath, std::string* OutErrorMessage)
{
    std::ofstream File(OutputPath, std::ios::binary | std::ios::trunc);
    if (!File)
    {
        if (OutErrorMessage)
        {
            *OutErrorMessage = "failed to open output file: " + OutputPath;
        }
        return false;
    }

    File << GenerateTypeScriptDeclaration();
    if (!File.good())
    {
        if (OutErrorMessage)
        {
            *OutErrorMessage = "failed to write declaration file: " + OutputPath;
        }
        return false;
    }

    return true;
}
}    // namespace PUERTS_NAMESPACE
