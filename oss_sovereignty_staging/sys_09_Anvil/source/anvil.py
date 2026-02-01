#!/usr/bin/env python3
import sys
import os
import re

# Anvil Sovereign Compiler (Stage 1: Transpiler)
# Converts .anv (Rust-subset) to .c (ANSI C)

def log(msg):
    print(f">> [ANVIL] {msg}", file=sys.stderr)

TYPE_MAP = {
    "u8": "uint8_t",
    "u16": "uint16_t",
    "u32": "uint32_t",
    "u64": "uint64_t",
    "i8": "int8_t",
    "i16": "int16_t",
    "i32": "int32_t",
    "i64": "int64_t",
    "usize": "size_t",
    "bool": "int",
    "void": "void",
    "&str": "const char*",
    "&[u8]": "const uint8_t*",
    "Vec<u8>": "uint8_t*"
}

def map_type(t, context=""):
    t = t.strip()
    if t == "Self": return context
    if t in TYPE_MAP: return TYPE_MAP[t]
    if t.startswith("*const "): return "const " + map_type(t[7:], context) + "*"
    if t.startswith("*mut "): return map_type(t[5:], context) + "*"
    if t.startswith("&"): return map_type(t[1:], context) + "*" 
    return t

def find_blocks(text):
    blocks = []
    i = 0
    n = len(text)
    while i < n:
        while i < n and text[i].isspace(): i += 1
        if i >= n: break
        start = i
        brace_open = text.find('{', i)
        if brace_open == -1: break
        sig = text[i:brace_open].strip()
        btype = None
        bname = None
        if sig.startswith("struct "):
            btype = "struct"
            bname = sig[7:].strip()
        elif sig.startswith("impl "):
            btype = "impl"
            bname = sig[5:].strip()
        elif sig.startswith("fn "):
            btype = "fn"
            m = re.match(r'fn\s+(\w+)', sig)
            if m: bname = m.group(1)
        count = 1
        j = brace_open + 1
        while j < n and count > 0:
            if text[j] == '{': count += 1
            elif text[j] == '}': count -= 1
            j += 1
        if count == 0:
            body = text[brace_open+1 : j-1]
            if btype:
                blocks.append((btype, bname, body, sig))
            i = j
        else:
            break
    return blocks

