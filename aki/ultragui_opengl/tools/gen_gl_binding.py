#!/usr/bin/env python3
"""Generates src/sys/gl.aki, the OpenGL 3.3 core entry points and constants
the OpenGL RHI (ugui/rhi/opengl/opengl_rhi.cc) and ugui_impl_opengl3 call.

  python3 tools/gen_gl_binding.py /path/to/GL/glcorearb.h > src/sys/gl.aki

The function list is the C++ RHI's GL_FUNC table (the renderer's table is a
subset of it); constants are looked up in glcorearb.h by name.
"""
import re
import sys

# (C name, C result, [C parameters])
FUNCS = [
    ("glGenVertexArrays", "void", ["GLsizei", "GLuint*"]),
    ("glBindVertexArray", "void", ["GLuint"]),
    ("glDeleteVertexArrays", "void", ["GLsizei", "const GLuint*"]),
    ("glGenBuffers", "void", ["GLsizei", "GLuint*"]),
    ("glBindBuffer", "void", ["GLenum", "GLuint"]),
    ("glDeleteBuffers", "void", ["GLsizei", "const GLuint*"]),
    ("glBufferData", "void", ["GLenum", "GLsizeiptr", "const void*", "GLenum"]),
    ("glBufferSubData", "void", ["GLenum", "GLintptr", "GLsizeiptr", "const void*"]),
    ("glGenTextures", "void", ["GLsizei", "GLuint*"]),
    ("glBindTexture", "void", ["GLenum", "GLuint"]),
    ("glDeleteTextures", "void", ["GLsizei", "const GLuint*"]),
    ("glTexImage2D", "void", ["GLenum", "GLint", "GLint", "GLsizei", "GLsizei", "GLint", "GLenum", "GLenum", "const void*"]),
    ("glTexSubImage2D", "void", ["GLenum", "GLint", "GLint", "GLint", "GLsizei", "GLsizei", "GLenum", "GLenum", "const void*"]),
    ("glTexParameteri", "void", ["GLenum", "GLenum", "GLint"]),
    ("glGenFramebuffers", "void", ["GLsizei", "GLuint*"]),
    ("glBindFramebuffer", "void", ["GLenum", "GLuint"]),
    ("glDeleteFramebuffers", "void", ["GLsizei", "const GLuint*"]),
    ("glFramebufferTexture2D", "void", ["GLenum", "GLenum", "GLenum", "GLuint", "GLint"]),
    ("glCheckFramebufferStatus", "GLenum", ["GLenum"]),
    ("glCreateShader", "GLuint", ["GLenum"]),
    ("glShaderSource", "void", ["GLuint", "GLsizei", "const GLchar* const*", "const GLint*"]),
    ("glCompileShader", "void", ["GLuint"]),
    ("glGetShaderiv", "void", ["GLuint", "GLenum", "GLint*"]),
    ("glGetShaderInfoLog", "void", ["GLuint", "GLsizei", "GLsizei*", "GLchar*"]),
    ("glDeleteShader", "void", ["GLuint"]),
    ("glCreateProgram", "GLuint", []),
    ("glAttachShader", "void", ["GLuint", "GLuint"]),
    ("glLinkProgram", "void", ["GLuint"]),
    ("glUseProgram", "void", ["GLuint"]),
    ("glDeleteProgram", "void", ["GLuint"]),
    ("glGetProgramiv", "void", ["GLuint", "GLenum", "GLint*"]),
    ("glGetProgramInfoLog", "void", ["GLuint", "GLsizei", "GLsizei*", "GLchar*"]),
    ("glGetUniformLocation", "GLint", ["GLuint", "const GLchar*"]),
    ("glUniform1i", "void", ["GLint", "GLint"]),
    ("glUniform2f", "void", ["GLint", "GLfloat", "GLfloat"]),
    ("glEnableVertexAttribArray", "void", ["GLuint"]),
    ("glVertexAttribPointer", "void", ["GLuint", "GLint", "GLenum", "GLboolean", "GLsizei", "const void*"]),
    ("glVertexAttribIPointer", "void", ["GLuint", "GLint", "GLenum", "GLsizei", "const void*"]),
    ("glViewport", "void", ["GLint", "GLint", "GLsizei", "GLsizei"]),
    ("glScissor", "void", ["GLint", "GLint", "GLsizei", "GLsizei"]),
    ("glEnable", "void", ["GLenum"]),
    ("glDisable", "void", ["GLenum"]),
    ("glBlendFunc", "void", ["GLenum", "GLenum"]),
    ("glBlendFuncSeparate", "void", ["GLenum", "GLenum", "GLenum", "GLenum"]),
    ("glBlendEquation", "void", ["GLenum"]),
    ("glClearColor", "void", ["GLfloat", "GLfloat", "GLfloat", "GLfloat"]),
    ("glClear", "void", ["GLbitfield"]),
    ("glDrawElements", "void", ["GLenum", "GLsizei", "GLenum", "const void*"]),
    ("glDrawArrays", "void", ["GLenum", "GLint", "GLsizei"]),
    ("glActiveTexture", "void", ["GLenum"]),
    ("glPixelStorei", "void", ["GLenum", "GLint"]),
    ("glGetString", "const GLubyte*", ["GLenum"]),
    ("glGetStringi", "const GLubyte*", ["GLenum", "GLuint"]),
    ("glGetIntegerv", "void", ["GLenum", "GLint*"]),
]

