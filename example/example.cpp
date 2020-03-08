#include <iostream>
#include "luna.h"
//#include "luna11.h"
#include <lauxlib.h>
using namespace std;

int add(int a, int b) {
    return a + b;
}

int del(int a, int b) {
    return a - b;
}


void stackDump1(lua_State* L) {
    int top = lua_gettop(L);
    printf("stack begin, total[%d]\n", top);

    for (int i = 1; i <= top; i++) {
        int type = lua_type(L, i);
        switch (type) {
            case LUA_TSTRING:
                printf("'%s'", lua_tostring(L, i)); break;
            case LUA_TBOOLEAN:
                printf(lua_toboolean(L, i) ? "true" : "false"); break;
            case LUA_TNUMBER:
                printf("%g", lua_tonumber(L, i)); break;
            default:
                printf("%s", lua_typename(L, type)); break;
        }
        printf("\t");
    }
    printf("\n");

    printf("stack end\n");
}

class my_class final {
    int func_a(const char* a, int b) {
        printf("func_a, a[%s], b[%d]\n", a, b);
        return 0;
    }
public:
    char m_name[32];
    my_class() {
        snprintf(m_name, sizeof(m_name), "hello");
    }

    ~my_class() {
        printf("my_class::~my_class\n");
    }

    /*
    void __gc() {
        printf("my_class::__gc\n");
    }
    */

    //类导出声明
    DECLARE_LUA_CLASS(my_class);
};

//导出类
LUA_EXPORT_CLASS_BEGIN(my_class)
//导出类方法
LUA_EXPORT_METHOD(func_a)
//导出类成员
LUA_EXPORT_PROPERTY(m_name)
LUA_EXPORT_CLASS_END()


my_class* NewMyClass() {
    my_class* ptr = new my_class();
    return ptr;
}


int main(){
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    //导出全局函数
    lua_register_function(L, "add", add);
    //luaL_loadstring(L, "print(add(1+a))");
    //lua_register_function(L, "del", del);

    //导出函数对象
    lua_register_function(L, "NewMyClass", NewMyClass);//导出全局函数
    luaL_dofile(L, "./test.lua");

    /*
    //c++ call
    stackDump(L);
    luaL_loadstring(L, "print(add(1+a))");
    stackDump(L);
    lua_pcall(L, 0, 0, 0);
    stackDump(L);
    */

    /*
    //lua call
    //调用整个文件
    luaL_loadfile(L, "./test.lua");
    stackDump(L);
    lua_pcall(L, 0, 0, 0);

    //调用局部方法
    lua_guard g(L); //用它来做栈保护
    int x, y;
    const char* name = nullptr;
    // 小心,如果用char*做字符串返回值的话,确保name变量不要在lua_guard g的作用域之外使用
    "cout << lua_call_table_function(L, nullptr, "s2s", "some_func0") << endl; //true
    cout << lua_call_table_function(L, nullptr, "s2s", "some_func_no_such") << endl; //false
    cout << lua_call_table_function(L, nullptr, "s2s", "some_func2", std::tie(), 11, 2) << endl;
    cout << lua_call_table_function(L, nullptr, "s2s", "some_func3", std::tie(), 11, 2, 3) << endl;


    //调用对象的 方法
    cout << lua_call_table_function(L, nullptr, "myClass", "some_func0") << endl;
    //覆盖对象的 方法
    cout << lua_call_table_function(L, nullptr, "myClass", "func_a") << endl;

    lua_close(L);
     */

    int a, b;
    lua_call_global_function(L, nullptr, "globalAdd", std::tie(a, b), 1, 2);
    printf("a[%d], b[%d]\n", a, b);
    //lua_call_table_function( )
    //lua_call_object_function()
    return 0;

}