def generate_header(sources):
    header = "#ifndef ANVIL_KERNEL_H\n#define ANVIL_KERNEL_H\n"
    header += "#include <stdint.h>\n#include <stddef.h>\n"
    header += "#define copy_nonoverlapping(src, dst, count) __builtin_memcpy(dst, src, (count) * sizeof(*(src)))\n\n"
    
    # 1. Struct Forward Decls & Definitions
    for src in sources:
        blocks = find_blocks(src)
        for btype, bname, body, sig in blocks:
            if btype == "struct":
                 header += "typedef struct %s %s;\n" % (bname, bname)
                 
    for src in sources:
        blocks = find_blocks(src)
        for btype, bname, body, sig in blocks:
            if btype == "struct":
                 fields = []
                 for f in body.split(','):
                    if ':' in f:
                        parts = f.split(':')
                        fname = parts[0]
                        ftype = parts[1]
                        c_type = map_type(ftype)
                        arr_match = re.match(r'\[(.+);(.+)\]', ftype.strip())
                        if arr_match:
                            base_type = map_type(arr_match.group(1))
                            count = arr_match.group(2)
                            fields.append("    %s %s[%s];" % (base_type, fname.strip(), count))
                        else:
                            fields.append("    %s %s;" % (c_type, fname.strip()))
                 header += "struct %s {\n%s\n};\n" % (bname, "\n".join(fields))

    # 2. Function Prototypes
    for src in sources:
        blocks = find_blocks(src)
        for btype, bname, body, sig in blocks:
            if btype == "impl":
                 struct_name = bname
                 inner_blocks = find_blocks(body)
                 for ibtype, ibname, ibody, isig in inner_blocks:
                     if ibtype == "fn":
                         m = re.match(r'fn\s+(\w+)\((.*)\)(.*)', isig, re.DOTALL)
                         if m:
                             name = m.group(1)
                             args = m.group(2)
                             ret_type = m.group(3).strip() if m.group(3) else None
                             if ret_type and ret_type.startswith("->"): ret_type = ret_type[2:].strip()
                             
                             c_ret = map_type(ret_type, struct_name) if ret_type else "void"
                             c_name = f"{struct_name}_{name}"
                             c_args = []
                             if args:
                                for a in args.split(','):
                                    if ':' in a:
                                        aname, atype = a.split(':')
                                        if aname.strip() == "self": c_args.append(f"{struct_name}* self")
                                        else: c_args.append(f"{map_type(atype, struct_name)} {aname.strip()}")
                                    elif a.strip() == "self": c_args.append(f"{struct_name} self")
                                    elif a.strip() == "&self": c_args.append(f"{struct_name}* self")
                                    elif a.strip() == "&mut self": c_args.append(f"{struct_name}* self")
                             header += "%s %s(%s);\n" % (c_ret, c_name, ', '.join(c_args))

            elif btype == "fn":
                 m = re.match(r'fn\s+(\w+)\((.*)\)(.*)', sig, re.DOTALL)
                 if m:
                     name = m.group(1)
                     args = m.group(2)
                     ret_type = m.group(3).strip() if m.group(3) else None
                     if ret_type and ret_type.startswith("->"): ret_type = ret_type[2:].strip()
                     c_ret = map_type(ret_type) if ret_type else "void"
                     c_args = []
                     if args:
                        for a in args.split(','):
                            if ':' in a:
                                aname, atype = a.split(':')
                                c_args.append(f"{map_type(atype)} {aname.strip()}")
                     header += "%s %s(%s);\n" % (c_ret, name, ', '.join(c_args))

    header += "#endif\n"
    return header

def replace_asm(text):
    out = ""
    i = 0
    n = len(text)
    while i < n:
        match = re.search(r'asm!\(', text[i:])
        if not match:
            out += text[i:]
            break
        
        start = i + match.start()
        out += text[i:start]
        
        open_idx = start + 4
        depth = 1
        j = open_idx + 1
        in_quote = False
        while j < n and depth > 0:
            if text[j] == '"' and text[j-1] != '\\':
                in_quote = not in_quote
            if not in_quote:
                if text[j] == '(': depth += 1
                elif text[j] == ')': depth -= 1
            j += 1
            
        if depth == 0:
            asm_content = text[open_idx+1:j-1]
            out += parse_asm_content(asm_content)
            i = j
        else:
            out += text[start:]
            break
    return out

def parse_asm_content(content):
    parts = []
    depth = 0
    in_quote = False
    curr = ""
    for char in content:
        if char == '"' and (len(curr)==0 or curr[-1] != '\\'):
            in_quote = not in_quote
            curr += char
        elif char == ',' and depth == 0 and not in_quote:
            parts.append(curr.strip())
            curr = ""
        else:
            if not in_quote:
                if char == '(': depth += 1
                elif char == ')': depth -= 1
            curr += char
    parts.append(curr.strip())
    
    template = parts[0]
    # Escape % for C asm
    if template.startswith('"') and template.endswith('"'):
         inner = template[1:-1]
         inner = inner.replace('%', '%%')
         template = f'"{inner}"'

    inputs = []
    outputs = []
    
    reg_map = {"al":"a", "ax":"a", "eax":"a", "rax":"a",
               "bl":"b", "bx":"b", "ebx":"b", "rbx":"b",
               "cl":"c", "cx":"c", "ecx":"c", "rcx":"c",
               "dl":"d", "dx":"d", "edx":"d", "rdx":"d",
               "si":"S", "rsi":"S", "di":"D", "rdi":"D"}

    for p in parts[1:]:
        if p.startswith("in"):
            m = re.match(r'in\("(\w+)"\)\s*(.+)', p)
            if m:
                reg = reg_map.get(m.group(1), "r")
                inputs.append(f'"{reg}"({m.group(2)})')
        elif p.startswith("out"):
            m = re.match(r'out\("(\w+)"\)\s*(.+)', p)
            if m:
                reg = reg_map.get(m.group(1), "r")
                outputs.append(f'"={reg}"({m.group(2)})')
    
    return f'__asm__ volatile({template} : {", ".join(outputs)} : {", ".join(inputs)} : );'

