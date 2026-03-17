function rawSet(obj, key, val) {
        Object.defineProperty(obj, key, {
            value: val,
            writable: true,
            configurable: true,
            enumerable: false,
        });
    }

let CPP = Object.create(null);
    let CPP_proxy = new Proxy(CPP, {
        get: function(CPP, name) {
            let cls = loadCPPType(name);
            rawSet(CPP, name, cls);
            return cls;
        }
    });
    Object.setPrototypeOf(CPP, CPP_proxy);
    
    puerts.registerBuildinModule('cpp', CPP);
    global.CPP = CPP;