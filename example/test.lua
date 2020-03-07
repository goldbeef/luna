--访问导出的全局函数
--[[
print(add(1, 2))
print(del(1, 2))
--]]


--访问导出的类的成员/方法
myClass = NewMyClass()
print(myClass.func_a("hello", 10))
print(type(myClass))

for k, v in pairs(myClass) do
    print("tObj", k ,v)
end

for k, v in pairs(getmetatable(myClass)) do
    print("meta", k ,v)
end

--[[
meta = getmetatable("_class_meta:my_class")
for k, v in pairs(meta) do
    print("meta1", k ,v)
    for k2, v2 in pairs(v) do
        print("meta2", k2 ,v2)
    end
end
--]]

--print(myClass.func_a(nil, 10))
--print(myClass.func_a(nil, nil))
--print(myClass.name)

--[[
function myClass.some_func0()
    print("myClass.some_func0")
end

function myClass.func_a()
    print("myClass.func_a")
end

s2s = {}
function s2s.some_func0()
    print("s2s.some_func0")
end

function s2s.some_func2(var1, var2)
    print(string.format("s2s.some_func2, [%g], [%g]", var1, var2))
end

function s2s.some_func3(var1, var2, var3)
    print(string.format("s2s.some_func3, [%g], [%g], [%g]", var1, var2, var3))
end
--]]
