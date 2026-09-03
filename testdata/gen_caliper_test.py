import cv2
import numpy as np

imw, imh = 1280, 960

# 暗背景 + 一条竖直亮线（线宽约 6 像素，位置略偏左，非正中，便于验证拟合位置）
img = np.full((imh, imw), 40, np.uint8)
line_x = 600
cv2.line(img, (line_x, 0), (line_x, imh - 1), 230, thickness=6)

cv2.imwrite(r'd:\MyCode\VisionPlatform\testdata\caliper_test_vertical_line.png', img)
print('saved caliper_test_vertical_line.png  vertical bright line at x =', line_x)

# 辅助：一条水平亮线（用于后续验证水平方向卡尺）
img2 = np.full((imh, imw), 40, np.uint8)
line_y = 480
cv2.line(img2, (0, line_y), (imw - 1, line_y), 230, thickness=6)
cv2.imwrite(r'd:\MyCode\VisionPlatform\testdata\caliper_test_horizontal_line.png', img2)
print('saved caliper_test_horizontal_line.png  horizontal bright line at y =', line_y)