CONSTS = [
    "GL_FALSE", "GL_TRIANGLES", "GL_TEXTURE_2D", "GL_CULL_FACE", "GL_DEPTH_TEST",
    "GL_BLEND", "GL_SCISSOR_TEST", "GL_UNPACK_ALIGNMENT", "GL_TEXTURE0",
    "GL_TEXTURE1", "GL_TEXTURE2", "GL_ARRAY_BUFFER", "GL_ELEMENT_ARRAY_BUFFER",
    "GL_DYNAMIC_DRAW", "GL_FRAGMENT_SHADER", "GL_VERTEX_SHADER",
    "GL_COMPILE_STATUS", "GL_LINK_STATUS", "GL_FLOAT", "GL_UNSIGNED_INT",
    "GL_UNSIGNED_BYTE", "GL_RGBA", "GL_RGBA8", "GL_SRGB8_ALPHA8", "GL_BGRA",
    "GL_RED", "GL_R8", "GL_TEXTURE_MIN_FILTER", "GL_TEXTURE_MAG_FILTER",
    "GL_TEXTURE_WRAP_S", "GL_TEXTURE_WRAP_T", "GL_NEAREST", "GL_LINEAR",
    "GL_CLAMP_TO_EDGE", "GL_SRC_ALPHA", "GL_ONE", "GL_ONE_MINUS_SRC_ALPHA",
    "GL_FUNC_ADD", "GL_FRAMEBUFFER", "GL_COLOR_ATTACHMENT0",
    "GL_COLOR_BUFFER_BIT", "GL_FRAMEBUFFER_COMPLETE", "GL_FRAMEBUFFER_SRGB",
    "GL_VERSION", "GL_RENDERER", "GL_NUM_EXTENSIONS", "GL_EXTENSIONS",
]
# EXT_texture_sRGB_decode: glcorearb.h is core-only, so the C++ spells these out.
EXTRA_CONSTS = [("GL_TEXTURE_SRGB_DECODE_EXT", 0x8A48), ("GL_SKIP_DECODE_EXT", 0x8A4A)]

SCALARS = {
    "GLenum": "u32", "GLuint": "u32", "GLint": "i32", "GLsizei": "i32",
    "GLfloat": "f32", "GLbitfield": "u32", "GLboolean": "u8",
    "GLintptr": "i64", "GLsizeiptr": "i64", "GLchar": "c_char", "GLubyte": "u8",
    "void": "u8",
}


def aki_type(c):
    c = c.strip()
    if c == "void":
        return "()"
    if c == "const GLchar* const*":
        return "*const *const c_char"
    const = c.startswith("const ")
    base = c[6:] if const else c
    if base.endswith("*"):
        return ("*const " if const else "*var ") + SCALARS[base[:-1].strip()]
    return SCALARS[base]


def snake(name):
    s = name[2:].replace("2D", "2d")
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", s).lower()


def const_name(c):
    return "kGl" + "".join(p.capitalize() if not p.isdigit() else p for p in c[3:].split("_")) \
        .replace("Ext", "Ext")


