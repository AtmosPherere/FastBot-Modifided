#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Fastbot测试结果活动覆盖率分析脚本（绝对路径版）
- 扫描 /Users/atmo/program/Fastbot_Android_副本 下在区间 [test_results_20250718_212053, test_results_20250819_005359] 的所有 test_results_* 目录
- 每个目录合并 round1_coverage.txt 与 round2_coverage.txt（去重）
- 原版三次测试目录使用绝对路径：test_results_original 下三组
- 输出Venn图与CSV到 /Users/atmo/program/Fastbot_Android_副本/venn_diagrams
- 内置中文字体设置，避免中文乱码
"""

import os
import sys

# 处理第三方库导入，并在缺少时给出清晰提示
try:
	import matplotlib
	import matplotlib.pyplot as plt
	from matplotlib_venn import venn2
except Exception as e:
	sys.stderr.write(
		"[依赖缺失] 需要 matplotlib 与 matplotlib-venn\n"
		"可一键安装：\n"
		"  python3 -m venv /Users/atmo/program/Fastbot_Android_副本/.venv\n"
		"  /Users/atmo/program/Fastbot_Android_副本/.venv/bin/pip install --upgrade pip matplotlib matplotlib-venn\n"
		"然后运行：\n"
		"  /Users/atmo/program/Fastbot_Android_副本/.venv/bin/python /Users/atmo/program/Fastbot_Android_副本/generate_venn_diagrams.py\n"
	)
	raise

# 设置中文字体，避免乱码
matplotlib.rcParams['font.sans-serif'] = [
	'PingFang SC', 'Hiragino Sans GB', 'Heiti TC', 'Songti SC',
	'SimHei', 'Arial Unicode MS', 'DejaVu Sans'
]
matplotlib.rcParams['axes.unicode_minus'] = False

ROOT = "/Users/atmo/program/Fastbot_Android_副本"
ORIGINAL_ROOT = os.path.join(ROOT, "test_results_original")
OUTPUT_DIR = os.path.join(ROOT, "venn_diagrams")
RANGE_START = "20250718_212053"
RANGE_END = "20250819_005359"

ORIGINAL_DIRS = [
	os.path.join(ORIGINAL_ROOT, "test_results_20250817_171329"),
	os.path.join(ORIGINAL_ROOT, "test_results_20250817_011722"),
	os.path.join(ORIGINAL_ROOT, "test_results_20250816_230536"),
]

# 回退候选：常见的改进版目录（存在则纳入）
FALLBACK_IMPROVED_DIRS = [
	os.path.join(ROOT, "test_results_20250718_212053"),
	os.path.join(ROOT, "test_results_20250816_150125"),
	os.path.join(ROOT, "test_results_20250817_193538"),
	os.path.join(ROOT, "test_results_20250817_220939"),
	os.path.join(ROOT, "test_results_20250818_224913"),
	os.path.join(ROOT, "test_results_20250819_005359"),
	os.path.join(ROOT, "test_results_20250819_184356"),
]

def warn(msg: str):
	print(f"[WARN] {msg}")

def read_activities(file_path: str) -> set:
	activities = set()
	if os.path.exists(file_path):
		with open(file_path, "r", encoding="utf-8") as f:
			for line in f:
				s = line.strip()
				if s:
					activities.add(s)
	else:
		warn(f"缺少文件: {file_path}")
	return activities

def merge_round_coverage(test_dir: str):
	r1p = os.path.join(test_dir, "round1_coverage.txt")
	r2p = os.path.join(test_dir, "round2_coverage.txt")
	r1 = read_activities(r1p)
	r2 = read_activities(r2p)
	merged = r1 | r2
	return {
		"round1": r1,
		"round2": r2,
		"merged": merged,
		"round1_count": len(r1),
		"round2_count": len(r2),
		"merged_count": len(merged),
	}

def list_improved_dirs_in_range() -> list:
	dirs = []
	print(f"扫描根目录: {ROOT}")
	try:
		items = sorted(os.listdir(ROOT))
	except Exception as e:
		warn(f"无法列出目录: {e}")
		items = []
	for name in items:
		if not name.startswith("test_results_"):
			continue
		full = os.path.join(ROOT, name)
		if not os.path.isdir(full):
			warn(f"跳过（非目录）: {name}")
			continue
		# 名称形如 test_results_YYYYMMDD_HHMMSS
		prefix = "test_results_"
		ts = name[len(prefix):] if len(name) > len(prefix) else ""
		if not ts:
			warn(f"跳过（无法解析时间戳）: {name}")
			continue
		if not (RANGE_START <= ts <= RANGE_END):
			# 不在时间范围
			continue
		# 需要两轮覆盖文件齐全
		r1 = os.path.join(full, "round1_coverage.txt")
		r2 = os.path.join(full, "round2_coverage.txt")
		if not os.path.exists(r1) or not os.path.exists(r2):
			warn(f"跳过（覆盖文件缺失）: {name} r1={os.path.exists(r1)} r2={os.path.exists(r2)}")
			continue
		dirs.append(full)
	print("=== 改进版测试目录（时间范围内）===")
	for d in dirs:
		print(" -", os.path.basename(d))
	# 若为空，尝试回退列表
	if not dirs:
		print("未匹配到目录，尝试回退候选...")
		for cand in FALLBACK_IMPROVED_DIRS:
			name = os.path.basename(cand)
			if os.path.isdir(cand):
				# 同样要求两轮覆盖文件
				r1 = os.path.join(cand, "round1_coverage.txt")
				r2 = os.path.join(cand, "round2_coverage.txt")
				if os.path.exists(r1) and os.path.exists(r2):
					print(" + 回退加入:", name)
					dirs.append(cand)
				else:
					warn(f"回退候选缺文件: {name}")
	return dirs

def analyze():
	os.makedirs(OUTPUT_DIR, exist_ok=True)

	results = {}

	# 原版三次
	print("\n=== 读取原版三次测试 ===")
	original_union = set()
	for d in ORIGINAL_DIRS:
		if not os.path.isdir(d):
			warn(f"原版目录不存在: {d}")
			continue
		key = f"original::{os.path.basename(d)}"
		res = merge_round_coverage(d)
		print(f" {os.path.basename(d)} -> R1:{res['round1_count']} R2:{res['round2_count']} 合并:{res['merged_count']}")
		results[key] = res
		original_union |= res["merged"]

	# 改进版（按时间范围）
	print("\n=== 读取改进版（时间范围内）===")
	improved_dirs = list_improved_dirs_in_range()
	improved_union = set()
	if not improved_dirs:
		warn("未发现任何符合时间范围的改进版目录")
	for d in improved_dirs:
		key = f"improved::{os.path.basename(d)}"
		res = merge_round_coverage(d)
		print(f" {os.path.basename(d)} -> R1:{res['round1_count']} R2:{res['round2_count']} 合并:{res['merged_count']}")
		results[key] = res
		improved_union |= res["merged"]

	print("\n=== 汇总 ===")
	print(f" 原版合并后去重总活动数: {len(original_union)}")
	print(f" 改进版合并后去重总活动数: {len(improved_union)}")
	print(f" 共同活动: {len(original_union & improved_union)}  |  仅原版: {len(original_union - improved_union)}  |  仅改进版: {len(improved_union - original_union)}")

	return results, original_union, improved_union

def save_csv(results: dict, original_union: set, improved_union: set):
	csv_path = os.path.join(OUTPUT_DIR, "activity_analysis.csv")
	common = original_union & improved_union
	only_orig = original_union - improved_union
	only_impr = improved_union - original_union

	with open(csv_path, "w", encoding="utf-8") as f:
		f.write("类型,名称,round1,round2,合并\n")
		for key, res in sorted(results.items()):
			typ, name = key.split("::", 1)
			f.write(f"{typ},{name},{res['round1_count']},{res['round2_count']},{res['merged_count']}\n")
		f.write("\n总体\n")
		f.write(f"原版总活动数,{len(original_union)}\n")
		f.write(f"改进版总活动数,{len(improved_union)}\n")
		f.write(f"共同活动数,{len(common)}\n")
		f.write(f"仅原版活动数,{len(only_orig)}\n")
		f.write(f"仅改进版活动数,{len(only_impr)}\n")
	print("CSV:", csv_path)

def plot_overall(original_union: set, improved_union: set):
	plt.figure(figsize=(8, 6))
	venn2([original_union, improved_union], set_labels=("原版Fastbot", "改进版Fastbot"), set_colors=("#ff9999", "#99ccff"))
	plt.title("原版 vs 改进版（合并去重）")
	out = os.path.join(OUTPUT_DIR, "overall_comparison.png")
	plt.tight_layout()
	plt.savefig(out, dpi=300, bbox_inches='tight')
	plt.close()
	print("图:", out)

def plot_best_improved_pairs(results: dict):
	# 选取改进版中合并覆盖最多的前2个进行对比
	improved_items = [(k, v) for k, v in results.items() if k.startswith("improved::")]
	improved_items.sort(key=lambda kv: kv[1]['merged_count'], reverse=True)
	if len(improved_items) < 2:
		return
	(k1, r1), (k2, r2) = improved_items[0], improved_items[1]
	plt.figure(figsize=(8, 6))
	venn2([r1['merged'], r2['merged']], set_labels=(k1.split('::',1)[1], k2.split('::',1)[1]), set_colors=("#99ccff", "#a3c1da"))
	plt.title("改进版两次最佳覆盖对比")
	out = os.path.join(OUTPUT_DIR, "improved_best_pair.png")
	plt.tight_layout()
	plt.savefig(out, dpi=300, bbox_inches='tight')
	plt.close()
	print("图:", out)

def plot_two_original(results: dict):
	# 取原版前两个目录作对比
	original_items = [(k, v) for k, v in results.items() if k.startswith("original::")]
	if len(original_items) < 2:
		return
	(k1, r1), (k2, r2) = original_items[0], original_items[1]
	plt.figure(figsize=(8, 6))
	venn2([r1['merged'], r2['merged']], set_labels=(k1.split('::',1)[1], k2.split('::',1)[1]), set_colors=("#ffb3b3", "#b3ffb3"))
	plt.title("原版两次对比")
	out = os.path.join(OUTPUT_DIR, "original_two_compare.png")
	plt.tight_layout()
	plt.savefig(out, dpi=300, bbox_inches='tight')
	plt.close()
	print("图:", out)

# 新增：原版×改进版所有两两组合（生成N*M张图）

def _sanitize(name: str) -> str:
	return ''.join(c if (c.isalnum() or c in ('-', '_', '.')) else '_' for c in name)

def plot_all_cross_pairs(results: dict):
	original_items = [(k, v) for k, v in results.items() if k.startswith("original::")]
	improved_items = [(k, v) for k, v in results.items() if k.startswith("improved::")]
	if not original_items or not improved_items:
		warn("原版或改进版目录为空，跳过交叉绘图")
		return
	print(f"\n=== 原版×改进版 交叉组合绘图（{len(original_items)}×{len(improved_items)}）===")
	count = 0
	for ok, orv in original_items:
		for ik, irv in improved_items:
			olabel = ok.split('::',1)[1]
			ilabel = ik.split('::',1)[1]
			plt.figure(figsize=(8, 6))
			venn2([orv['merged'], irv['merged']], set_labels=(olabel, ilabel), set_colors=("#ffcccc", "#cce5ff"))
			title = f"原版 {olabel} vs 改进版 {ilabel}"
			plt.title(title)
			fname = f"pair_original_{_sanitize(olabel)}__improved_{_sanitize(ilabel)}.png"
			out = os.path.join(OUTPUT_DIR, fname)
			plt.tight_layout()
			plt.savefig(out, dpi=300, bbox_inches='tight')
			plt.close()
			count += 1
			print("图:", out)
	print(f"共生成 {count} 张交叉Venn图")

def main():
	results, original_union, improved_union = analyze()
	save_csv(results, original_union, improved_union)
	plot_overall(original_union, improved_union)
	plot_best_improved_pairs(results)
	plot_two_original(results)
	# 新增：生成原版×改进版所有组合图
	plot_all_cross_pairs(results)
	print("输出目录:", OUTPUT_DIR)

if __name__ == "__main__":
	main() 