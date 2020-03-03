--访问导出的全局函数
--[[
print(add(1, 2))
print(del(1, 2))
--]]

--访问导出的类的成员/方法
--[[
myClass = NewMyClass()
print(myClass.func_a("hello", 10))
print(myClass.func_a(nil, 10))
print(myClass.func_a(nil, nil))
print(myClass.name)
--]]


function s2s.some_func0()
    print("s2s.some_func0")
end
