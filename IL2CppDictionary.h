#pragma once
#include <cstdint>
#include <cstring>
#include <windows.h>

//kgm 
namespace IL2Cpp {
    namespace Dict {

        //-------- internal api function pointers --------

        namespace API {
            using ObjectNew_t = void* (*)(void*);
            using ValueBox_t = void* (*)(void*, void*);
            using ArrayNew_t = void* (*)(void*, uintptr_t);
            using ObjectGetClass_t = void* (*)(void*);
            using ClassFromName_t = void* (*)(void*, const char*, const char*);
            using ClassFromType_t = void* (*)(void*);
            using ClassGetType_t = void* (*)(void*);
            using ClassGetMethod_t = void* (*)(void*, const char*, int);
            using DomainGet_t = void* (*)();
            using DomainGetAssemblies_t = void** (*)(void*, size_t*);
            using AssemblyGetImage_t = void* (*)(void*);
            using RuntimeInvoke_t = void* (*)(void*, void*, void**, void**);
            using ArrayNewSpec_t = void* (*)(void*, uintptr_t);
            using TypeGetObject_t = void* (*)(void*);

            inline ObjectNew_t          fnObjectNew = nullptr;
            inline ValueBox_t           fnValueBox = nullptr;
            inline ArrayNew_t           fnArrayNew = nullptr;
            inline ObjectGetClass_t     fnGetClass = nullptr;
            inline ClassFromName_t      fnClassFromName = nullptr;
            inline ClassFromType_t      fnClassFromType = nullptr;
            inline ClassGetType_t       fnClassGetType = nullptr;
            inline ClassGetMethod_t     fnClassGetMethod = nullptr;
            inline DomainGet_t          fnDomainGet = nullptr;
            inline DomainGetAssemblies_t fnGetAssemblies = nullptr;
            inline AssemblyGetImage_t   fnGetImage = nullptr;
            inline RuntimeInvoke_t      fnRuntimeInvoke = nullptr;
            inline TypeGetObject_t      fnTypeGetObject = nullptr;

            inline void* s_intClass = nullptr;
            inline void* s_objClass = nullptr;
            inline void* s_typeClass = nullptr;
            inline bool  s_init = false;

            inline void Init() {
                if (s_init) return;
                s_init = true;
                HMODULE h = ::GetModuleHandleA("GameAssembly.dll");
#define GETPROC(name, type) fn##name = (type##_t)::GetProcAddress(h, "il2cpp_" #name)
                // manual since macro won't match all names cleanly:
                fnObjectNew = (ObjectNew_t)      ::GetProcAddress(h, "il2cpp_object_new");
                fnValueBox = (ValueBox_t)       ::GetProcAddress(h, "il2cpp_value_box");
                fnArrayNew = (ArrayNew_t)       ::GetProcAddress(h, "il2cpp_array_new");
                fnGetClass = (ObjectGetClass_t) ::GetProcAddress(h, "il2cpp_object_get_class");
                fnClassFromName = (ClassFromName_t)  ::GetProcAddress(h, "il2cpp_class_from_name");
                fnClassFromType = (ClassFromType_t)  ::GetProcAddress(h, "il2cpp_class_from_type");
                fnClassGetType = (ClassGetType_t)   ::GetProcAddress(h, "il2cpp_class_get_type");
                fnClassGetMethod = (ClassGetMethod_t) ::GetProcAddress(h, "il2cpp_class_get_method_from_name");
                fnDomainGet = (DomainGet_t)      ::GetProcAddress(h, "il2cpp_domain_get");
                fnGetAssemblies = (DomainGetAssemblies_t)::GetProcAddress(h, "il2cpp_domain_get_assemblies");
                fnGetImage = (AssemblyGetImage_t)::GetProcAddress(h, "il2cpp_assembly_get_image");
                fnRuntimeInvoke = (RuntimeInvoke_t)  ::GetProcAddress(h, "il2cpp_runtime_invoke");
                fnTypeGetObject = (TypeGetObject_t)  ::GetProcAddress(h, "il2cpp_type_get_object");
#undef GETPROC

                void* domain = fnDomainGet();
                size_t count = 0;
                void** asms = fnGetAssemblies(domain, &count);
                for (size_t i = 0; i < count; i++) {
                    void* img = fnGetImage(asms[i]);
                    if (!img) continue;
                    if (!s_intClass)  s_intClass = fnClassFromName(img, "System", "Int32");
                    if (!s_objClass)  s_objClass = fnClassFromName(img, "System", "Object");
                    if (!s_typeClass) s_typeClass = fnClassFromName(img, "System", "Type");
                    if (s_intClass && s_objClass && s_typeClass) break;
                }
            }

