#!/usr/bin/env python3
#
# make-icons.py - generates the PNG icons used by the Add-On Store and the bundled add-ons
#
# usage: python3 addons/tools/make-icons.py   (requires Pillow)
#
# All icons are drawn at a high resolution and downsampled for smooth edges. They consist of
# a rounded, gradient filled tile with a white glyph which works on light and dark themes.

import math
import os

from PIL import Image, ImageDraw

SCALE = 4
SIZE = 128
S = SIZE * SCALE

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

WHITE = (255, 255, 255, 255)


def tile(color_top, color_bottom):
	img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
	gradient = Image.new("RGBA", (S, S))
	gd = ImageDraw.Draw(gradient)
	for y in range(S):
		t = y / (S - 1)
		c = tuple(int(color_top[i] + (color_bottom[i] - color_top[i]) * t) for i in range(3)) + (255,)
		gd.line([(0, y), (S, y)], fill=c)
	mask = Image.new("L", (S, S), 0)
	md = ImageDraw.Draw(mask)
	margin = int(S * 0.04)
	md.rounded_rectangle([margin, margin, S - margin, S - margin], radius=int(S * 0.22), fill=255)
	img.paste(gradient, (0, 0), mask)
	return img


def p(v):
	"""convert relative coordinate (0..1) to pixels"""
	return int(v * S)


def width(v):
	return max(1, int(v * S))


def save(img, path):
	out = img.resize((SIZE, SIZE), Image.LANCZOS)
	os.makedirs(os.path.dirname(path), exist_ok=True)
	out.save(path)
	print("wrote", os.path.relpath(path, ROOT))


def glyph_puzzle(d, color=WHITE):
	d.rounded_rectangle([p(0.27), p(0.36), p(0.67), p(0.76)], radius=p(0.05), fill=color)
	d.ellipse([p(0.40), p(0.24), p(0.54), p(0.38)], fill=color)
	d.ellipse([p(0.62), p(0.49), p(0.76), p(0.63)], fill=color)


def glyph_bag(d):
	d.rounded_rectangle([p(0.24), p(0.38), p(0.76), p(0.80)], radius=p(0.06), fill=WHITE)
	d.arc([p(0.36), p(0.20), p(0.64), p(0.52)], start=180, end=360, fill=WHITE, width=width(0.045))
	# small puzzle piece cut-out on the bag
	color = (25, 140, 179, 255)
	d.rounded_rectangle([p(0.42), p(0.52), p(0.58), p(0.68)], radius=p(0.02), fill=color)
	d.ellipse([p(0.46), p(0.47), p(0.54), p(0.55)], fill=color)
	d.ellipse([p(0.55), p(0.56), p(0.63), p(0.64)], fill=color)


def glyph_list_bars(d):
	for i, length in enumerate([0.52, 0.36, 0.46]):
		y = 0.30 + i * 0.15
		d.ellipse([p(0.22), p(y), p(0.30), p(y + 0.08)], fill=WHITE)
		d.rounded_rectangle([p(0.34), p(y + 0.005), p(0.34 + length), p(y + 0.075)], radius=p(0.035), fill=WHITE)
	# activity line
	pts = [(0.22, 0.80), (0.38, 0.80), (0.45, 0.70), (0.53, 0.88), (0.60, 0.76), (0.78, 0.76)]
	d.line([(p(x), p(y)) for x, y in pts], fill=WHITE, width=width(0.035), joint="curve")


def shield_points(cx, top, w, h):
	pts = []
	for i in range(0, 181, 6):
		a = math.radians(i)
		pts.append((cx - w / 2 * math.cos(a), top + h * 0.55 + h * 0.45 * math.sin(a)))
	return [(cx - w / 2, top + h * 0.12), (cx, top), (cx + w / 2, top + h * 0.12)] + list(reversed(pts))


def glyph_shield(d, mark="check", color_bg=(0, 0, 0, 0)):
	pts = shield_points(0.5, 0.20, 0.54, 0.62)
	d.polygon([(p(x), p(y)) for x, y in pts], fill=WHITE)
	if mark == "check":
		d.line([(p(0.38), p(0.52)), (p(0.47), p(0.62)), (p(0.64), p(0.42))], fill=color_bg, width=width(0.06), joint="curve")
	elif mark == "grid":
		for ix in range(2):
			for iy in range(2):
				x = 0.40 + ix * 0.12
				y = 0.40 + iy * 0.12
				d.rounded_rectangle([p(x), p(y), p(x + 0.09), p(y + 0.09)], radius=p(0.015), fill=color_bg)


