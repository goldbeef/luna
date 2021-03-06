﻿/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/06/18, trumanzhao@foxmail.com
*/

#pragma once

#include <assert.h>
#include <string.h>
#include <cstdint>
#include <string>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include "lua.hpp"

void stackDump(lua_State* L, int line, const char* filename);

template <typename T> void lua_push_object(lua_State* L, T obj);
template <typename T> T lua_to_object(lua_State* L, int idx);

template <typename T>
T lua_to_native(lua_State* L, int i) {
    if constexpr (std::is_same_v<T, bool>) {
        return lua_toboolean(L, i) != 0;
    } else if constexpr (std::is_same_v<T, std::string>) {
        const char* str = lua_tostring(L, i);
        return str == nullptr ? "" : str;
    } else if constexpr (std::is_integral_v<T>) {
        return (T)lua_tointeger(L, i);
    } else if constexpr (std::is_floating_point_v<T>) {
        return (T)lua_tonumber(L, i);
    } else if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_volatile_t<std::remove_pointer_t<T>>;
        if constexpr (std::is_same_v<type, const char>) {
            return lua_tostring(L, i);
        } else {
            return lua_to_object<T>(L, i); 
        }
    } else {
        // unsupported type
    }
}

template <typename T>
void native_to_lua(lua_State* L, T v) {
    if constexpr (std::is_same_v<T, bool>) {
        lua_pushboolean(L, v);
    } else if constexpr (std::is_same_v<T, std::string>) {
        lua_pushstring(L, v.c_str());
    } else if constexpr (std::is_integral_v<T>) {
        lua_pushinteger(L, (lua_Integer)v);
    } else if constexpr (std::is_floating_point_v<T>) {
        lua_pushnumber(L, v);
    } else if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_cv_t<std::remove_pointer_t<T>>;
        if constexpr (std::is_same_v<type, char>) {
            if (v != nullptr) {
                lua_pushstring(L, v);
            } else {
                lua_pushnil(L);
            }
        } else {
            stackDump(L, __LINE__, __FUNCTION__);
            lua_push_object(L, v); 
        }
    } else {
        // unsupported type
        lua_pushnil(L);
    }
}

inline int lua_normal_index(lua_State* L, int idx) {
    int top = lua_gettop(L);
    if (idx < 0 && -idx <= top)
        return idx + top + 1; //转换为正向索引(栈中的)
    return idx; //可能为负
}

bool _lua_set_fence(lua_State* L, const void* p);
void _lua_del_fence(lua_State* L, const void* p);

using lua_global_function = std::function<int(lua_State*)>;
using lua_object_function = std::function<int(void*, lua_State*)>;

template<size_t... integers, typename return_type, typename... arg_types>
return_type call_helper(lua_State* L, return_type(*func)(arg_types...), std::index_sequence<integers...>&&) {
    return (*func)(lua_to_native<arg_types>(L, integers + 1)...);
}

template<size_t... integers, typename return_type, typename class_type, typename... arg_types>
return_type call_helper(lua_State* L, class_type* obj, return_type(class_type::*func)(arg_types...), std::index_sequence<integers...>&&) {
    return (obj->*func)(lua_to_native<arg_types>(L, integers + 1)...);
}

template<size_t... integers, typename return_type, typename class_type, typename... arg_types>
return_type call_helper(lua_State* L, class_type* obj, return_type(class_type::*func)(arg_types...) const, std::index_sequence<integers...>&&) {
    return (obj->*func)(lua_to_native<arg_types>(L, integers + 1)...);
}

