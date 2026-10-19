//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <csetjmp>
#include <csignal>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

// ponytail: longjmp skips toxic classes; leaks on crash path OK vs whole-process die
static sigjmp_buf g_dump_jmp;
static volatile sig_atomic_t g_dump_guard = 0;

static void dump_crash_handler(int) {
    if (!g_dump_guard) {
        _exit(1);
    }
    g_dump_guard = 0;
    siglongjmp(g_dump_jmp, 1);
}

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

static std::string get_type_name(const Il2CppType *type) {
    if (!type) {
        return "void";
    }
    if (il2cpp_type_get_name) {
        char *raw = il2cpp_type_get_name(type);
        if (raw) {
            std::string name(raw);
            if (il2cpp_free) {
                il2cpp_free(raw);
            }
            if (!name.empty() && name.back() == '&') {
                name.pop_back();
            }
            return name;
        }
    }
    if (il2cpp_class_from_type && il2cpp_class_get_name) {
        auto klass = il2cpp_class_from_type(type);
        if (klass) {
            auto name = il2cpp_class_get_name(klass);
            if (name) {
                return name;
            }
        }
    }
    return "void";
}

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    if (!type) {
        return false;
    }
    if (il2cpp_type_is_byref) {
        return il2cpp_type_is_byref(type);
    }
    return type->byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        //TODO attribute
        // 只用 API；MethodInfo 布局随 Unity 变，勿直接读 methodPointer
        outPut << "\t// RVA: - VA: -\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        //TODO genericContainerIndex
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto method_name = il2cpp_method_get_name(method);
        outPut << get_type_name(return_type) << " " << (method_name ? method_name : "?")
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            if (!param) {
                outPut << "?, ";
                continue;
            }
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto pname = il2cpp_method_get_param_name(method, i);
            outPut << get_type_name(param) << " " << (pname ? pname : "arg" + std::to_string(i));
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        //TODO attribute
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            auto prop_type = get ? il2cpp_method_get_return_type(get) : il2cpp_method_get_param(set, 0);
            outPut << get_type_name(prop_type) << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_name = il2cpp_field_get_name(field);
        outPut << get_type_name(field_type) << " " << (field_name ? field_name : "?");
        // 跳过 static_get_value：未初始化/加固游戏上易 SIGSEGV
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    if (!type) {
        return outPut.str();
    }
    auto *klass = il2cpp_class_from_type(type);
    if (!klass) {
        return outPut.str();
    }
    auto ns = il2cpp_class_get_namespace(klass);
    outPut << "\n// Namespace: " << (ns ? ns : "") << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    //TODO attribute
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    //TODO EventInfo
    outPut << "}\n";
    return outPut.str();
}

bool il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    il2cpp_base = 0;
    xdl_info_t info{};
    if (xdl_info(handle, XDL_DI_DLINFO, &info) == 0 && info.dli_fbase) {
        il2cpp_base = reinterpret_cast<uint64_t>(info.dli_fbase);
    } else if (il2cpp_domain_get_assemblies) {
        xdl_info_t addr_info{};
        void *cache = nullptr;
        if (xdl_addr(reinterpret_cast<void *>(il2cpp_domain_get_assemblies), &addr_info, &cache)) {
            il2cpp_base = reinterpret_cast<uint64_t>(addr_info.dli_fbase);
        }
        xdl_addr_clean(&cache);
    }
    LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    if (!il2cpp_base || !il2cpp_domain_get || !il2cpp_thread_attach || !il2cpp_domain_get_assemblies) {
        LOGE("Failed to initialize il2cpp api.");
        return false;
    }
    if (il2cpp_is_vm_thread) {
        for (int i = 0; i < 60 && !il2cpp_is_vm_thread(nullptr); ++i) {
            LOGI("Waiting for il2cpp_init...");
            sleep(1);
        }
        if (!il2cpp_is_vm_thread(nullptr)) {
            LOGE("il2cpp vm thread not ready");
            return false;
        }
    }
    auto domain = il2cpp_domain_get();
    if (!domain) {
        LOGE("il2cpp_domain_get returned null");
        return false;
    }
    il2cpp_thread_attach(domain);
    // 等元数据/静态构造稍稳再扫，减少开局秒崩
    sleep(3);
    return true;
}

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");
    size_t size = 0;
    auto domain = il2cpp_domain_get();
    if (!domain || !il2cpp_domain_get_assemblies) {
        LOGE("domain is not ready");
        return;
    }
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    if (!assemblies) {
        LOGE("no assemblies");
        return;
    }

    auto outPath = std::string(outDir).append("/files/dump.cs");
    std::ofstream outStream(outPath);
    if (!outStream) {
        LOGE("failed to write %s", outPath.c_str());
        return;
    }

    struct sigaction sa{}, old_sa{};
    sa.sa_handler = dump_crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, &old_sa);
    sigaction(SIGBUS, &sa, nullptr);

    size_t count = 0;
    size_t skipped = 0;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        auto image_name = il2cpp_image_get_name(image);
        outStream << "// Image " << i << ": " << (image_name ? image_name : "?") << "\n";
    }
    outStream.flush();

    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            auto image_name = il2cpp_image_get_name(image);
            std::string imageStr = std::string("\n// Dll : ") + (image_name ? image_name : "?");
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                if (sigsetjmp(g_dump_jmp, 1) == 0) {
                    g_dump_guard = 1;
                    auto klass = il2cpp_image_get_class(image, j);
                    if (!klass) {
                        g_dump_guard = 0;
                        continue;
                    }
                    auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                    if (!type) {
                        g_dump_guard = 0;
                        continue;
                    }
                    outStream << imageStr << dump_type(type);
                    outStream.flush();
                    g_dump_guard = 0;
                    ++count;
                } else {
                    ++skipped;
                    outStream << "\n// [skipped crashed class image=" << i << " idx=" << j << "]\n";
                    outStream.flush();
                    LOGW("skip crashed class image=%d idx=%d", i, j);
                }
            }
        }
    } else {
        LOGI("Version less than 2018.3 — need methodPointer; skip reflection path if layout differs");
        LOGE("il2cpp_image_get_class missing; cannot dump safely");
    }

    sigaction(SIGSEGV, &old_sa, nullptr);
    outStream.close();
    LOGI("dump done: %s (%zu types, %zu skipped)", outPath.c_str(), count, skipped);
}