            inline void* FindClass(const char* ns, const char* name) {
                Init();
                void* domain = fnDomainGet();
                size_t count = 0;
                void** asms = fnGetAssemblies(domain, &count);
                for (size_t i = 0; i < count; i++) {
                    void* img = fnGetImage(asms[i]);
                    if (!img) continue;
                    void* cls = fnClassFromName(img, ns, name);
                    if (cls) return cls;
                }
                return nullptr;
            }
        }

        //-------- resolve Dictionary<object,object> class universally --------

        inline void* g_DictObjObjClass = nullptr;

        inline void* GetDictClass() {
            if (::IL2Cpp::Dict::g_DictObjObjClass) return ::IL2Cpp::Dict::g_DictObjObjClass;

            API::Init();

            void* domain = API::fnDomainGet();
            size_t asmCount = 0;
            void** asms = API::fnGetAssemblies(domain, &asmCount);


            void* dictDef = nullptr;
            for (size_t i = 0; i < asmCount; i++) {
                void* img = API::fnGetImage(asms[i]);
                if (!img) continue;
                if (!dictDef) dictDef = API::fnClassFromName(img, "System.Collections.Generic", "Dictionary`2");
                if (dictDef) break;
            }
            if (!dictDef || !API::s_objClass || !API::s_typeClass) return nullptr;

            void* objType = API::fnClassGetType(API::s_objClass);
            void* objTypeObj = API::fnTypeGetObject(objType);
            void* dictType = API::fnClassGetType(dictDef);
            void* dictTypeObj = API::fnTypeGetObject(dictType);

            void* typeArr = API::fnArrayNew(API::s_typeClass, 2);
            if (!typeArr) return nullptr;
            ((void**)((uintptr_t)typeArr + 0x20))[0] = objTypeObj;
            ((void**)((uintptr_t)typeArr + 0x20))[1] = objTypeObj;

            // walk up from RuntimeType to find MakeGenericType
            void* makeGeneric = nullptr;
            void* searchClass = API::fnGetClass(dictTypeObj);
            while (searchClass && !makeGeneric) {
                makeGeneric = API::fnClassGetMethod(searchClass, "MakeGenericType", 1);
                if (!makeGeneric) searchClass = *(void**)((uintptr_t)searchClass + 0x58);
            }
            if (!makeGeneric) return nullptr;

            void* exc = nullptr;
            void* args[1] = { typeArr };
            void* concreteTypeObj = API::fnRuntimeInvoke(makeGeneric, dictTypeObj, args, &exc);
            if (exc || !concreteTypeObj) return nullptr;

            // walk up activator to find CreateInstance
            void* activatorClass = API::FindClass("System", "Activator");
            void* createInst = nullptr;
            searchClass = activatorClass;
            while (searchClass && !createInst) {
                createInst = API::fnClassGetMethod(searchClass, "CreateInstance", 1);
                if (!createInst) searchClass = *(void**)((uintptr_t)searchClass + 0x58);
            }
            if (!createInst) return nullptr;

            exc = nullptr;
            void* args2[1] = { concreteTypeObj };
            void* dictInst = API::fnRuntimeInvoke(createInst, nullptr, args2, &exc);
            if (exc || !dictInst) return nullptr;

            ::IL2Cpp::Dict::g_DictObjObjClass = API::fnGetClass(dictInst);
            return ::IL2Cpp::Dict::g_DictObjObjClass;
        }