template <typename return_type, typename... arg_types>
lua_global_function lua_adapter(return_type(*func)(arg_types...)) {
    return [=](lua_State* L) {
        stackDump(L, __LINE__, __FUNCTION__);
        native_to_lua(L, call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    };
}

template <typename... arg_types>
lua_global_function lua_adapter(void(*func)(arg_types...)) {
    return [=](lua_State* L) {//todo
        call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    };
}

template <>
inline lua_global_function lua_adapter(int(*func)(lua_State* L)) {
    return func;
}

template <typename return_type, typename T, typename... arg_types>
lua_object_function lua_adapter(return_type(T::*func)(arg_types...)) {
    return [=](void* obj, lua_State* L) {
        native_to_lua(L, call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    };
}

template <typename return_type, typename T, typename... arg_types>
lua_object_function lua_adapter(return_type(T::*func)(arg_types...) const) {
    return [=](void* obj, lua_State* L) {
        native_to_lua(L, call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    };
}

template <typename T, typename... arg_types>
lua_object_function lua_adapter(void(T::*func)(arg_types...)) {
    return [=](void* obj, lua_State* L) {
        stackDump(L, __LINE__, __FUNCTION__);
        call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>());
        stackDump(L, __LINE__, __FUNCTION__);
        return 0;
    };
}

template <typename T, typename... arg_types>
lua_object_function lua_adapter(void(T::*func)(arg_types...) const) {
    return [=](void* obj, lua_State* L) {
        call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    };
}

template <typename T>
lua_object_function lua_adapter(int(T::*func)(lua_State* L)) {
    return [=](void* obj, lua_State* L) {
        T* this_ptr = (T*)obj;
        return (this_ptr->*func)(L);
    };
}

template <typename T>
lua_object_function lua_adapter(int(T::*func)(lua_State* L) const) {
    return [=](void* obj, lua_State* L) {
        T* this_ptr = (T*)obj;
        return (this_ptr->*func)(L);
    };
}

using luna_member_wrapper = std::function<void(lua_State*, void*, char*)>;

int _lua_object_bridge(lua_State* L);

struct lua_export_helper {
    static luna_member_wrapper getter(const bool&) {
        return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
            lua_pushboolean(L, *(bool*)addr);
            stackDump(L, __LINE__, __FUNCTION__);
        };
    }

    static luna_member_wrapper setter(const bool&) {
        return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
            *(bool*)addr = lua_toboolean(L, -1);
            stackDump(L, __LINE__, __FUNCTION__);
        };
    }

	template <typename T>
	static typename std::enable_if<std::is_integral<T>::value, luna_member_wrapper>::type getter(const T&) {
		return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
		    lua_pushinteger(L, (lua_Integer)*(T*)addr);
            stackDump(L, __LINE__, __FUNCTION__);
		};
    }

	template <typename T>
	static typename std::enable_if<std::is_integral<T>::value, luna_member_wrapper>::type setter(const T&) {
		return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
		    *(T*)addr = (T)lua_tonumber(L, -1);
            stackDump(L, __LINE__, __FUNCTION__);
		};
    }    

	template <typename T>
	static typename std::enable_if<std::is_floating_point<T>::value, luna_member_wrapper>::type getter(const T&) {
		return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
		    lua_pushnumber(L, (lua_Number)*(T*)addr);
            stackDump(L, __LINE__, __FUNCTION__);
		};
    }

	template <typename T>
	static typename std::enable_if<std::is_floating_point<T>::value, luna_member_wrapper>::type setter(const T&) {
		return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
		    *(T*)addr = (T)lua_tonumber(L, -1);
            stackDump(L, __LINE__, __FUNCTION__);
		};
    }    

	static luna_member_wrapper getter(const std::string&) {
	    return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
            const std::string& str = *(std::string*)addr;
            lua_pushlstring(L, str.c_str(), str.size());
            stackDump(L, __LINE__, __FUNCTION__);
        };
	}

	static luna_member_wrapper setter(const std::string&) {
        return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
            size_t len = 0;
            const char* str = lua_tolstring(L, -1, &len);
            if (str != nullptr) {
                *(std::string*)addr = std::string(str, len);                        
            }
            stackDump(L, __LINE__, __FUNCTION__);
		};
	}

	template <size_t Size>
	static luna_member_wrapper getter(const char (&)[Size]) {
		return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
		    lua_pushstring(L, addr);
            stackDump(L, __LINE__, __FUNCTION__);
		};
	}

	template <size_t Size>
	static luna_member_wrapper setter(const char (&)[Size]) {
        return [](lua_State* L, void*, char* addr){
            stackDump(L, __LINE__, __FUNCTION__);
            size_t len = 0;
            const char* str = lua_tolstring(L, -1, &len);
            if (str != nullptr && len < Size) {
                memcpy(addr, str, len);
                addr[len] = '\0';                    
            }
            stackDump(L, __LINE__, __FUNCTION__);
        };
	}
	
	template <typename return_type, typename T, typename... arg_types>
	static luna_member_wrapper getter(return_type(T::*func)(arg_types...)) {
		return [adapter=lua_adapter(func)](lua_State* L, void* obj, char*) mutable {
		        //table, 'func_a'
                stackDump(L, __LINE__, __FUNCTION__);

                //table, 'func_a', obj
				lua_pushlightuserdata(L, obj);
                //table, 'func_a', obj, adapter(global Func)
				lua_pushlightuserdata(L, &adapter);

                //table, 'func_a', _lua_object_bridge
                //_lua_object_bridge(obj, adapter)
				lua_pushcclosure(L, _lua_object_bridge, 2);
                stackDump(L, __LINE__, __FUNCTION__);
			};
	}

	template <typename return_type, typename T, typename... arg_types>
	static luna_member_wrapper setter(return_type(T::*func)(arg_types...)) {
		return [=](lua_State* L, void* obj, char*){
            stackDump(L, __LINE__, __FUNCTION__);
		    lua_rawset(L, -3);
            stackDump(L, __LINE__, __FUNCTION__);
		};
	}

	template <typename return_type, typename T, typename... arg_types>
	static luna_member_wrapper getter(return_type(T::*func)(arg_types...) const) {
		return [adapter=lua_adapter(func)](lua_State* L, void* obj, char*) mutable {
            stackDump(L, __LINE__, __FUNCTION__);
				lua_pushlightuserdata(L, obj);
				lua_pushlightuserdata(L, &adapter);
				lua_pushcclosure(L, _lua_object_bridge, 2);
            stackDump(L, __LINE__, __FUNCTION__);
			};
	}

	template <typename return_type, typename T, typename... arg_types>
	static luna_member_wrapper setter(return_type(T::*func)(arg_types...) const) {
		return [=](lua_State* L, void* obj, char*){
            stackDump(L, __LINE__, __FUNCTION__);
		    lua_rawset(L, -3);
            stackDump(L, __LINE__, __FUNCTION__);
		};
	}        
};

