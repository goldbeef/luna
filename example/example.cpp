#include <iostream>
#include "luna.h"
#include <lauxlib.h>

int add(int a, int b) {
    return a + b;
}

int del(int a, int b) {
    return a - b;
}

void stackDump(lua_State* L) {
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
    }
public:
    DECLARE_LUA_CLASS(my_class);
};

LUA_EXPORT_CLASS_BEGIN(my_class)
LUA_EXPORT_METHOD(func_a)
LUA_EXPORT_CLASS_END()


int main(){
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);


    lua_register_function(L, "add", add);
    lua_register_function(L, "del", del);

    /*
    //c++ call
    stackDump(L);
    luaL_loadstring(L, "print(add(1+a))");
    stackDump(L);
    lua_pcall(L, 0, 0, 0);
    stackDump(L);
    */

    //lua call
    /*
    luaL_dofile(L, "./test.lua");
    */


    lua_close(L);
    return 0;

}