        //-------- structs --------

        template<typename T>
        struct Array {
            void* klass;
            void* monitor;
            void* bounds;
            uint32_t max_length;
            uint32_t padding;
            T        m_Items[1];

            T& operator[](uint32_t i) { return m_Items[i]; }
            const T& operator[](uint32_t i) const { return m_Items[i]; }
            uint32_t size()                  const { return max_length; }
        };

        template<typename TKey, typename TValue>
        struct Entry {
            int32_t hashCode;
            int32_t next;
            TKey    key;
            TValue  value;
        };

        template<typename TKey, typename TValue> struct Dictionary;

        template<typename TKey, typename TValue>
        struct KeyCollection {
            void* klass;
            void* monitor;
            Dictionary<TKey, TValue>* dictionary;

            int32_t get_Count()       const { return dictionary ? dictionary->count : 0; }
            TKey    operator[](int32_t i) const { return dictionary->GetKeyAt(i); }
        };

        template<typename TKey, typename TValue>
        struct ValueCollection {
            void* klass;
            void* monitor;
            Dictionary<TKey, TValue>* dictionary;

            int32_t get_Count()       const { return dictionary ? dictionary->count : 0; }
            TValue  operator[](int32_t i) const { return dictionary->GetValueAt(i); }
        };

        template<typename TKey, typename TValue>
        struct Dictionary {
            void* klass;
            void* monitor;
            Array<int32_t>* _buckets;
            Array<Entry<TKey, TValue>>* _entries;
            int32_t                             count;
            int32_t                             _freeList;
            int32_t                             _freeCount;
            int32_t                             _version;
            void* _comparer;
            KeyCollection<TKey, TValue>* _keys;
            ValueCollection<TKey, TValue>* _values;
            void* _syncRoot;

            int32_t get_Count() const { return count - _freeCount; }

            Entry<TKey, TValue>* GetEntry(int32_t index) const {
                if (!_entries || index < 0 || (uint32_t)index >= _entries->max_length) return nullptr;
                return &_entries->m_Items[index];
            }

            TKey GetKeyAt(int32_t nthLive) const {
                if (!_entries) return TKey{};
                int32_t live = 0;
                for (uint32_t i = 0; i < _entries->max_length; i++) {
                    auto& e = _entries->m_Items[i];
                    if (e.hashCode < 0) continue;
                    if (live == nthLive) return e.key;
                    live++;
                }
                return TKey{};
            }

            TValue GetValueAt(int32_t nthLive) const {
                if (!_entries) return TValue{};
                int32_t live = 0;
                for (uint32_t i = 0; i < _entries->max_length; i++) {
                    auto& e = _entries->m_Items[i];
                    if (e.hashCode < 0) continue;
                    if (live == nthLive) return e.value;
                    live++;
                }
                return TValue{};
            }

            bool TryGetValue(TKey key, TValue& outValue) const {
                if (!_entries || !_buckets) return false;
                const int32_t bucketCount = static_cast<int32_t>(_buckets->max_length);
                if (bucketCount <= 0) return false;
                const int32_t hash = static_cast<int32_t>((uintptr_t)(void*)key & 0x7FFFFFFF);
                int32_t       i = _buckets->m_Items[hash % bucketCount] - 1;
                while (i >= 0) {
                    auto& e = _entries->m_Items[i];
                    if (e.hashCode == hash && e.key == key) { outValue = e.value; return true; }
                    i = e.next;
                }
                return false;
            }

            TValue operator[](TKey key) const { TValue out{}; TryGetValue(key, out); return out; }