struct lua_member_item {
    const char* name;
    int offset;
    luna_member_wrapper getter;
    luna_member_wrapper setter;
};

template <typename T>
int lua_member_index(lua_State* L) {
    //tObj, key
    stackDump(L, __LINE__, __FUNCTION__);
    T* obj = lua_to_object<T*>(L, 1); //强制转换为对象指针
    if (obj == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    //tObj, key
    const char* key = lua_tostring(L, 2);
    const char* meta_name = obj->lua_get_meta_name();
    if (key == nullptr || meta_name == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    //tObj, key, _G."_class_meta:"#ClassName
    luaL_getmetatable(L, meta_name);
    stackDump(L, __LINE__, __FUNCTION__);

    //tObj, key, _G."_class_meta:"#ClassName, key,
    lua_pushstring(L, key);
    stackDump(L, __LINE__, __FUNCTION__);

    //tObj, key, _G."_class_meta:"#ClassName, _G."_class_meta:"#ClassName.key(item - userdata)
    lua_rawget(L, -2);
    stackDump(L, __LINE__, __FUNCTION__);

    //tObj, key, _G."_class_meta:"#ClassName, _G."_class_meta:"#ClassName.key(item - userdata)
    auto item = (lua_member_item*)lua_touserdata(L, -1);
    if (item == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    //tObj, key,
    lua_settop(L, 2);
    stackDump(L, __LINE__, __FUNCTION__);
    item->getter(L, obj, (char*)obj + item->offset); //lua_export_helper::getter(&class_type::Method/member)
    stackDump(L, __LINE__, __FUNCTION__);
    //压入数据
    //tObj, key, val
    return 1;
}

template <typename T>
int lua_member_new_index(lua_State* L) {
    //tObj, mem_name, value
    stackDump(L, __LINE__, __FUNCTION__);
    T* obj = lua_to_object<T*>(L, 1);
    if (obj == nullptr)
        return 0;

    const char* key = lua_tostring(L, 2);
    const char* meta_name = obj->lua_get_meta_name();
    if (key == nullptr || meta_name == nullptr)
        return 0;

    stackDump(L, __LINE__, __FUNCTION__);

    //tObj, mem_name, value, G._class_meta:my_class
    luaL_getmetatable(L, meta_name);
    stackDump(L, __LINE__, __FUNCTION__);

    //tObj, mem_name, value, G._class_meta:my_class, mem_name
    lua_pushvalue(L, 2);
    stackDump(L, __LINE__, __FUNCTION__);

    //tObj, mem_name, value, G._class_meta:my_class, G._class_meta:my_class.mem_name(item)
    /*
     *  _G."_class_meta:"#ClassName = {__index = indexTab, __newindex = newindexTab, __gc = gcTab
         *              mem_name1 = item1,
         *              mem_name2 = item2,
         *              }
     */
    lua_rawget(L, -2);
    stackDump(L, __LINE__, __FUNCTION__);

    auto item = (lua_member_item*)lua_touserdata(L, -1);
    lua_pop(L, 2);
    stackDump(L, __LINE__, __FUNCTION__);
    if (item == nullptr) {
        lua_rawset(L, -3);
        return 0;
    }

    if (item->setter) {
        stackDump(L, __LINE__, __FUNCTION__);
        item->setter(L, obj, (char*)obj + item->offset);
        stackDump(L, __LINE__, __FUNCTION__);
    }
    stackDump(L, __LINE__, __FUNCTION__);
    return 0;
}

template<typename T>
struct has_member_gc {
    template<typename U> static auto check_gc(int) -> decltype(std::declval<U>().__gc(), std::true_type());
    template<typename U> static std::false_type check_gc(...);
    enum { value = std::is_same<decltype(check_gc<T>(0)), std::true_type>::value };
};

template <typename T>
int lua_object_gc(lua_State* L) {
    T* obj = lua_to_object<T*>(L, 1);
    if (obj == nullptr)
        return 0;

    _lua_del_fence(L, obj);

    if constexpr (has_member_gc<T>::value) {
        obj->__gc();
    } else {
        delete obj;
    }
    return 0;
}

template <typename T>
void lua_register_class(lua_State* L, T* obj) {
    //LUA_REGISTRYINDEX.__objects__,  tObj
    stackDump(L, __LINE__, __FUNCTION__);

    int top = lua_gettop(L); //stack num

    const char* meta_name = obj->lua_get_meta_name(); //"_class_meta:"#ClassName
    lua_member_item* item = obj->lua_get_meta_data();

    // LUA_REGISTRYINDEX.__objects__, tObj, _G."_class_meta:"#ClassName
    //_G."_class_meta:"#ClassName {__name = meta_name}
    luaL_newmetatable(L, meta_name);
    stackDump(L, __LINE__, __FUNCTION__);

    // LUA_REGISTRYINDEX.__objects__, tObj, _G."_class_meta:"#ClassName, __index
    lua_pushstring(L, "__index");
    stackDump(L, __LINE__, __FUNCTION__);

    // LUA_REGISTRYINDEX.__objects__, tObj, _G."_class_meta:"#ClassName, __index， indexFunc(_lua_object_bridge)
    lua_pushcfunction(L, &lua_member_index<T>);
    stackDump(L, __LINE__, __FUNCTION__);

    // LUA_REGISTRYINDEX.__objects__, tObj, _G."_class_meta:"#ClassName,
    //_G."_class_meta:"#ClassName = {_index = indexFunc}
    lua_rawset(L, -3);
    stackDump(L, __LINE__, __FUNCTION__);

    // ..., tObj, _G."_class_meta:"#ClassName, __newindex
    //_G."_class_meta:"#ClassName = {_index = indexTab}
    lua_pushstring(L, "__newindex");
    stackDump(L, __LINE__, __FUNCTION__);

    // ..., tObj, _G."_class_meta:"#ClassName, __newindex, newIndexFunc
    //_G."_class_meta:"#ClassName = {_index = newIndexFunc}
    lua_pushcfunction(L, &lua_member_new_index<T>);
    stackDump(L, __LINE__, __FUNCTION__);

    // ..., tObj, _G."_class_meta:"#ClassName,
    //_G."_class_meta:"#ClassName = {_index = indexFunc, __newindex = newIndexFunc}
    lua_rawset(L, -3);
    stackDump(L, __LINE__, __FUNCTION__);

    // ..., tObj, _G."_class_meta:"#ClassName,  __gc
    //_G."_class_meta:"#ClassName = {_index = indexFunc, __newindex = newIndexFunc}
    lua_pushstring(L, "__gc");
    stackDump(L, __LINE__, __FUNCTION__);

    // ..., tObj, _G."_class_meta:"#ClassName,  __gc, gcFunc
    //_G."_class_meta:"#ClassName = {_index = indexFunc, __newindex = newIndexFunc}
    lua_pushcfunction(L, &lua_object_gc<T>);
    stackDump(L, __LINE__, __FUNCTION__);

    // ..., tObj, _G."_class_meta:"#ClassName,
    //_G."_class_meta:"#ClassName = {_index = indexFunc, __newindex = newIndexFunc, __gc = gcFunc}
    lua_rawset(L, -3);
    stackDump(L, __LINE__, __FUNCTION__);

    // ..., tObj, _G."_class_meta:"#ClassName,
    //_G."_class_meta:"#ClassName = {__index = indexFunc, __newindex = newIndexFunc, __gc = gcFunc}
    //设置成员
    while (item->name) {
        const char* name = item->name;
        // export member name "m_xxx" as "xxx"
#if !defined(LUNA_KEEP_MEMBER_PREFIX)
        if (name[0] == 'm' && name[1] == '_')
            name += 2; //数据字段调整
#endif
        // ..., tObj, _G."_class_meta:"#ClassName, member_name
        //_G."_class_meta:"#ClassName = {__index = indexTab, __newindex = newindexTab, __gc = gcTab}
        lua_pushstring(L, name);
        stackDump(L, __LINE__, __FUNCTION__);

        // ..., tObj, _G."_class_meta:"#ClassName, member_name, item
        //_G."_class_meta:"#ClassName = {__index = indexTab, __newindex = newindexTab, __gc = gcTab}
        lua_pushlightuserdata(L, item);
        stackDump(L, __LINE__, __FUNCTION__);

        // ..., tObj, _G."_class_meta:"#ClassName
        /*
         * _G."_class_meta:"#ClassName = {__index = indexTab, __newindex = newindexTab, __gc = gcTab
         *              mem_name1 = item1,
         *              mem_name2 = item2,
         *              }
         * */
        lua_rawset(L, -3);
        stackDump(L, __LINE__, __FUNCTION__);
        item++;
    }

    stackDump(L, __LINE__, __FUNCTION__);
    // ..., tObj,
    /*
     * _G."_class_meta:"#ClassName = {__index = indexTab, __newindex = newindexTab, __gc = gcTab
     *              mem_name1 = item1,
     *              mem_name2 = item2,
     *              }
     * */
    lua_settop(L, top);
    stackDump(L, __LINE__, __FUNCTION__);
}

template <typename T>
void lua_push_object(lua_State* L, T obj) {
    stackDump(L, __LINE__, __FUNCTION__);
    if (obj == nullptr) {
        lua_pushnil(L);
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, "__objects__");
    stackDump(L, __LINE__, __FUNCTION__);
    if (lua_isnil(L, -1)) {
        // nil,

        //
        lua_pop(L, 1);

        // t1
        lua_newtable(L);
        // t1, t2
        lua_newtable(L);
        // t1, t2, "v"
        lua_pushstring(L, "v");
        /*
         * t1, t2,
         *
         * t2 = { __mode = "v", }
         * */
        lua_setfield(L, -2, "__mode");

        /*
         * t1
         * t1.meta --> t2
         * t2 = { __mode = "v", }
         * */
        lua_setmetatable(L, -2);

        // t1, t1
        lua_pushvalue(L, -1);

        /*
         * t1
         * t1.meta --> t2
         * t2 = { __mode = "v", }
         *
         * LUA_REGISTRYINDEX = {__objects__ = t1}
         */
        lua_setfield(L, LUA_REGISTRYINDEX, "__objects__");
    }

    //t1 ==>
    // LUA_REGISTRYINDEX.__objects__, LUA_REGISTRYINDEX.__objects__.obj
    stackDump(L, __LINE__, __FUNCTION__);
    if (lua_rawgetp(L, -1, obj) != LUA_TTABLE) {
        //说明对象obj还没有完全导出来
        //LUA_REGISTRYINDEX.__objects__, LUA_REGISTRYINDEX.__objects__.obj
        if (!_lua_set_fence(L, obj)) {
            //已经导出来了, 直接返回
            //
            lua_remove(L, -2);
            return;
        }

        //LUA_REGISTRYINDEX.__objects__, LUA_REGISTRYINDEX.__objects__.obj

        //LUA_REGISTRYINDEX.__objects__
        lua_pop(L, 1);

        //LUA_REGISTRYINDEX.__objects__, tObj
        lua_newtable(L);

        //LUA_REGISTRYINDEX.__objects__, tObj, __pointer__
        lua_pushstring(L, "__pointer__");

        //LUA_REGISTRYINDEX.__objects__, tObj, __pointer__, obj
        lua_pushlightuserdata(L, obj);

        /*
         * LUA_REGISTRYINDEX.__objects__, tObj
         * tObj = {__pointer__ = obj}
         * */
        lua_rawset(L, -3);

        // LUA_REGISTRYINDEX.__objects__, tObj
        const char* meta_name = obj->lua_get_meta_name(); //_G."_class_meta:"#ClassName.meta

        // LUA_REGISTRYINDEX.__objects__, tObj, _G."_class_meta:"#ClassName.meta or nil
        luaL_getmetatable(L, meta_name);

        if (lua_isnil(L, -1)) {
            // LUA_REGISTRYINDEX.__objects__, tObj, nil

            // LUA_REGISTRYINDEX.__objects__, tObj
            lua_remove(L, -1);

            // ..., tObj,
            /*
             * _G."_class_meta:"#ClassName = {__index = indexTab, __newindex = newindexTab, __gc = gcTab
             *              mem_name1 = item1,
             *              mem_name2 = item2,
             *              }
             * */
            stackDump(L, __LINE__, __FUNCTION__);
            lua_register_class(L, obj);
            stackDump(L, __LINE__, __FUNCTION__);

            // LUA_REGISTRYINDEX.__objects__, tObj,  _G."_class_meta:"#ClassName.meta
            luaL_getmetatable(L, meta_name);
            stackDump(L, __LINE__, __FUNCTION__);
        }

        // LUA_REGISTRYINDEX.__objects__, tObj,  _G."_class_meta:"#ClassName.meta

        /*
         * LUA_REGISTRYINDEX.__objects__, tObj,
         tObj.meta = _G."_class_meta:"#ClassName.meta
         _G."_class_meta:"#ClassName = {__index = indexTab, __newindex = newindexTab, __gc = gcTab
                           mem_name1 = item1,
                           mem_name2 = item2,
                          }
        */
        lua_setmetatable(L, -2);

        /*
        * LUA_REGISTRYINDEX.__objects__, tObj, tObj
        * tObj.meta = _G."_class_meta:"#ClassName.meta
        */
        lua_pushvalue(L, -1);

        /*
        * LUA_REGISTRYINDEX.__objects__, tObj
        * tObj.meta = _G."_class_meta:"#ClassName.meta
        * LUA_REGISTRYINDEX.__objects__.obj = tObj
        */
        lua_rawsetp(L, -3, obj);
    }
    stackDump(L, __LINE__, __FUNCTION__);

    // LUA_REGISTRYINDEX.__objects__, LUA_REGISTRYINDEX.__objects__.obj

    // LUA_REGISTRYINDEX.__objects__.obj
    lua_remove(L, -2);
    stackDump(L, __LINE__, __FUNCTION__);
}

template <typename T>
void lua_detach(lua_State* L, T obj) {
    if (obj == nullptr)
        return;

    _lua_del_fence(L, obj);

    lua_getfield(L, LUA_REGISTRYINDEX, "__objects__");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    // stack: __objects__
    if (lua_rawgetp(L, -1, obj) != LUA_TTABLE) {
        lua_pop(L, 2);
        return;
    }

    // stack: __objects__, __shadow_object__
    lua_pushstring(L, "__pointer__");
    lua_pushnil(L);
    lua_rawset(L, -3);

    lua_pushnil(L);
    lua_rawsetp(L, -3, obj);
    lua_pop(L, 2);
}

template<typename T>
struct has_meta_data {
    template<typename U> static auto check_meta(int) -> decltype(std::declval<U>().lua_get_meta_data(), std::true_type());
    template<typename U> static std::false_type check_meta(...);
    enum { value = std::is_same<decltype(check_meta<T>(0)), std::true_type>::value };
};

template <typename T>
T lua_to_object(lua_State* L, int idx) {//伪索引
    stackDump(L, __LINE__, __FUNCTION__);
    T obj = nullptr;

    static_assert(has_meta_data<typename std::remove_pointer<T>::type>::value, "T should be declared export !");

    //转换成正向索引
    idx = lua_normal_index(L, idx);

    //类对象
    if (lua_istable(L, idx)) {
        //tObj ..
        //tObj .., __pointer__
        lua_pushstring(L, "__pointer__");

        //tObj ..,  obj
        lua_rawget(L, idx);

        obj = (T)lua_touserdata(L, -1);

        //tObj ..,
        lua_pop(L, 1);
    }

    //tObj, name
    stackDump(L, __LINE__, __FUNCTION__);
    return obj;
}

#define DECLARE_LUA_CLASS(ClassName)    \
    const char* lua_get_meta_name() { return "_class_meta:"#ClassName; }    \
    lua_member_item* lua_get_meta_data();

#define LUA_EXPORT_CLASS_BEGIN(ClassName)   \
lua_member_item* ClassName::lua_get_meta_data() { \
    using class_type = ClassName;  \
    static lua_member_item s_member_list[] = {

#define LUA_EXPORT_CLASS_END()    \
        { nullptr, 0, luna_member_wrapper(), luna_member_wrapper()}  \
    };  \
    return s_member_list;  \
}

#define LUA_EXPORT_PROPERTY_AS(Member, Name)   {Name, offsetof(class_type, Member), lua_export_helper::getter(((class_type*)nullptr)->Member), lua_export_helper::setter(((class_type*)nullptr)->Member)},
#define LUA_EXPORT_PROPERTY_READONLY_AS(Member, Name)   {Name, offsetof(class_type, Member), lua_export_helper::getter(((class_type*)nullptr)->Member), luna_member_wrapper()},
#define LUA_EXPORT_PROPERTY(Member)   LUA_EXPORT_PROPERTY_AS(Member, #Member)
#define LUA_EXPORT_PROPERTY_READONLY(Member)   LUA_EXPORT_PROPERTY_READONLY_AS(Member, #Member)

#define LUA_EXPORT_METHOD_AS(Method, Name) { Name, 0, lua_export_helper::getter(&class_type::Method), lua_export_helper::setter(&class_type::Method)},
#define LUA_EXPORT_METHOD_READONLY_AS(Method, Name) { Name, 0, lua_export_helper::getter(&class_type::Method), luna_member_wrapper()},
#define LUA_EXPORT_METHOD(Method) LUA_EXPORT_METHOD_AS(Method, #Method)
#define LUA_EXPORT_METHOD_READONLY(Method) LUA_EXPORT_METHOD_READONLY_AS(Method, #Method)

void lua_push_function(lua_State* L, lua_global_function func);
inline void lua_push_function(lua_State* L, lua_CFunction func) { lua_pushcfunction(L, func); }

template <typename T>
void lua_push_function(lua_State* L, T func) {
    //c_function:
    //          a.可能是原生的，
    //          b. 或者是lua_global_bridge包装的
    lua_push_function(L, lua_adapter(func));
}

template <typename T>
void lua_register_function(lua_State* L, const char* name, T func) {
    stackDump(L, __LINE__, __FUNCTION__);
    lua_push_function(L, func);
    stackDump(L, __LINE__, __FUNCTION__);
    lua_setglobal(L, name);
}

inline bool lua_get_global_function(lua_State* L, const char function[]) {
    lua_getglobal(L, function);
    return lua_isfunction(L, -1);
}

bool lua_get_table_function(lua_State* L, const char table[], const char function[]);

template <typename T>
void lua_set_table_function(lua_State* L, int idx, const char name[], T func) {
    idx = lua_normal_index(L, idx);
    lua_push_function(L, func);
    lua_setfield(L, idx, name);
}

template <typename T>
bool lua_get_object_function(lua_State* L, T* object, const char function[]) {
    lua_push_object(L, object);
    if (!lua_istable(L, -1))
        return false;
    lua_getfield(L, -1, function);
    lua_remove(L, -2);
    return lua_isfunction(L, -1);
}

template<size_t... integers, typename... var_types>
void lua_to_native_mutil(lua_State* L, std::tuple<var_types&...>& vars, std::index_sequence<integers...>&&) {
    int _[] = { 0, (std::get<integers>(vars) = lua_to_native<var_types>(L, (int)integers - (int)sizeof...(integers)), 0)... };
}

bool lua_call_function(lua_State* L, std::string* err, int arg_count, int ret_count);

template <typename... ret_types, typename... arg_types>
bool lua_call_function(lua_State* L, std::string* err, std::tuple<ret_types&...>&& rets, arg_types... args) {
    stackDump(L, __LINE__, __FUNCTION__);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    stackDump(L, __LINE__, __FUNCTION__);
    if (!lua_call_function(L, err, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    return true;
}

template <typename... ret_types, typename... arg_types>
bool lua_call_table_function(lua_State* L, std::string* err, const char table[], const char function[], std::tuple<ret_types&...>&& rets, arg_types... args) {
    stackDump(L, __LINE__, __FUNCTION__);
    lua_get_table_function(L, table, function);
    stackDump(L, __LINE__, __FUNCTION__);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    stackDump(L, __LINE__, __FUNCTION__);
    if (!lua_call_function(L, err, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    stackDump(L, __LINE__, __FUNCTION__);
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    stackDump(L, __LINE__, __FUNCTION__);
    return true;
}

template <typename T, typename... ret_types, typename... arg_types>
bool lua_call_object_function(lua_State* L, std::string* err, T* o, const char function[], std::tuple<ret_types&...>&& rets, arg_types... args) {
    lua_get_object_function(L, o, function);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(L, err, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    return true;
}

template <typename... ret_types, typename... arg_types>
bool lua_call_global_function(lua_State* L, std::string* err, const char function[], std::tuple<ret_types&...>&& rets, arg_types... args) {
    //
    stackDump(L, __LINE__, __FUNCTION__);

    //func,
    lua_getglobal(L, function);
    stackDump(L, __LINE__, __FUNCTION__);

    //func, arg1, arg2, arg3
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    stackDump(L, __LINE__, __FUNCTION__);
    if (!lua_call_function(L, err, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    stackDump(L, __LINE__, __FUNCTION__);
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    stackDump(L, __LINE__, __FUNCTION__);
    return true;
}

inline bool lua_call_table_function(lua_State* L, std::string* err, const char table[], const char function[]) {
    stackDump(L, __LINE__, __FUNCTION__);
    return lua_call_table_function(L, err, table, function, std::tie());
}
template <typename T> inline bool lua_call_object_function(lua_State* L, std::string* err, T* o, const char function[]) { return lua_call_object_function(L, err, o, function, std::tie()); }
inline bool lua_call_global_function(lua_State* L, std::string* err, const char function[]) { return lua_call_global_function(L, err, function, std::tie()); }

class lua_guard {
public:
    lua_guard(lua_State* L) : m_lvm(L) { m_top = lua_gettop(L); }
    ~lua_guard() { lua_settop(m_lvm, m_top); }
    lua_guard(const lua_guard& other) = delete;
    lua_guard(lua_guard&& other) = delete;
    lua_guard& operator =(const lua_guard&) = delete;
private:
    int m_top = 0;
    lua_State* m_lvm = nullptr;
};
