function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function assertEqual(actual, expected, label) {
    if (actual !== expected) {
        throw new Error(label + ": expected " + expected + ", got " + actual);
    }
}

function test(TestClass, name, fn) {
    TestClass.Print("[TEST] " + name);
    fn();
    TestClass.Print("[PASS] " + name);
}

module.exports = {
    assert: assert,
    assertEqual: assertEqual,
    test: test,
};
