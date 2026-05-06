#pragma once
#include "Libs/IL2CppResolver/IL2CPP_Resolver.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <Windows.h>

namespace Dumper {

    // Function pointers for name retrieval
    typedef const char* (*il2cpp_class_get_name_t)(void*);
    typedef const char* (*il2cpp_method_get_name_t)(void*);

    il2cpp_class_get_name_t class_get_name = nullptr;
    il2cpp_method_get_name_t method_get_name = nullptr;

    inline bool InitializeFunctions() {
        HMODULE gameAssembly = GetModuleHandleA("GameAssembly.dll");
        if (!gameAssembly) return false;

        class_get_name = (il2cpp_class_get_name_t)GetProcAddress(gameAssembly, "il2cpp_class_get_name");
        method_get_name = (il2cpp_method_get_name_t)GetProcAddress(gameAssembly, "il2cpp_method_get_name");

        return (class_get_name != nullptr && method_get_name != nullptr);
    }

    inline std::string GetClassName(Unity::il2cppClass* klass) {
        if (!klass || !class_get_name) return "Unknown";
        const char* name = class_get_name(klass);
        if (!name || strlen(name) == 0) return "Unknown";
        return std::string(name);
    }

    inline std::string GetMethodName(Unity::il2cppMethodInfo* method) {
        if (!method || !method_get_name) return "Unknown";
        const char* name = method_get_name(method);
        if (!name || strlen(name) == 0) return "Unknown";
        return std::string(name);
    }

    inline std::string GetDllDirectory() {
        char path[MAX_PATH];
        HMODULE hModule = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetDllDirectory, &hModule);
        GetModuleFileNameA(hModule, path, MAX_PATH);
        std::string fullPath(path);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(0, lastSlash + 1);
        }
        return "";
    }

    inline std::string SanitizeIdentifier(const std::string& str) {
        std::string result;
        for (char c : str) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '_') {
                result += c;
            }
            else {
                result += '_';
            }
        }
        if (!result.empty() && result[0] >= '0' && result[0] <= '9') {
            result = "_" + result;
        }
        return result;
    }

    inline void Run() {
        void* m_pThisThread = IL2CPP::Thread::Attach(IL2CPP::Domain::Get());

        printf("[INFO] Starting IL2CPP RVA Dumper...\n");

        if (!InitializeFunctions()) {
            printf("[ERROR] Failed to initialize IL2CPP name functions\n");
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        std::string dllDir = GetDllDirectory();
        std::string txtPath = dllDir + "offsets.h";
        std::string csvPath = dllDir + "method_rva.csv";

        printf("[INFO] Output directory: %s\n", dllDir.c_str());

        std::ofstream txtFile(txtPath);
        std::ofstream csvFile(csvPath);

        if (!txtFile.is_open() || !csvFile.is_open()) {
            printf("[ERROR] Failed to open output files\n");
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        txtFile << "#pragma once\n";
        txtFile << "// IL2CPP Method Offsets (Assembly-CSharp)\n";
        txtFile << "// Auto-generated - duplicates have _RVA suffix\n\n";
        txtFile << "namespace Offsets {\n\n";

        csvFile << "Class,Method,RVA_Hex,RVA_Dec\n";

        uintptr_t baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA("GameAssembly.dll"));
        printf("[INFO] GameAssembly.dll base: 0x%llX\n", baseAddr);

        size_t assemblyCount = 0;
        Unity::il2cppAssembly** assemblies = IL2CPP::Domain::GetAssemblies(&assemblyCount);

        if (!assemblies || assemblyCount == 0) {
            printf("[ERROR] No assemblies found!\n");
            txtFile.close();
            csvFile.close();
            IL2CPP::Thread::Detach(m_pThisThread);
            return;
        }

        printf("[INFO] Found %zu assemblies\n", assemblyCount);

        int totalMethods = 0;
        bool foundAssemblyCSharp = false;

        // Map: ClassName -> (MethodName -> RVA)
        std::map<std::string, std::map<std::string, uintptr_t>> classMethods;

        for (size_t i = 0; i < assemblyCount; i++) {
            Unity::il2cppAssembly* assembly = assemblies[i];
            if (!assembly || !assembly->m_pImage) continue;

            Unity::il2cppImage* image = assembly->m_pImage;
            const char* assemblyName = image->m_pNameNoExt ? image->m_pNameNoExt : "Unknown";

            if (strcmp(assemblyName, "Assembly-CSharp") != 0) {
                continue;
            }

            foundAssemblyCSharp = true;
            printf("[INFO] Processing: %s\n", assemblyName);

            size_t classCount = reinterpret_cast<size_t(*)(Unity::il2cppImage*)>(
                IL2CPP::Functions.m_ImageGetClassCount)(image);

            for (size_t j = 0; j < classCount; j++) {
                Unity::il2cppClass* klass = reinterpret_cast<Unity::il2cppClass * (*)(Unity::il2cppImage*, size_t)>(
                    IL2CPP::Functions.m_ImageGetClass)(image, j);

                if (!klass) continue;

                std::string className = GetClassName(klass);
                std::string sanitizedClassName = SanitizeIdentifier(className);

                std::vector<Unity::il2cppMethodInfo*> methods;
                IL2CPP::Class::FetchMethods(klass, &methods);

                // Track method name occurrences within this class to handle duplicates
                std::map<std::string, int> methodNameCount;

                for (auto method : methods) {
                    if (!method || !method->m_pMethodPointer) continue;

                    std::string methodName = GetMethodName(method);
                    std::string sanitizedMethodName = SanitizeIdentifier(methodName);

                    uintptr_t methodPtr = reinterpret_cast<uintptr_t>(method->m_pMethodPointer);
                    uintptr_t rva = methodPtr - baseAddr;

                    // Make unique name for duplicates
                    std::string uniqueMethodName = sanitizedMethodName;
                    if (methodNameCount[sanitizedMethodName] > 0) {
                        // Append RVA in hex to make it unique
                        char suffix[32];
                        sprintf_s(suffix, "_0x%llX", rva);
                        uniqueMethodName += suffix;
                    }
                    methodNameCount[sanitizedMethodName]++;

                    // Store in map
                    classMethods[sanitizedClassName][uniqueMethodName] = rva;

                    // CSV output
                    csvFile << className << "," << methodName << ",0x"
                        << std::hex << std::uppercase << rva << std::dec << "," << rva << "\n";

                    totalMethods++;
                }
            }
        }

        // Write C++ namespaces
        for (const auto& [className, methods] : classMethods) {
            txtFile << "    namespace " << className << " {\n";

            for (const auto& [methodName, rva] : methods) {
                txtFile << "        inline constexpr uintptr_t " << methodName << " = " << rva << ";\n";
            }

            txtFile << "    }\n\n";
        }

        txtFile << "} // namespace Offsets\n";

        txtFile.close();
        csvFile.close();

        if (!foundAssemblyCSharp) {
            printf("[WARNING] Assembly-CSharp not found!\n");
        }

        printf("[SUCCESS] Dumped %d methods from Assembly-CSharp\n", totalMethods);
        printf("[SUCCESS] Output: %s\n", txtPath.c_str());
        printf("[SUCCESS] Output: %s\n", csvPath.c_str());

        IL2CPP::Thread::Detach(m_pThisThread);
    }

} // namespace Dumper