def do_transpile_fn(name, args, ret_type, body, context=""):
    c_ret = map_type(ret_type, context) if ret_type else "void"
    c_name = f"{context}_{name}" if context else name
    
    c_args = []
    if args:
        for a in args.split(','):
            if ':' in a:
                aname, atype = a.split(':')
                aname = aname.strip()
                if aname == "self": 
                    c_args.append(f"{context}* self")
                else:
                    c_args.append(f"{map_type(atype, context)} {aname}")
            elif a.strip() == "self":
                 c_args.append(f"{context} self") 
            elif a.strip() == "&self":
                 c_args.append(f"{context}* self") 
            elif a.strip() == "&mut self":
                 c_args.append(f"{context}* self") 
    
    c_args_str = ", ".join(c_args)
    
    c_body = body
    
    # 1. ASM Parser (Manual)
    c_body = replace_asm(c_body)

    # 2. Block/Unsafe/Loop
    c_body = c_body.replace("unsafe{", "{")
    c_body = c_body.replace("loop{", "while(1){") 
    
    # 3. Let bindings
    c_body = re.sub(r'let\s+mut\s+(\w+)\s*:\s*([^=;]+);', lambda m: f"{map_type(m.group(2), context)} {m.group(1)};", c_body)
    c_body = re.sub(r'let\s+mut\s+(\w+)\s*:\s*([^=]+)\s*=\s*(.+?);', lambda m: f"{map_type(m.group(2), context)} {m.group(1)} = {m.group(3)};", c_body)
    c_body = re.sub(r'let\s+mut\s+(\w+)\s*=\s*(.+?);', lambda m: f"__auto_type {m.group(1)} = {m.group(2)};", c_body)
    c_body = re.sub(r'let\s+(\w+)\s*:\s*([^=]+)\s*=\s*(.+?);', lambda m: f"{map_type(m.group(2), context)} {m.group(1)} = {m.group(3)};", c_body)
    c_body = re.sub(r'let\s+(\w+)\s*=\s*(.+?);', lambda m: f"__auto_type {m.group(1)} = {m.group(2)};", c_body)
    
    # 4. Casts
    c_body = re.sub(r'(0x[0-9a-fA-F]+)as', r'\1 as', c_body)
    c_body = re.sub(r'(\d)as\b', r'\1 as', c_body)
    c_body = re.sub(r'\)as\b\s*[\w]+', ')', c_body)
    c_body = re.sub(r'(\w+)\s+as\s*([\w\*\s]+)', lambda m: f"({map_type(m.group(2), context)}){m.group(1)}", c_body)
    
    # Control Flow
    c_body = re.sub(r'if\s+(.+?)\{', r'if (\1) {', c_body)
    c_body = re.sub(r'while\s+(.+?)\{', r'while (\1) {', c_body)

    # 5. Struct Literal
    c_body = re.sub(r'\b([A-Z]\w*)\{', r'(\1){', c_body)
    c_body = re.sub(r'\[.+?;.+?\]', '{{0}}', c_body)
    c_body = re.sub(r'(\w+):(?!:)', r'.\1 =', c_body) # Keys
    
    # 6. Pointers
    c_body = c_body.replace("self.", "self->")
    c_body = c_body.replace("&mut ", "&") # Ref mut -> Addr
    # *ptr.add(x) -> ptr[x]
    c_body = re.sub(r'\*\s*([\w\.\->]+)\.add\((.+?)\)', r'\1[\2]', c_body)
    # ptr.add(x) -> ptr + x
    c_body = re.sub(r'\.add\((.+?)\)', r' + \1', c_body)
    
    # Match -> Switch
    c_body = re.sub(r'match\s+(.+?)\{', r'switch(\1){', c_body)
    c_body = re.sub(r'(\d)\s*=>\s*\{', r'case \1: {', c_body)
    c_body = re.sub(r'_\s*=>\s*\{', r'default: {', c_body)

    c_body = c_body.replace("core::ptr::", "") 
    c_body = c_body.replace("::", "__") 
    c_body = c_body.replace(".as_mut_ptr()", "")
    c_body = c_body.replace(".as_ptr()", "")

    # 7. Implicit Return
    if c_ret != "void":
        c_body = c_body.strip()
        if not c_body.endswith(';'):
             if c_body.endswith('}'):
                 if c_body.lstrip().startswith('(') or c_body.lstrip().startswith(c_ret):
                      c_body = f"return {c_body};"
                 elif c_body.startswith("switch"):
                      pass 
             else:
                 last_semi = c_body.rfind(';')
                 if last_semi != -1:
                     last_chunk = c_body[last_semi+1:].strip()
                     if not last_chunk.startswith("}"): 
                        c_body = c_body[:last_semi+1] + f"\nreturn {last_chunk};"
                 else:
                     c_body = f"return {c_body};"

    return f"{c_ret} {c_name}({c_args_str}) {{\n{c_body}\n}}\n"

