var global = global || (function () { return this; }());
(function (global) {
    "use strict";

    var puerts = global.puerts = global.puerts || {};
    var evalScript = global.__tgjsEvalScript || function (script) { return eval(script); };
    var loadModule = global.__tgjsLoadModule;
    var searchModule = global.__tgjsSearchModule;
    var findModule = global.__tgjsFindModule;
    var orgRequire = global.require;

    global.__tgjsEvalScript = undefined;
    global.__tgjsLoadModule = undefined;
    global.__tgjsSearchModule = undefined;
    global.__tgjsFindModule = undefined;

    function normalize(name) {
        return name;
    }

    var tmpModuleStorage = [];
    function addModule(m) {
        for (var i = 0; i < tmpModuleStorage.length; ++i) {
            if (!tmpModuleStorage[i]) {
                tmpModuleStorage[i] = m;
                return i;
            }
        }
        return tmpModuleStorage.push(m) - 1;
    }

    function getModuleBySID(id) {
        return tmpModuleStorage[id];
    }

    var moduleCache = Object.create(null);
    var buildinModule = Object.create(null);

    function executeModule(fullPath, script, debugPath, sid) {
        var module = puerts.getModuleBySID(sid);
        var exports = {};
        module.exports = exports;
        var fullPathInJs = fullPath.replace(/\\/g, "\\\\");
        var fullDirInJs = fullPath.lastIndexOf("/") !== -1
            ? fullPath.substring(0, fullPath.lastIndexOf("/"))
            : fullPath.substring(0, fullPath.lastIndexOf("\\")).replace(/\\/g, "\\\\");
        var wrapped = evalScript(
            "(function (exports, require, module, __filename, __dirname) { " + script + "\n});",
            debugPath || fullPath
        );
        wrapped(exports, puerts.genRequire(fullDirInJs), module, fullPathInJs, fullDirInJs);
        return module.exports;
    }

    function genRequire(requiringDir) {
        var localModuleCache = Object.create(null);
        function require(moduleName) {
            if (orgRequire) {
                try {
                    return orgRequire(moduleName);
                } catch (e) {
                }
            }

            moduleName = normalize(moduleName);
            var forceReload = false;
            if (moduleName in localModuleCache) {
                var cachedLocal = localModuleCache[moduleName];
                if (cachedLocal && !cachedLocal.__forceReload) {
                    return cachedLocal.exports;
                }
                forceReload = !!(cachedLocal && cachedLocal.__forceReload);
            }

            if (moduleName in buildinModule) {
                return buildinModule[moduleName];
            }

            var nativeModule = findModule && findModule(moduleName);
            if (nativeModule) {
                buildinModule[moduleName] = nativeModule;
                return nativeModule;
            }

            if (!searchModule || !loadModule) {
                throw new Error("module loader is not initialized");
            }

            var moduleInfo = searchModule(moduleName, requiringDir || "");
            if (!moduleInfo) {
                throw new Error("can not find " + moduleName + " in " + requiringDir);
            }

            var fullPath = moduleInfo[0];
            var debugPath = moduleInfo[1];
            if (!forceReload && fullPath in moduleCache) {
                localModuleCache[moduleName] = moduleCache[fullPath];
                return moduleCache[fullPath].exports;
            }

            var m = { exports: {} };
            localModuleCache[moduleName] = m;
            moduleCache[fullPath] = m;

            var sid = addModule(m);
            try {
                if (fullPath.endsWith(".json")) {
                    m.exports = JSON.parse(loadModule(fullPath));
                } else {
                    executeModule(fullPath, loadModule(fullPath), debugPath, sid);
                }
            } catch (e) {
                localModuleCache[moduleName] = undefined;
                moduleCache[fullPath] = undefined;
                throw e;
            } finally {
                tmpModuleStorage[sid] = undefined;
            }

            return m.exports;
        }

        return require;
    }

    function registerBuildinModule(name, module) {
        buildinModule[name] = module;
    }

    function forceReload(reloadModuleKey) {
        if (reloadModuleKey) {
            reloadModuleKey = normalize(reloadModuleKey);
        }
        for (var moduleKey in moduleCache) {
            if (!reloadModuleKey || reloadModuleKey === moduleKey) {
                moduleCache[moduleKey].__forceReload = true;
                if (reloadModuleKey) {
                    break;
                }
            }
        }
    }

    function getModuleByUrl(url) {
        if (url) {
            return moduleCache[normalize(url)];
        }
    }

    registerBuildinModule("puerts", puerts);

    puerts.genRequire = genRequire;
    puerts.__require = genRequire("");
    puerts.getModuleBySID = getModuleBySID;
    puerts.registerBuildinModule = registerBuildinModule;
    puerts.loadModule = loadModule;
    puerts.forceReload = forceReload;
    puerts.getModuleByUrl = getModuleByUrl;

    global.require = puerts.__require;
}(global));
