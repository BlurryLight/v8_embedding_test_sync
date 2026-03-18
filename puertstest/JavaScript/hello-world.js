const { TestClass } = require("cpp");
const { assert, assertEqual, test } = require("./helpers/test-helper");

test(TestClass, "static methods are callable", function () {
    TestClass.Print("hello world from external script");
    assertEqual(TestClass.AddAll(1, 2, 3), 6, "AddAll result");
    assertEqual(TestClass.GetCreatedCount(), 0, "initial created count");
});

test(TestClass, "instance property read and write works", function () {
    const obj = new TestClass(123);
    assertEqual(obj.X, 123, "initial X");
    obj.X = 99;
    assertEqual(obj.X, 99, "updated X");
});

test(TestClass, "instance methods mutate and return values", function () {
    const obj = new TestClass(10);
    assertEqual(obj.Add(1, 3), 4, "Add result");
    assertEqual(obj.SetX(7), 7, "SetX result");
    assertEqual(obj.Scale(6), 42, "Scale result");
    assertEqual(obj.Describe(), "TestClass(42)", "Describe result");
});

test(TestClass, "multiple objects do not share state", function () {
    const left = new TestClass(5);
    const right = new TestClass(8);
    left.Scale(2);
    right.SetX(-3);
    assertEqual(left.X, 10, "left.X");
    assertEqual(right.X, -3, "right.X");
});

test(TestClass, "created count tracks constructor calls", function () {
    const before = TestClass.GetCreatedCount();
    const samples = [new TestClass(1), new TestClass(2), new TestClass(3)];
    assert(samples.length === 3, "sample array should contain 3 objects");
    assertEqual(TestClass.GetCreatedCount(), before + 3, "created count increment");
});

test(TestClass, "loop-based aggregate scenario works", function () {
    let total = 0;
    for (let i = 0; i < 5; ++i) {
        const obj = new TestClass(i);
        total += obj.Scale(3);
    }
    assertEqual(total, 30, "scaled sum");
});

TestClass.Print("[DONE] all external tests passed");
