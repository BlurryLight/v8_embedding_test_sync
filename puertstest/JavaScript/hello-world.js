// @ts-ignore
const { TestClass } = require("cpp");
function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}
function assertEqual(actual, expected, label) {
    if (actual !== expected) {
        throw new Error(`${label}: expected ${expected}, got ${actual}`);
    }
}
function test(name, fn) {
    TestClass.Print(`[TEST] ${name}`);
    fn();
    TestClass.Print(`[PASS] ${name}`);
}
test("static methods are callable", () => {
    TestClass.Print("hello world from external script");
    assertEqual(TestClass.AddAll(1, 2, 3), 6, "AddAll result");
    assertEqual(TestClass.GetCreatedCount(), 0, "initial created count");
});
test("instance property read and write works", () => {
    const obj = new TestClass(123);
    assertEqual(obj.X, 123, "initial X");
    obj.X = 99;
    assertEqual(obj.X, 99, "updated X");
});
test("instance methods mutate and return values", () => {
    const obj = new TestClass(10);
    assertEqual(obj.Add(1, 3), 4, "Add result");
    assertEqual(obj.SetX(7), 7, "SetX result");
    assertEqual(obj.Scale(6), 42, "Scale result");
    assertEqual(obj.Describe(), "TestClass(42)", "Describe result");
});
test("multiple objects do not share state", () => {
    const left = new TestClass(5);
    const right = new TestClass(8);
    left.Scale(2);
    right.SetX(-3);
    assertEqual(left.X, 10, "left.X");
    assertEqual(right.X, -3, "right.X");
});
test("created count tracks constructor calls", () => {
    const before = TestClass.GetCreatedCount();
    const samples = [new TestClass(1), new TestClass(2), new TestClass(3)];
    assert(samples.length === 3, "sample array should contain 3 objects");
    assertEqual(TestClass.GetCreatedCount(), before + 3, "created count increment");
});
test("loop-based aggregate scenario works", () => {
    let total = 0;
    for (let i = 0; i < 5; ++i) {
        const obj = new TestClass(i);
        total += obj.Scale(3);
    }
    assertEqual(total, 30, "scaled sum");
});
TestClass.Print("[DONE] all external tests passed");
//# sourceMappingURL=hello-world.js.map