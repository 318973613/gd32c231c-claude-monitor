"""
Claude 图标绘制器
使用 matplotlib 绘制 Claude 风格的图标
"""

import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np

def draw_claude_icon():
    """绘制 Claude 图标"""
    # 创建画布
    fig, ax = plt.subplots(1, 1, figsize=(8, 8), facecolor='#1a1a1a')
    ax.set_facecolor('#1a1a1a')
    ax.set_xlim(-1.5, 1.5)
    ax.set_ylim(-1.5, 1.5)
    ax.set_aspect('equal')
    ax.axis('off')

    # Claude 的主色调 - 橙棕色
    claude_color = '#D4A574'
    claude_dark = '#C4956A'
    claude_light = '#E8C4A0'

    # 绘制中心圆（代表 Claude 的"头部"）
    circle = plt.Circle((0, 0), 0.8, color=claude_color, fill=True, linewidth=3)
    ax.add_patch(circle)

    # 绘制 Claude 标志性的射线图案
    num_rays = 12
    angles = np.linspace(0, 2 * np.pi, num_rays, endpoint=False)

    for i, angle in enumerate(angles):
        # 内圆起点
        inner_r = 0.85
        # 外圆终点（交替长短）
        outer_r = 1.25 if i % 2 == 0 else 1.1

        # 计算射线的起点和终点
        x_start = inner_r * np.cos(angle)
        y_start = inner_r * np.sin(angle)
        x_end = outer_r * np.cos(angle)
        y_end = outer_r * np.sin(angle)

        # 绘制射线
        linewidth = 6 if i % 2 == 0 else 4
        ax.plot([x_start, x_end], [y_start, y_end],
                color=claude_color, linewidth=linewidth, solid_capstyle='round')

    # 在中心绘制一个微笑的脸
    # 眼睛
    eye_y = 0.15
    eye_r = 0.08
    left_eye = plt.Circle((-0.2, eye_y), eye_r, color='#1a1a1a', fill=True)
    right_eye = plt.Circle((0.2, eye_y), eye_r, color='#1a1a1a', fill=True)
    ax.add_patch(left_eye)
    ax.add_patch(right_eye)

    # 微笑
    theta = np.linspace(np.pi * 0.2, np.pi * 0.8, 50)
    smile_x = 0.3 * np.cos(theta)
    smile_y = -0.15 + 0.2 * np.sin(theta) - 0.1
    ax.plot(smile_x, smile_y, color='#1a1a1a', linewidth=3, solid_capstyle='round')

    # 添加文字
    ax.text(0, -1.35, 'Claude', fontsize=24, fontweight='bold',
            color=claude_color, ha='center', va='center',
            fontfamily='sans-serif')

    ax.text(0, -1.55, 'by Anthropic', fontsize=12,
            color='#888888', ha='center', va='center',
            fontfamily='sans-serif')

    plt.tight_layout()
    plt.savefig('claude_icon.png', dpi=150, bbox_inches='tight',
                facecolor='#1a1a1a', edgecolor='none')
    plt.show()
    print("✅ Claude 图标已保存为 claude_icon.png")

if __name__ == '__main__':
    draw_claude_icon()
