#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对比两次「文件快照」（ls -lR 的输出），找出新增 / 删除 / 大小或时间变化的文件。

用法：
    python diff_files.py <旧目录> <新目录>
    两个目录里应有同名文件：snap_priv.txt / snap_ext.txt / snap_tmp.txt / ...

输出：
    [+] 新增文件
    [-] 删除文件
    [~] 变化文件（大小或 mtime 不同）—— 这类最可疑
"""
import io
import os
import re
import sys

# 匹配 ls -lR 里的文件行：  -rw------- 1 u0_a343 u0_a343 163840 2026-10-25 18:43 文件名
FILE_RE = re.compile(
    r'^[-dlbcsp][rwxsStT-]{9}\s+\d+\s+\S+\s+\S+\s+(\d+)\s+(\d{4}-\d{2}-\d{2})\s+(\d{2}:\d{2})\s+(.+)$'
)


def parse(path):
    """返回 {相对路径: (大小, mtime)}，目录行也记录（大小=0）"""
    out = {}
    cur_dir = ''
    if not os.path.exists(path):
        return out
    for ln in io.open(path, encoding='utf-8', errors='replace'):
        ln = ln.rstrip('\n')
        if ln.endswith(':') and ln.startswith('/'):
            cur_dir = ln[:-1]
            continue
        m = FILE_RE.match(ln)
        if not m:
            continue
        size, date, tm, name = m.group(1), m.group(2), m.group(3), m.group(4)
        if name in ('.', '..'):
            continue
        full = (cur_dir + '/' + name) if cur_dir else name
        out[full] = (int(size), date + ' ' + tm)
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return
    old_dir, new_dir = sys.argv[1], sys.argv[2]
    names = ['snap_priv.txt', 'snap_ext.txt', 'snap_tmp.txt', 'snap_sdcard.txt']
    total = 0
    for n in names:
        a = parse(os.path.join(old_dir, n))
        b = parse(os.path.join(new_dir, n))
        if not a and not b:
            continue
        added = sorted(set(b) - set(a))
        removed = sorted(set(a) - set(b))
        changed = sorted(k for k in set(a) & set(b) if a[k] != b[k])
        if not (added or removed or changed):
            print("== %s : 无变化 ==" % n)
            continue
        print("\n===== %s =====" % n)
        for k in added:
            print("  [+] %s  (%d B, %s)" % (k, b[k][0], b[k][1]))
        for k in removed:
            print("  [-] %s" % k)
        for k in changed:
            print("  [~] %s  %dB/%s  ->  %dB/%s" % (k, a[k][0], a[k][1], b[k][0], b[k][1]))
        total += len(added) + len(removed) + len(changed)
    print("\n合计变化 %d 项" % total)


if __name__ == '__main__':
    main()
