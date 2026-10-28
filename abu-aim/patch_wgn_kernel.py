#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
WGN 后端补丁：卡密分「环境卡」和「内核卡」两类
- createCard 增加 kind 参数（env / kernel），落库到 card.kind
- activateCard（环境激活）拒绝内核卡
- activateKernelCard（内核激活）拒绝环境卡
- 老卡密无 kind 字段时按 env 处理（向后兼容）
"""
import io, re, sys, shutil, datetime

STORE = "/opt/wgn/lib/store.mjs"
INDEX = "/opt/wgn/index.mjs"
ts = datetime.datetime.now().strftime("%Y%m%d%H%M%S")

def bak(p):
    shutil.copy2(p, p + ".bak-kernel-" + ts)

# ============ store.mjs ============
s = io.open(STORE, encoding="utf-8").read()
n0 = len(s)

# 1) createCard 签名加 kind
a = "export async function createCard({ plan = 'monthly', days, minutes, devices = 1, note = '', key: customKey } = {}) {"
b = "export async function createCard({ plan = 'monthly', days, minutes, devices = 1, note = '', key: customKey, kind = 'env' } = {}) {"
assert a in s, "createCard 签名锚点未找到"
s = s.replace(a, b, 1)

# 2) card 对象加 kind（写在 plan 后面）
a = """  const card = {
    id: randomId('card'),
    keyHash: hash,
    plan,"""
b = """  const card = {
    id: randomId('card'),
    keyHash: hash,
    kind: (kind === 'kernel' ? 'kernel' : 'env'),
    plan,"""
assert a in s, "card 对象锚点未找到"
s = s.replace(a, b, 1)

# 3) activateCard：内核卡不许用于环境
a = """  if (index < 0) throw new StoreError('CARD_NOT_FOUND', '卡密不存在或格式错误', 404);
  const card = cards[index];
  // Fresh cards start their clock on first activation (important for 30-min
  // trial cards and hourly cards)."""
b = """  if (index < 0) throw new StoreError('CARD_NOT_FOUND', '卡密不存在或格式错误', 404);
  const card = cards[index];
  // 内核卡只能用于内核激活，不能拿来当环境卡
  if (card.kind === 'kernel') {
    throw new StoreError('CARD_KIND_MISMATCH', '这是内核卡密，只能用于内核板块，不能激活环境', 403);
  }
  // Fresh cards start their clock on first activation (important for 30-min
  // trial cards and hourly cards)."""
assert a in s, "activateCard 锚点未找到"
s = s.replace(a, b, 1)

# 4) activateKernelCard：只有内核卡能激活内核
a = """export async function activateKernelCard(cardKey, hwid) {
  const keys = await loadKeys();
  const cards = await readCards();
  const hash = cardKeyHash(cardKey, keys);
  const index = cards.findIndex((card) => card.keyHash === hash);
  if (index < 0) throw new StoreError('CARD_NOT_FOUND', '卡密不存在或格式错误', 404);
  const card = cards[index];"""
b = """export async function activateKernelCard(cardKey, hwid) {
  const keys = await loadKeys();
  const cards = await readCards();
  const hash = cardKeyHash(cardKey, keys);
  const index = cards.findIndex((card) => card.keyHash === hash);
  if (index < 0) throw new StoreError('CARD_NOT_FOUND', '卡密不存在或格式错误', 404);
  const card = cards[index];
  // 环境卡不能激活内核：内核必须用单独生成的内核卡密
  if ((card.kind || 'env') !== 'kernel') {
    throw new StoreError('CARD_KIND_MISMATCH', '这是环境卡密，不能用于内核，请使用内核卡密', 403);
  }"""
assert a in s, "activateKernelCard 锚点未找到"
s = s.replace(a, b, 1)

bak(STORE)
io.open(STORE, "w", encoding="utf-8", newline="\n").write(s)
print("store.mjs 已改：%d -> %d 字节" % (n0, len(s)))

# ============ index.mjs ============
t = io.open(INDEX, encoding="utf-8").read()
m0 = len(t)

# 5) 单张创建传 kind
a = """        const result = await createCard({
          plan: body.plan || 'monthly',
          days: body.days,
          minutes: body.minutes,
          devices: body.devices,
          note: body.note,
          key: body.key,
        });"""
b = """        const result = await createCard({
          plan: body.plan || 'monthly',
          days: body.days,
          minutes: body.minutes,
          devices: body.devices,
          note: body.note,
          key: body.key,
          kind: body.kind,
        });"""
assert a in t, "admin/cards 单张锚点未找到"
t = t.replace(a, b, 1)

# 6) 批量创建传 kind
a = """          const result = await createCard({
            plan: body.plan || 'monthly',
            days: body.days,
            minutes: body.minutes,
            devices: body.devices,
            note: body.note,
          });"""
b = """          const result = await createCard({
            plan: body.plan || 'monthly',
            days: body.days,
            minutes: body.minutes,
            devices: body.devices,
            note: body.note,
            kind: body.kind,
          });"""
assert a in t, "admin/cards 批量锚点未找到"
t = t.replace(a, b, 1)

bak(INDEX)
io.open(INDEX, "w", encoding="utf-8", newline="\n").write(t)
print("index.mjs 已改：%d -> %d 字节" % (m0, len(t)))
print("OK")
