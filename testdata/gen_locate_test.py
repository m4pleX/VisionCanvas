import cv2
import numpy as np


def make_bar(cx, cy, w, h, angle_deg, bright=230, bg=40, imw=1280, imh=960):
    img = np.full((imh, imw), bg, np.uint8)
    rect = ((cx, cy), (w, h), angle_deg)
    pts = cv2.boxPoints(rect)
    pts = np.int32(pts)
    cv2.fillConvexPoly(img, pts, bright)
    return img


# 主图：亮色倾斜长条，明显长轴，验证中心 + 角度
img1 = make_bar(640, 480, 300, 180, -30)
cv2.imwrite(r'd:\MyCode\VisionPlatform\testdata\locate_test_bright_bar.png', img1)
print('saved locate_test_bright_bar.png  center=(640,480) size=300x180 angle=-30deg')

# 辅助图：暗目标 + 亮背景（验证 invert/阈值边界，供后续调参用）
img2 = make_bar(640, 480, 300, 180, 20, bright=40, bg=220)
cv2.imwrite(r'd:\MyCode\VisionPlatform\testdata\locate_test_dark_bar.png', img2)
print('saved locate_test_dark_bar.png   center=(640,480) size=300x180 angle=+20deg (dark target)')