def glyph_speaker(d):
	d.polygon([(p(0.20), p(0.40)), (p(0.32), p(0.40)), (p(0.48), p(0.26)), (p(0.48), p(0.74)),
			   (p(0.32), p(0.60)), (p(0.20), p(0.60))], fill=WHITE)


def glyph_volume(d):
	glyph_speaker(d)
	for r in (0.14, 0.24):
		d.arc([p(0.48 - r), p(0.5 - r), p(0.48 + r), p(0.5 + r)], start=-50, end=50, fill=WHITE, width=width(0.045))


def glyph_volume_mute(d):
	glyph_speaker(d)
	d.line([(p(0.58), p(0.38)), (p(0.80), p(0.62))], fill=WHITE, width=width(0.06))
	d.line([(p(0.58), p(0.62)), (p(0.80), p(0.38))], fill=WHITE, width=width(0.06))


def glyph_volume_lock(d, color_bg):
	glyph_speaker(d)
	d.arc([p(0.58), p(0.30), p(0.78), p(0.52)], start=180, end=360, fill=WHITE, width=width(0.04))
	d.rounded_rectangle([p(0.55), p(0.42), p(0.81), p(0.66)], radius=p(0.03), fill=WHITE)
	d.ellipse([p(0.655), p(0.50), p(0.705), p(0.55)], fill=color_bg)


def glyph_folder(d):
	d.rounded_rectangle([p(0.18), p(0.28), p(0.46), p(0.40)], radius=p(0.03), fill=WHITE)
	d.rounded_rectangle([p(0.18), p(0.34), p(0.82), p(0.76)], radius=p(0.05), fill=WHITE)


def glyph_folder_arrows(d, color_bg):
	glyph_folder(d)
	# up/down arrows indicating file exchange
	d.polygon([(p(0.40), p(0.44)), (p(0.48), p(0.54)), (p(0.43), p(0.54)), (p(0.43), p(0.68)),
			   (p(0.37), p(0.68)), (p(0.37), p(0.54)), (p(0.32), p(0.54))], fill=color_bg)
	d.polygon([(p(0.60), p(0.70)), (p(0.68), p(0.60)), (p(0.63), p(0.60)), (p(0.63), p(0.46)),
			   (p(0.57), p(0.46)), (p(0.57), p(0.60)), (p(0.52), p(0.60))], fill=color_bg)


def glyph_globe(d, box=(0.22, 0.22, 0.78, 0.78), line=0.035):
	x0, y0, x1, y1 = box
	w = width(line)
	d.ellipse([p(x0), p(y0), p(x1), p(y1)], outline=WHITE, width=w)
	cx = (x0 + x1) / 2
	rx = (x1 - x0) / 2
	d.ellipse([p(cx - rx * 0.45), p(y0), p(cx + rx * 0.45), p(y1)], outline=WHITE, width=w)
	d.line([(p(cx), p(y0)), (p(cx), p(y1))], fill=WHITE, width=w)
	cy = (y0 + y1) / 2
	d.line([(p(x0), p(cy)), (p(x1), p(cy))], fill=WHITE, width=w)
	ry = (y1 - y0) / 2
	for f in (-0.55, 0.55):
		yy = cy + ry * f
		dx = rx * math.sqrt(1 - f * f)
		d.line([(p(cx - dx), p(yy)), (p(cx + dx), p(yy))], fill=WHITE, width=w)


def glyph_globe_block(d, color_bg):
	glyph_globe(d)
	d.line([(p(0.24), p(0.76)), (p(0.76), p(0.24))], fill=color_bg, width=width(0.11))
	d.line([(p(0.24), p(0.76)), (p(0.76), p(0.24))], fill=WHITE, width=width(0.05))


def glyph_browser(d, color_bg):
	d.rounded_rectangle([p(0.18), p(0.24), p(0.82), p(0.76)], radius=p(0.05), outline=WHITE, width=width(0.04))
	d.rectangle([p(0.18), p(0.24), p(0.82), p(0.36)], fill=WHITE)
	d.rounded_rectangle([p(0.18), p(0.24), p(0.82), p(0.38)], radius=p(0.05), fill=WHITE)
	for i, x in enumerate((0.24, 0.30, 0.36)):
		d.ellipse([p(x), p(0.285), p(x + 0.035), p(0.32)], fill=color_bg)


