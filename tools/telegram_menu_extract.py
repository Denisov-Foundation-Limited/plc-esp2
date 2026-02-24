import re
import sys
from pathlib import Path


def find_matching(text, start, open_ch, close_ch):
    d = 0
    i = start
    n = len(text)
    in_str = None
    esc = False
    line_comment = False
    block_comment = False
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if line_comment:
            if c == '\n':
                line_comment = False
            i += 1
            continue
        if block_comment:
            if c == '*' and nxt == '/':
                block_comment = False
                i += 2
                continue
            i += 1
            continue
        if in_str:
            if esc:
                esc = False
            elif c == '\\':
                esc = True
            elif c == in_str:
                in_str = None
            i += 1
            continue
        if c == '/' and nxt == '/':
            line_comment = True
            i += 2
            continue
        if c == '/' and nxt == '*':
            block_comment = True
            i += 2
            continue
        if c in ('"', "'"):
            in_str = c
            i += 1
            continue
        if c == open_ch:
            d += 1
        elif c == close_ch:
            d -= 1
            if d == 0:
                return i
        i += 1
    raise RuntimeError('no matching brace')


def strip_leading_static(sig):
    s = sig.lstrip()
    prefix_len = len(sig) - len(s)
    s = re.sub(r'^(?:inline\s+)?static\s+', '', s, count=1)
    return sig[:prefix_len] + s


def strip_default_args(sig):
    i = sig.find('(')
    if i < 0:
        return sig
    j = find_matching(sig, i, '(', ')')
    params = sig[i + 1:j]
    out = []
    par = ang = brk = brace = 0
    in_str = None
    esc = False
    k = 0
    while k < len(params):
        c = params[k]
        if in_str:
            out.append(c)
            if esc:
                esc = False
            elif c == '\\':
                esc = True
            elif c == in_str:
                in_str = None
            k += 1
            continue
        if c in ('"', "'"):
            in_str = c
            out.append(c)
            k += 1
            continue
        if c == '<':
            ang += 1
        elif c == '>' and ang > 0:
            ang -= 1
        elif c == '(':
            par += 1
        elif c == ')' and par > 0:
            par -= 1
        elif c == '[':
            brk += 1
        elif c == ']' and brk > 0:
            brk -= 1
        elif c == '{':
            brace += 1
        elif c == '}' and brace > 0:
            brace -= 1
        if c == '=' and par == 0 and ang == 0 and brk == 0 and brace == 0:
            k += 1
            while k < len(params):
                c2 = params[k]
                if in_str:
                    if esc:
                        esc = False
                    elif c2 == '\\':
                        esc = True
                    elif c2 == in_str:
                        in_str = None
                    k += 1
                    continue
                if c2 in ('"', "'"):
                    in_str = c2
                    k += 1
                    continue
                if c2 == '<':
                    ang += 1
                elif c2 == '>' and ang > 0:
                    ang -= 1
                elif c2 == '(':
                    par += 1
                elif c2 == ')' and par > 0:
                    par -= 1
                elif c2 == '[':
                    brk += 1
                elif c2 == ']' and brk > 0:
                    brk -= 1
                elif c2 == '{':
                    brace += 1
                elif c2 == '}' and brace > 0:
                    brace -= 1
                if c2 == ',' and par == 0 and ang == 0 and brk == 0 and brace == 0:
                    out.append(',')
                    k += 1
                    break
                k += 1
            continue
        out.append(c)
        k += 1
    return sig[:i + 1] + ''.join(out) + sig[j:]


def qualify_signature(sig, cls):
    i = sig.find('(')
    if i < 0:
        return sig
    j = i - 1
    while j >= 0 and sig[j].isspace():
        j -= 1
    end = j + 1
    while j >= 0 and (sig[j].isalnum() or sig[j] == '_'):
        j -= 1
    start = j + 1
    if start >= end:
        return sig
    return sig[:start] + cls + '::' + sig[start:end] + sig[end:]


def transform(header_path, cpp_path):
    hp = Path(header_path)
    cp = Path(cpp_path)
    text = hp.read_text(encoding='utf-8')
    m = re.search(r'class\s+(TelegramMenu\w+)\s*\{', text)
    if not m:
        raise RuntimeError(f'class not found in {header_path}')
    cls = m.group(1)
    open_pos = text.find('{', m.end() - 1)
    close_pos = find_matching(text, open_pos, '{', '}')
    body = text[open_pos + 1:close_pos]

    out = []
    defs = []
    i = 0
    n = len(body)

    def scan_delim(pos):
        par = ang = brk = 0
        in_str = None
        esc = False
        line = False
        block = False
        j = pos
        while j < n:
            c = body[j]
            nxt = body[j + 1] if j + 1 < n else ''
            if line:
                if c == '\n':
                    line = False
                j += 1
                continue
            if block:
                if c == '*' and nxt == '/':
                    block = False
                    j += 2
                    continue
                j += 1
                continue
            if in_str:
                if esc:
                    esc = False
                elif c == '\\':
                    esc = True
                elif c == in_str:
                    in_str = None
                j += 1
                continue
            if c == '/' and nxt == '/':
                line = True
                j += 2
                continue
            if c == '/' and nxt == '*':
                block = True
                j += 2
                continue
            if c in ('"', "'"):
                in_str = c
                j += 1
                continue
            if c == '(':
                par += 1
            elif c == ')' and par > 0:
                par -= 1
            elif c == '<':
                ang += 1
            elif c == '>' and ang > 0:
                ang -= 1
            elif c == '[':
                brk += 1
            elif c == ']' and brk > 0:
                brk -= 1
            elif c == '{' and par == 0 and ang == 0 and brk == 0:
                return j, '{'
            elif c == ';' and par == 0 and ang == 0 and brk == 0:
                return j, ';'
            j += 1
        return n, ''

    while i < n:
        macc = re.match(r'(\s*(?:public|private|protected)\s*:\s*)', body[i:])
        if macc:
            out.append(macc.group(1))
            i += len(macc.group(1))
            continue
        dpos, kind = scan_delim(i)
        if not kind:
            out.append(body[i:])
            break
        if kind == ';':
            out.append(body[i:dpos + 1])
            i = dpos + 1
            continue
        sig = body[i:dpos]
        if '(' not in sig:
            block_end = find_matching(body, dpos, '{', '}')
            out.append(body[i:block_end + 1])
            i = block_end + 1
            continue
        block_end = find_matching(body, dpos, '{', '}')
        func_body = body[dpos:block_end + 1]
        out.append(sig.rstrip() + '\n    ;')
        def_sig = qualify_signature(strip_default_args(strip_leading_static(sig)), cls)
        defs.append(def_sig + func_body + '\n')
        i = block_end + 1

    hp.write_text(text[:open_pos + 1] + ''.join(out) + text[close_pos:], encoding='utf-8')

    cpp_text = cp.read_text(encoding='utf-8')
    marker = '\n// ---- telegram menu extracted definitions ----\n'
    base = cpp_text.split(marker)[0]
    cp.write_text(base + marker + ''.join(defs), encoding='utf-8')
    return cls, len(defs)


if __name__ == '__main__':
    cls, cnt = transform(sys.argv[1], sys.argv[2])
    print(f'{cls}: extracted {cnt} methods')
