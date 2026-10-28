#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对比两次快照（加密态 / 非加密态），找出「只在这两种状态下不同」的字段。

用法：
    python diff_snapshot.py snapshots/enc_on/dump_enc_on.txt snapshots/enc_off/dump_enc_off.txt

逻辑：
  1. 两份 dump 各自按 actor 分组（真人 / 人机）
  2. 对每个偏移，比较「加密态典型值」vs「非加密态典型值」
  3. 输出：
     A) 真人组：加密态 vs 非加密态 不同的字段  ← 这就是加密标志/被加密的数据
     B) 人机组：两次是否一致（应该完全一致，用来验证「人机不受影响」）
"""
import io
import re
import sys
from collections import defaultdict


def load(path):
    d = io.open(path, encoding='utf-8', errors='replace').read()
    blocks = re.split(r'\n=== ', d)
    out = {'p': [], 'a': []}
    for b in blocks[1:]:
        head = b.split('\n', 1)[0]
        if 'BP_UamCharacter_C' in head:
            key = 'p'
        elif 'BP_UamAICharacter_C' in head:
            key = 'a'
        else:
            continue
        vals = {}
        for ln in b.split('\n')[1:]:
            m = re.match(r'^([0-9a-f]{4}) ([0-9a-f]{8})$', ln.strip())
            if m:
                vals[int(m.group(1), 16)] = m.group(2)
        nm = re.search(r'# name=(.*)', b)
        out[key].append((nm.group(1) if nm else '?', vals))
    return out


def typical(actors, off):
    """同组内该偏移的一致值；不一致或全空返回 None"""
    s = {v.get(off, '-') for _, v in actors}
    if len(s) == 1:
        v = list(s)[0]
        return v if v != '-' else None
    return None


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return
    A = load(sys.argv[1])   # 加密态
    B = load(sys.argv[2])   # 非加密态
    print("加密态  : 真人 %d / 人机 %d" % (len(A['p']), len(A['a'])))
    print("非加密态: 真人 %d / 人机 %d" % (len(B['p']), len(B['a'])))

    offs = set()
    for g in (A, B):
        for k in ('p', 'a'):
            for _, v in g[k]:
                offs |= set(v.keys())

    print("\n=== A) 真人组：加密态 vs 非加密态 不同的字段（加密标志候选）===")
    n = 0
    for o in sorted(offs):
        va = typical(A['p'], o)
        vb = typical(B['p'], o)
        if va is None or vb is None:
            continue
        if va != vb:
            n += 1
            print("  +%04x   加密=%s   正常=%s" % (o, va, vb))
    print("  共 %d 个" % n)

    print("\n=== B) 人机组：两次是否一致（应完全一致，证明人机不受影响）===")
    diff = 0
    for o in sorted(offs):
        va = typical(A['a'], o)
        vb = typical(B['a'], o)
        if va is None or vb is None:
            continue
        if va != vb:
            diff += 1
            if diff <= 30:
                print("  +%04x   加密=%s   正常=%s" % (o, va, vb))
    print("  差异字段数 %d（理想为 0）" % diff)


if __name__ == '__main__':
    main()
