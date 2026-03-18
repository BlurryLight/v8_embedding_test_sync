#include <iostream>
#include <string>

#include "Binding.hpp"
#include "hello-world-bindings.h"

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

namespace puerts
{

void RegisterExampleBindings()
{
    DefineClass<TestClass>()
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

}