def main():
    header = open(sys.argv[1]).read()
    out = []
    w = out.append
    w("// Generated by tools/gen_gl_binding.py from GL/glcorearb.h: do not edit.")
    w("//")
    w("// No C++ counterpart: the C++ declares these as PFN_ typedefs and globals in")
    w("// each .cc (GL_FUNC in opengl_rhi.cc, GL_PROCS in ugui_impl_opengl3.cc). Here")
    w("// they are fields of one dispatch table, `GlApi`, filled by `GlApi::load` through")
    w("// a get-proc-address, like the C++ loaders. GL types: GLenum/GLuint u32,")
    w("// GLint/GLsizei i32, GLfloat f32, GLintptr/GLsizeiptr i64, GLboolean u8.")
    w("")
    w("module ultragui_opengl::sys::gl;")
    w("")
    w("import {eprintln} from akikit::logging;")
    w("")
    for c in CONSTS:
        m = re.search(r"#define\s+%s\s+(0x[0-9A-Fa-f]+|\d+)\b" % c, header)
        if not m:
            sys.exit("constant %s not found in header" % c)
        w("pub comptime let %s: u32 = %s;" % (const_name(c), m.group(1)))
    for c, v in EXTRA_CONSTS:
        w("pub comptime let %s: u32 = 0x%X;" % (const_name(c), v))
    w("")
    w("// A get-proc-address: GLFW's, or the host's. Null for an entry point it lacks.")
    w("pub type GlLoader = fn(*const c_char): *var u8;")
    w("")
    def sig(args, ret):
        return "extern fn(%s): %s" % (", ".join(aki_type(x) for x in args), aki_type(ret))

    w("// Every entry point as a C function pointer, called through the method of the")
    w("// same name (`unsafe fn` where it takes a pointer, as C does not check them).")
    w("pub struct GlApi : Copy {")
    for name, ret, args in FUNCS:
        w("  let p_%s: %s" % (snake(name), sig(args, ret)))
    w("")
    w("  // GL_LOAD for every entry point, in the C++ order: stops at the first one the")
    w("  // loader cannot supply (none), as load_gl_functions() returns false.")
    w("  pub fn load(loader: GlLoader): GlApi?;")
    w("")
    w("  // A table with every entry null, for state that is loaded later; calling")
    w("  // through it crashes, as calling an unloaded PFN_ global does in the C++.")
    w("  pub fn unloaded(): GlApi;")
    w("")
    for name, ret, args in FUNCS:
        n = snake(name)
        params = ", ".join("a%d: %s" % (i, aki_type(x)) for i, x in enumerate(args))
        call = ", ".join("a%d" % i for i in range(len(args)))
        has_ptr = any("*" in x for x in args)
        res = "" if ret == "void" else ": " + aki_type(ret)
        if has_ptr:
            w("  pub unsafe fn %s(self&%s)%s => unsafe (self.p_%s)(%s);" % (n, (", " + params) if params else "", res, n, call))
        else:
            w("  pub fn %s(self&%s)%s => unsafe (self.p_%s)(%s);" % (n, (", " + params) if params else "", res, n, call))
    w("}")
    w("")
    w("// Resolves one entry point. Prints the C++ loader's message when it is missing.")
    w("fn resolve(loader: GlLoader, name: *const c_char, text: str): (*var u8)? {")
    w("  let p = loader(name);")
    w("  if (p == null) {")
    w("    eprintln(f\"ultragui: failed to load GL function '{text}'\");")
    w("    return none;")
    w("  }")
    w("  return p;")
    w("}")
    w("")
    w("fn GlApi::load(loader: GlLoader): GlApi? {")
    for name, ret, args in FUNCS:
        w("  let %s = resolve(loader, c\"%s\", \"%s\") else { return none; };" % (snake(name), name, name))
    w("  return unsafe GlApi {")
    for name, ret, args in FUNCS:
        w("    p_%s: %s as %s," % (snake(name), snake(name), sig(args, ret)))
    w("  };")
    w("}")
    w("")
    w("fn GlApi::unloaded(): GlApi {")
    w("  return unsafe GlApi {")
    for name, ret, args in FUNCS:
        w("    p_%s: 0usize as %s," % (snake(name), sig(args, ret)))
    w("  };")
    w("}")
    sys.stdout.write("\n".join(out) + "\n")


main()