def transpile(source):
    c_code = '#include "kernel.h"\n\n'
    blocks = find_blocks(source)
    for btype, bname, body, sig in blocks:
        if btype == "impl":
             struct_name = bname
             inner_blocks = find_blocks(body)
             for ibtype, ibname, ibody, isig in inner_blocks:
                 if ibtype == "fn":
                     m = re.match(r'fn\s+(\w+)\((.*)\)(.*)', isig, re.DOTALL)
                     if m:
                         name = m.group(1)
                         args = m.group(2)
                         ret_type = m.group(3).strip() if m.group(3) else None
                         if ret_type and ret_type.startswith("->"): ret_type = ret_type[2:].strip()
                         c_code += do_transpile_fn(name, args, ret_type, ibody, struct_name)

    for btype, bname, body, sig in blocks:
        if btype == "fn":
             m = re.match(r'fn\s+(\w+)\((.*)\)(.*)', sig, re.DOTALL)
             if m:
                 name = m.group(1)
                 args = m.group(2)
                 ret_type = m.group(3).strip() if m.group(3) else None
                 if ret_type and ret_type.startswith("->"): ret_type = ret_type[2:].strip()
                 c_code += do_transpile_fn(name, args, ret_type, body, "")

    return c_code

def main():
    if len(sys.argv) < 2:
        print("Usage: anvil <command> [args]")
        return
    cmd = sys.argv[1]
    
    if cmd == "transpile":
        if len(sys.argv) < 4: return
        in_path = sys.argv[2]
        out_path = sys.argv[3]
        with open(in_path, 'r') as f: src = f.read()
        c_src = transpile(src)
        with open(out_path, 'w') as f: f.write(c_src)
        log(f"Transpiled {in_path}")
        
    elif cmd == "header":
        if len(sys.argv) < 4: return
        out_path = sys.argv[2]
        in_files = sys.argv[3:]
        sources = []
        for p in in_files:
            with open(p, 'r') as f: sources.append(f.read())
        header = generate_header(sources)
        with open(out_path, 'w') as f: f.write(header)
        log(f"Generated {out_path}")

if __name__ == "__main__":
    main()