""" 生成lua代码的包含文件
lua_codes.gen.h
"""

import math
import os, sys, re
import codecs
from platform_methods import subprocess_main
from io import StringIO
from hashlib import md5

def make_lua_templates(target, source, env):
    dst = target[0]

    code_str = StringIO()
    count = 0
    for f in source:
        count += 1
        fname = str(f)
        basename = os.path.splitext(os.path.basename(fname))[0]
        dirname = os.path.basename(os.path.dirname(fname))
        code_str.write("    { String(\"%s\"), String(\"%s\"), " % (dirname, basename))
        with open(fname, "r") as fr:
            bom = fr.read(3)
            if bom != codecs.BOM_UTF8:
                fr.seek(0)
            desc = fr.readline()[3:-1]
            code_str.write("String(\"%s\"), String(\"" % desc)
            code = fr.readline()
            while len(code) > 0:
                code_str.write("%s\\n" % code.replace("\"", "\\\"").rstrip(" \r\n\t"))
                code = fr.readline()

            code_str.write("\") },\n")

    code_str.write("};\n\n#endif")

    s = StringIO()
    s.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n\n")
    s.write("#ifndef _LUACODE_TEMPLATES_H\n#define _LUACODE_TEMPLATES_H\n\n")
    s.write("#include \"core/object/object.h\"\n#include \"core/object/script_language.h\"\n\n")
    s.write("static const int LUA_TEMPLATES_ARRAY_SIZE = " + str(count) + ";\n")
    s.write("static const struct ScriptLanguage::ScriptTemplate LUA_TEMPLATES[] = {\n")
    s.write(code_str.getvalue())

    with open(dst, "w") as f:
        f.write(s.getvalue())

    s.close()
    code_str.close()


def make_lua_codes_action(target, source, env):
    dst = target[0] #sys.path[0] + "/wrapper/" + os.path.basename(target[0])
    lua_codes = source

    icons_string = StringIO()

    for f in lua_codes:
        fname = str(f)
        basename = os.path.basename(fname).replace('.', '_')
        icons_string.write("\n// " + fname)
        icons_string.write('\nconst char* code_' + basename + ' = "')
        with open(fname, "rb") as codef:
            # UTF8-BOM
            bom = codef.read(3)
            if bom != codecs.BOM_UTF8:
                codef.seek(0)
            b = codef.read(1)
            while len(b) == 1:
                icons_string.write("\\" + str(hex(ord(b)))[1:])
                b = codef.read(1)

        icons_string.write('";\n')

    s = StringIO()
    s.write('/* THIS FILE IS GENERATED DO NOT EDIT */\n')
    s.write('#ifndef _LUA_CODES_GEN_H\n')
    s.write('#define _LUA_CODES_GEN_H\n')
    s.write(icons_string.getvalue())
    s.write('\n')
    s.write("#endif\n")

    with open(dst, "w") as f:
        f.write(s.getvalue())

    s.close()
    icons_string.close()

def make_wrap_codes_action(target, source, env):
    dir = os.getcwd() + "/../modules/luascript/wrapper/"
    cppfiles = source

    s_header = StringIO()
    s_header.write('/* THIS FILE IS GENERATED DO NOT EDIT */\n')
    s_header.write('#ifndef _WRAP_FOR_LUA_GEN_H\n')
    s_header.write('#define _WRAP_FOR_LUA_GEN_H\n')
    s_header.write('#include "../godot_lua_convert_api.h"\n\n')

    pattern = re.compile("(\w+)\s+(\w+)\s*\((.*)\)")
    TOFuncs = {
        'int' : 'luaL_optinteger(L, %d, 0)',
    }
    PUSHFuncs = {
        'int' : 'lua_pushinteger(L, %s)',
        'bool' : 'lua_pushboolean(L, %s)',
        'String' : 'lua_pushstring(L, GD_UTF8_STR(%s))',
        'StringName' : 'lua_pushstring(L, GD_UTF8_NAME(%s))',
    }
    funcs = []
    for s in cppfiles:
        cls_name = str.split(os.path.basename(s), '.')[0]
        s_header.write("/*\n * Generated members of: %s\n */\n" % cls_name)
        # TODO
        members = []
        with open(s, "r") as f:
            for l in f.readlines():
                m = pattern.match(l)
                if m != None:
                    members.append({ 'ret' : m.group(1), 'name' : m.group(2), 'args' : m.group(3).split(',') })

        for m in members:
            s_header.write('static int godot_%s_%s(lua_State *L) {\n' % (cls_name,  m['name']))
            s_header.write('    auto self = TO_USERDATA(%s, 1);\n' % cls_name)
            iarg = 0
            args = []
            for arg in m['args']:
                if arg != '':
                    tof = TOFuncs.get(arg, '')
                    if tof != '':
                        s_header.write(('    auto arg%d = ' + tof + ';\n') % (iarg, iarg + 2))
                        args.append("arg%d" % iarg)
                        iarg = iarg + 1

            if m['name'] == '__tostring':
                s_header.write('    lua_pushfstring(L, "[%s %%s]", GD_STR_NAME_DATA(self->operator %s()));\n' % (cls_name, m['ret']))
                s_header.write('    return 1;\n')
            else:
                if m['ret'] == 'void':
                    s_header.write('    self->%s(%s);\n' % (m['name'], str.join(', ', args)))
                    s_header.write('    return 0;\n')
                else:
                    s_header.write('    auto ret = self->%s(%s);\n' % (m['name'], str.join(', ', args)))
                    pushf = PUSHFuncs.get(m['ret'], '')
                    if pushf != '':
                        s_header.write(('    ' + pushf  + ';\n') % 'ret')
                    else:
                        s_header.write('    LuaScriptLanguage::push_userdata(L, "%s", ret);\n' % m['ret'])
                    s_header.write('    return 1;\n')
            s_header.write('}\n')
        # __gc
        s_header.write('static int godot_%s___gc(lua_State *L) {\n' % cls_name)
        s_header.write('    TO_USERDATA(%s, 1)->~%s();\n' % (cls_name, cls_name))
        s_header.write('    return 0;\n')
        s_header.write('}\n')

        fn = "register_godot_%s" % cls_name
        funcs.append(fn)
        s_header.write("static void %s(lua_State *L) {\n" % fn)
        for m in members:
            s_header.write('    luatable_rawset(L, -1, "%s", &godot_%s_%s);\n' % (m['name'], cls_name,  m['name']))
        s_header.write('    luatable_rawset(L, -1, "__gc", &godot_%s___gc);\n' % cls_name)
        s_header.write('}\n\n')

    s_header.write('// register for all types.\nextern void register_godot_userdata_types(lua_State *L) {\n')
    for f in funcs:
        s_header.write('    LuaScriptLanguage::add_register("%s", &%s);\n' % (cls_name, f))
    s_header.write('}\n')

    s_header.write('\n')
    s_header.write("#endif\n")

    header = os.path.basename(target[0])
    content = s_header.getvalue()
    with open(dir + header, "w") as f:
        f.write(s_header.getvalue())


    with open(target[0], "w") as f:
        f.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n// ")
        f.write(md5(str.encode(content, encoding='UTF-8')).hexdigest())
        f.write('\n')
        for s in source:
            f.write("//" + s + "\n")

    s_header.close()

if __name__ == "__main__":
    subprocess_main(globals())