def glyph_website_filter(d, color_bg):
	glyph_browser(d, color_bg)
	# funnel
	d.polygon([(p(0.34), p(0.44)), (p(0.66), p(0.44)), (p(0.54), p(0.57)), (p(0.54), p(0.70)),
			   (p(0.46), p(0.66)), (p(0.46), p(0.57))], fill=WHITE)


def glyph_webpage_send(d, color_bg):
	glyph_browser(d, color_bg)
	d.line([(p(0.36), p(0.66)), (p(0.62), p(0.44))], fill=WHITE, width=width(0.05))
	d.polygon([(p(0.66), p(0.40)), (p(0.66), p(0.56)), (p(0.50), p(0.42))], fill=WHITE)


def glyph_file_send(d, color_bg):
	d.polygon([(p(0.26), p(0.18)), (p(0.56), p(0.18)), (p(0.72), p(0.34)), (p(0.72), p(0.82)), (p(0.26), p(0.82))], fill=WHITE)
	d.polygon([(p(0.56), p(0.18)), (p(0.56), p(0.34)), (p(0.72), p(0.34))], fill=(220, 230, 240, 255))
	d.line([(p(0.49), p(0.72)), (p(0.49), p(0.44))], fill=color_bg, width=width(0.06))
	d.polygon([(p(0.37), p(0.50)), (p(0.49), p(0.36)), (p(0.61), p(0.50))], fill=color_bg)


def glyph_terminal(d, color_bg):
	d.rounded_rectangle([p(0.16), p(0.24), p(0.84), p(0.76)], radius=p(0.06), fill=WHITE)
	d.line([(p(0.28), p(0.40)), (p(0.40), p(0.50)), (p(0.28), p(0.60))], fill=color_bg, width=width(0.05), joint="curve")
	d.line([(p(0.46), p(0.62)), (p(0.66), p(0.62))], fill=color_bg, width=width(0.05))


def make(path, colors, painter):
	img = tile(*colors)
	d = ImageDraw.Draw(img)
	painter(d)
	save(img, os.path.join(ROOT, path))


BLUE = ((54, 176, 214), (25, 120, 168))
TEAL = ((47, 190, 160), (22, 128, 118))
ORANGE = ((255, 167, 61), (230, 102, 26))
PURPLE = ((150, 110, 230), (98, 64, 186))
RED = ((240, 98, 98), (190, 44, 58))
GREEN = ((108, 196, 92), (52, 140, 60))
SLATE = ((96, 116, 140), (54, 68, 90))
PINK = ((236, 104, 168), (186, 52, 120))
INDIGO = ((88, 132, 236), (52, 82, 196))


def bg(colors):
	return tuple(colors[1]) + (255,)


icons = [
	("core/resources/addon-store.png", BLUE, glyph_bag),
	("core/resources/addon-generic.png", SLATE, glyph_puzzle),
	("core/resources/addon-script.png", PURPLE, lambda d: glyph_terminal(d, bg(PURPLE))),
	("addons/taskmanager/resources/taskmanager.png", TEAL, glyph_list_bars),
	("addons/taskmanager/resources/application-control.png", TEAL, lambda d: glyph_shield(d, "grid", bg(TEAL))),
	("addons/volumecontrol/resources/volume.png", ORANGE, glyph_volume),
	("addons/volumecontrol/resources/volume-mute.png", ORANGE, glyph_volume_mute),
	("addons/volumecontrol/resources/volume-lock.png", ORANGE, lambda d: glyph_volume_lock(d, bg(ORANGE))),
	("addons/filemanager/resources/filemanager.png", INDIGO, lambda d: glyph_folder_arrows(d, bg(INDIGO))),
	("addons/internetaccess/resources/internet-access.png", GREEN, glyph_globe),
	("addons/internetaccess/resources/internet-block.png", RED, lambda d: glyph_globe_block(d, (215, 70, 76, 255))),
	("addons/websitefilter/resources/website-filter.png", PINK, lambda d: glyph_website_filter(d, bg(PINK))),
	("addons/webpagesender/resources/webpage-send.png", BLUE, lambda d: glyph_webpage_send(d, bg(BLUE))),
	("addons/filesender/resources/file-send.png", INDIGO, lambda d: glyph_file_send(d, bg(INDIGO))),
	("addons/scriptedaddons/resources/scripted-addons.png", PURPLE, lambda d: glyph_terminal(d, bg(PURPLE))),
]

for path, colors, painter in icons:
	make(path, colors, painter)