            KeyCollection<TKey, TValue>* get_Keys()   const { return _keys; }
            ValueCollection<TKey, TValue>* get_Values() const { return _values; }

            struct Iterator {
                Dictionary* dict;
                uint32_t    index;

                void advance() {
                    if (!dict || !dict->_entries) { index = UINT32_MAX; return; }
                    while (index < dict->_entries->max_length && dict->_entries->m_Items[index].hashCode < 0)
                        ++index;
                }
                struct KVPair { TKey key; TValue value; };
                KVPair    operator*()  const { auto& e = dict->_entries->m_Items[index]; return { e.key, e.value }; }
                Iterator& operator++() { ++index; advance(); return *this; }
                bool operator!=(const Iterator& o) const { return index != o.index; }
            };

            Iterator begin() { Iterator it{ this, 0 }; it.advance(); return it; }
            Iterator end() { return { this, _entries ? _entries->max_length : 0u }; }

            static Dictionary<TKey, TValue>* CreateWithEntry(TKey key, TValue value) {
                API::Init();

                void* dictClass = ::IL2Cpp::Dict::GetDictClass();
                if (!dictClass || !API::fnObjectNew || !API::fnArrayNew
                    || !API::s_intClass || !API::s_objClass)
                    return nullptr;

                void* raw = API::fnObjectNew(dictClass);
                if (!raw) return nullptr;

                // call default ctor via MakeGenericType-resolved class method
                void* ctorMethod = API::fnClassGetMethod(dictClass, ".ctor", 0);
                if (!ctorMethod) return nullptr;
                void* exc = nullptr;
                API::fnRuntimeInvoke(ctorMethod, raw, nullptr, &exc);
                if (exc) return nullptr;

                auto* dict = static_cast<Dictionary<TKey, TValue>*>(raw);

                void* bucketsArr = API::fnArrayNew(API::s_intClass, 1);
                if (!bucketsArr) return nullptr;
                reinterpret_cast<int32_t*>((uintptr_t)bucketsArr + 0x20)[0] = 1;

                constexpr size_t entrySize = sizeof(Entry<TKey, TValue>);
                const     size_t slots = (entrySize + 7) / 8;
                void* entriesArr = API::fnArrayNew(API::s_objClass, static_cast<uintptr_t>(slots));
                if (!entriesArr) return nullptr;

                auto* ep = reinterpret_cast<Entry<TKey, TValue>*>((uintptr_t)entriesArr + 0x20);
                ep->hashCode = static_cast<int32_t>((uintptr_t)(void*)key & 0x7FFFFFFF);
                ep->next = -1;
                ep->key = key;
                ep->value = value;

                dict->_buckets = static_cast<Array<int32_t>*>(bucketsArr);
                dict->_entries = static_cast<Array<Entry<TKey, TValue>>*>(entriesArr);
                dict->count = 1;
                dict->_freeList = -1;
                dict->_freeCount = 0;
                dict->_version = 1;

                return dict;
            }
        };

    }

    //

    inline void* Box(const char* ns, const char* typeName, void* valuePtr) {
        void* cls = Dict::API::FindClass(ns, typeName);
        if (!cls || !Dict::API::fnValueBox) return nullptr;
        return Dict::API::fnValueBox(cls, valuePtr);
    }

