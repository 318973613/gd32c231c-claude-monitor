#!/usr/bin/env python3
"""
文件目录分析工具
扫描指定目录，生成详细的文件统计报告
"""

import os
import sys
import io
from pathlib import Path
from collections import defaultdict
import time

# 修复 Windows 终端编码问题
if sys.stdout.encoding != 'utf-8':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')


def format_size(size_bytes):
    """将字节大小转换为人类可读格式"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.2f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.2f} TB"


def scan_directory(root_path):
    """扫描目录并收集统计信息"""
    stats = {
        'total_files': 0,
        'total_dirs': 0,
        'total_size': 0,
        'file_types': defaultdict(lambda: {'count': 0, 'size': 0}),
        'largest_files': [],
        'newest_files': [],
        'oldest_files': [],
    }

    root = Path(root_path)
    if not root.exists():
        print(f"错误: 路径 '{root_path}' 不存在")
        return None

    print(f"正在扫描目录: {root_path}")
    start_time = time.time()

    try:
        for item in root.rglob('*'):
            if item.is_file():
                try:
                    size = item.stat().st_size
                    mtime = item.stat().st_mtime
                    ext = item.suffix.lower() or '(无扩展名)'

                    stats['total_files'] += 1
                    stats['total_size'] += size
                    stats['file_types'][ext]['count'] += 1
                    stats['file_types'][ext]['size'] += size

                    # 记录最大的10个文件
                    stats['largest_files'].append((size, item.name, str(item)))
                    stats['largest_files'].sort(reverse=True)
                    stats['largest_files'] = stats['largest_files'][:10]

                    # 记录最新的10个文件
                    stats['newest_files'].append((mtime, item.name, str(item)))
                    stats['newest_files'].sort(reverse=True)
                    stats['newest_files'] = stats['newest_files'][:10]

                    # 记录最旧的10个文件
                    stats['oldest_files'].append((mtime, item.name, str(item)))
                    stats['oldest_files'].sort()
                    stats['oldest_files'] = stats['oldest_files'][:10]

                except (PermissionError, OSError):
                    continue

            elif item.is_dir():
                stats['total_dirs'] += 1

    except KeyboardInterrupt:
        print("\n扫描被中断")

    elapsed = time.time() - start_time
    stats['scan_time'] = elapsed
    stats['root_path'] = str(root)

    return stats


def print_report(stats):
    """打印统计报告"""
    if not stats:
        return

    print("\n" + "=" * 60)
    print("📊 文件目录分析报告")
    print("=" * 60)

    print(f"\n📁 扫描路径: {stats['root_path']}")
    print(f"⏱️  扫描耗时: {stats['scan_time']:.2f} 秒")

    print(f"\n📈 总体统计:")
    print(f"   文件总数: {stats['total_files']:,}")
    print(f"   目录总数: {stats['total_dirs']:,}")
    print(f"   总大小:   {format_size(stats['total_size'])}")

    # 文件类型统计
    print(f"\n📋 文件类型分布 (按大小排序):")
    sorted_types = sorted(stats['file_types'].items(),
                         key=lambda x: x[1]['size'], reverse=True)

    for ext, data in sorted_types[:15]:  # 只显示前15种
        percentage = (data['size'] / stats['total_size'] * 100) if stats['total_size'] > 0 else 0
        bar = '█' * int(percentage / 2)
        print(f"   {ext:15} │ {data['count']:6,} 个 │ {format_size(data['size']):>10} │ {bar} {percentage:.1f}%")

    # 最大的文件
    print(f"\n📦 最大的 10 个文件:")
    for i, (size, name, path) in enumerate(stats['largest_files'], 1):
        print(f"   {i:2}. {format_size(size):>10} │ {name}")

    # 最新的文件
    print(f"\n🆕 最新的 10 个文件:")
    for i, (mtime, name, path) in enumerate(stats['newest_files'], 1):
        date_str = time.strftime('%Y-%m-%d %H:%M', time.localtime(mtime))
        print(f"   {i:2}. {date_str} │ {name}")

    # 最旧的文件
    print(f"\n📜 最旧的 10 个文件:")
    for i, (mtime, name, path) in enumerate(stats['oldest_files'], 1):
        date_str = time.strftime('%Y-%m-%d %H:%M', time.localtime(mtime))
        print(f"   {i:2}. {date_str} │ {name}")

    print("\n" + "=" * 60)


def main():
    """主函数"""
    if len(sys.argv) > 1:
        target_path = sys.argv[1]
    else:
        target_path = "."

    print("🔍 文件目录分析工具 v1.0")
    print("-" * 40)

    stats = scan_directory(target_path)
    if stats:
        print_report(stats)

        # 询问是否保存报告
        try:
            save = input("\n💾 是否保存报告到文件? (y/n): ").strip().lower()
            if save == 'y':
                report_file = f"file_report_{int(time.time())}.txt"
                with open(report_file, 'w', encoding='utf-8') as f:
                    old_stdout = sys.stdout
                    sys.stdout = f
                    print_report(stats)
                    sys.stdout = old_stdout
                print(f"✅ 报告已保存到: {report_file}")
        except (KeyboardInterrupt, EOFError):
            print("\n👋 再见!")


if __name__ == "__main__":
    main()