    inline bool SafeReadIL2CPPString(void* strObj, char* outBuf, int outBufSize) {
        if (!strObj) return false;
        __try {
            int32_t len = *(int32_t*)((uintptr_t)strObj + 0x10);
            if (len <= 0 || len > 512) return false;
            wchar_t wbuf[512] = {};
            wchar_t* chars = (wchar_t*)((uintptr_t)strObj + 0x14);
            int copy = len < 511 ? len : 511;
            for (int i = 0; i < copy; i++) wbuf[i] = chars[i];
            wbuf[copy] = 0;
            int sz = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, outBuf, outBufSize, nullptr, nullptr);
            return sz > 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline bool SafeReadIL2CPPInt32(void* boxedObj, int32_t& out) {
        if (!boxedObj) return false;
        __try {
            //boxed int32: value sits right after the object header (0x10)
            out = *(int32_t*)((uintptr_t)boxedObj + 0x10);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline bool SafeReadIL2CPPFloat(void* boxedObj, float& out) {
        if (!boxedObj) return false;
        __try {
            out = *(float*)((uintptr_t)boxedObj + 0x10);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline bool SafeReadRawBytes(void* obj, uint8_t* outBuf, int count) {
        if (!obj) return false;
        __try {
            memcpy(outBuf, (void*)((uintptr_t)obj + 0x10), count);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    inline void LogDictionary(void* dictPtr, const char* label = "Dictionary") {
        if (!dictPtr) { printf("[%s] null pointer\n", label); return; }

        //.. cache float once
        static void* s_floatClass = nullptr;
        static void* s_stringClass = nullptr;
        if (!s_floatClass) {
            Dict::API::Init();
            s_floatClass = Dict::API::FindClass("System", "Single");
            s_stringClass = Dict::API::FindClass("System", "String");
        }

        auto dict = (IL2Cpp::Dict::Dictionary<void*, void*>*)dictPtr;
        if (!dict->_entries) { printf("[%s] no entries\n", label); return; }

        printf("[%s] count=%d\n", label, dict->get_Count());

        auto formatKey = [](void* key, char* buf, int sz) {
            //small pointer = unboxed integer/enum stored directly as ptr value-
            if ((uintptr_t)key < 0x10000) {
                snprintf(buf, sz, "%lld", (long long)(intptr_t)key);
                return;
            }
            //otherwise try as boxed string
            char tmp[512];
            if (SafeReadIL2CPPString(key, tmp, sizeof(tmp)) && tmp[0] != '\0') {
                snprintf(buf, sz, "\"%s\"", tmp);
                return;
            }
            //boxed int
            int32_t ival;
            if (SafeReadIL2CPPInt32(key, ival)) {
                snprintf(buf, sz, "int(%d)", ival);
                return;
            }
            snprintf(buf, sz, "ptr(%p)", key);
            };

        auto formatValue = [&](void* val, char* buf, int sz) {
            if (!val) { snprintf(buf, sz, "null"); return; }

            //read klass pointer to detect boxed type
            void* klass = nullptr;
            __try { klass = *(void**)val; }
            __except (EXCEPTION_EXECUTE_HANDLER) {}

            if (klass && klass == Dict::API::s_intClass) {
                int32_t ival = 0;
                SafeReadIL2CPPInt32(val, ival);
                snprintf(buf, sz, "int(%d)", ival);
                return;
            }
            if (klass && klass == s_floatClass) {
                float fval = 0.f;
                SafeReadIL2CPPFloat(val, fval);
                snprintf(buf, sz, "float(%.4f)", fval);
                return;
            }
            if (klass && klass == s_stringClass) {
                char tmp[512];
                if (SafeReadIL2CPPString(val, tmp, sizeof(tmp)) && tmp[0] != '\0') {
                    snprintf(buf, sz, "\"%s\"", tmp);
                    return;
                }
            }
            //unknown = show klass ptr + raw bytes
            uint8_t raw[16] = {};
            SafeReadRawBytes(val, raw, 16);
            snprintf(buf, sz, "klass=%p raw[%02X %02X %02X %02X %02X %02X %02X %02X]",
                klass,
                raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
            };

        char keyBuf[512], valBuf[1024];
        for (uint32_t i = 0; i < dict->_entries->max_length; i++) {
            auto& e = dict->_entries->m_Items[i];
            if (e.hashCode < 0) continue;

            formatKey(e.key, keyBuf, sizeof(keyBuf));
            formatValue(e.value, valBuf, sizeof(valBuf));

            printf("  [%u] key=%-30s val=%s\n", i, keyBuf, valBuf);
        }
    }

}