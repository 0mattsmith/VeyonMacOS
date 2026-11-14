#!/usr/bin/env python3
#
# make-icons.py - generates the icon sets of Veyon: the icons of the Add-On Store, the bundled
#                 add-ons, the Veyon features (Master toolbar and menus, Master panels, remote
#                 access window) and the Configurator pages
#
# usage: python3 addons/tools/make-icons.py                        (requires Pillow)
#        python3 addons/tools/make-icons.py --contact-sheet DIR
#        python3 addons/tools/make-icons.py --restore-classic
#
# Every icon exists in each icon set, which are shipped as freedesktop.org icon themes inside the
# Qt resources (:/icons/veyon-<set>/index.theme and :/icons/veyon-<set>/scalable/<name>.png) and
# selected with QIcon::setThemeName() in VeyonCore::initUi() (setting "UI/IconTheme"):
#
#   modern            rounded, gradient filled colour tiles with white glyphs (default)
#   monochrome        the glyphs of "modern" as flat, dark silhouettes for light colour schemes
#   monochrome-dark   the same silhouettes in a light colour for dark colour schemes
#   classic           the original (KDE Breeze) icons of Veyon, restored from git history with
#                     --restore-classic, plus Breeze style icons for the add-ons drawn here
#
# The PNGs are written to <library>/icons/<set>/<name>.png. The script also writes the
# index.theme files (core/resources/icons/<set>/) and the icon theme sections of the .qrc files,
# so adding an icon to ICONS is all that is needed to ship it in every set.
#
# --contact-sheet writes previews of every icon in all sets at 16, 20, 32 and 48 px on light and
# dark backgrounds into DIR.

import argparse
import io
import math
import os
import re
import subprocess

from PIL import Image, ImageChops, ImageDraw, ImageFont


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


def make(colors, painter):
	img = tile(*colors)
	d = ImageDraw.Draw(img)
	painter(d)
	return img


# ---------------------------------------------------------------------------------------------
# Glyphs of the Veyon features and Configurator pages
#
# They are drawn into a coverage mask (ON = white, OFF = cut-out) which is laid over the tile,
# so cut-outs show the tile's gradient. Strokes are at least 6% of the tile wide and gaps at
# least 4% so the glyphs stay legible at 16-20 px. Coordinates are relative (0..1).
# ---------------------------------------------------------------------------------------------

ON = 255
OFF = 0


class Canvas:
	def __init__(self, scale=1.0, dx=0.0, dy=0.0):
		self.mask = Image.new("L", (S, S), 0)
		self.d = ImageDraw.Draw(self.mask)
		self.scale = scale
		self.dx = dx
		self.dy = dy

	def x(self, v):
		return p(0.5 + (v - 0.5) * self.scale + self.dx)

	def y(self, v):
		return p(0.5 + (v - 0.5) * self.scale + self.dy)

	def w(self, v):
		return max(1, int(v * self.scale * S))

	def box(self, x0, y0, x1, y1):
		return [self.x(x0), self.y(y0), self.x(x1), self.y(y1)]

	def points(self, pts):
		return [(self.x(px), self.y(py)) for px, py in pts]

	def rect(self, x0, y0, x1, y1, fill=ON):
		self.d.rectangle(self.box(x0, y0, x1, y1), fill=fill)

	def rrect(self, x0, y0, x1, y1, r=0.04, fill=ON):
		self.d.rounded_rectangle(self.box(x0, y0, x1, y1), radius=self.w(r), fill=fill)

	def ellipse(self, x0, y0, x1, y1, fill=ON):
		self.d.ellipse(self.box(x0, y0, x1, y1), fill=fill)

	def circle(self, cx, cy, r, fill=ON):
		self.ellipse(cx - r, cy - r, cx + r, cy + r, fill)

	def ring(self, x0, y0, x1, y1, w, fill=ON):
		"""ellipse outline centred on the given box"""
		h = w / 2
		self.d.ellipse(self.box(x0 - h, y0 - h, x1 + h, y1 + h), outline=fill, width=self.w(w))

	def poly(self, pts, fill=ON):
		self.d.polygon(self.points(pts), fill=fill)

	def line(self, pts, w, fill=ON, caps=True):
		self.d.line(self.points(pts), fill=fill, width=self.w(w), joint="curve")
		if caps:
			# round caps and corners
			for px, py in pts:
				self.circle(px, py, w / 2, fill)

	def arc(self, cx, cy, r, start, end, w, fill=ON, caps=True):
		"""arc centred on radius r, angles in degrees clockwise from 3 o'clock"""
		o = r + w / 2
		self.d.arc(self.box(cx - o, cy - o, cx + o, cy + o), start=start, end=end, fill=fill, width=self.w(w))
		if caps:
			for a in (start, end):
				self.circle(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a)), w / 2, fill)

	def halo(self, pts, gap):
		"""clears a margin around a polygon so it can be drawn on top of other shapes"""
		self.poly(pts, OFF)
		self.line(pts + [pts[0]], 2 * gap, OFF)

	def layer(self, painter, angle=0, scale=1.0, dx=0.0, dy=0.0):
		"""draws a painter on its own layer (scaled, moved and rotated counter-clockwise by angle)"""
		sub = Canvas(scale, dx, dy)
		painter(sub)
		mask = sub.mask
		if angle:
			mask = mask.rotate(angle, resample=Image.BICUBIC, center=(p(0.5 + dx), p(0.5 + dy)))
		self.mask = ImageChops.lighter(self.mask, mask)
		self.d = ImageDraw.Draw(self.mask)


def make_glyph(colors, painter):
	img = tile(*colors)
	canvas = Canvas()
	painter(canvas)
	glyph = Image.new("RGBA", (S, S), WHITE)
	glyph.putalpha(canvas.mask)
	img.alpha_composite(glyph)
	return img


# building blocks

def monitor(c, x0=0.16, y0=0.20, x1=0.84, y1=0.64, frame=0.06, screen=True):
	"""monitor with stand; the screen is cut out unless screen is False"""
	c.rrect(x0, y0, x1, y1, 0.05)
	if screen:
		c.rrect(x0 + frame, y0 + frame, x1 - frame, y1 - frame, 0.015, OFF)
	cx = (x0 + x1) / 2
	w = x1 - x0
	c.rect(cx - w * 0.07, y1 - 0.01, cx + w * 0.07, y1 + 0.09)
	c.rrect(cx - w * 0.26, y1 + 0.07, cx + w * 0.26, y1 + 0.14, 0.03)


CURSOR = [(0, 0), (0, 0.80), (0.20, 0.62), (0.33, 0.90), (0.46, 0.84), (0.33, 0.57), (0.58, 0.55)]


def cursor(x, y, h):
	"""mouse pointer with its tip at x, y"""
	return [(x + px * h, y + py * h) for px, py in CURSOR]


def eye(c, cx, cy, a, h, fill=ON):
	"""almond shaped eye of half width a and height h"""
	r = (a * a + h * h / 4) / h
	upper = []
	for i in range(41):
		t = -a + 2 * a * i / 40
		upper.append((cx + t, cy - (math.sqrt(max(0.0, r * r - t * t)) - (r - h / 2))))
	c.poly(upper + [(x, 2 * cy - y) for x, y in reversed(upper)], fill)


def globe(c, cx=0.5, cy=0.5, r=0.30, w=0.06):
	c.ring(cx - r, cy - r, cx + r, cy + r, w)
	c.ring(cx - r * 0.42, cy - r, cx + r * 0.42, cy + r, w)
	c.line([(cx - r, cy), (cx + r, cy)], w, caps=False)


def page(c, x0, y0, x1, y1, fold=0.13):
	"""sheet of paper with a folded corner"""
	c.poly([(x0, y0), (x1 - fold, y0), (x1, y0 + fold), (x1, y1), (x0, y1)])
	c.line([(x1 - fold, y0), (x1 - fold, y0 + fold), (x1, y0 + fold)], 0.035, OFF, caps=False)


def play(c, cx, cy, h, fill=OFF):
	c.poly([(cx - h * 0.38, cy - h / 2), (cx - h * 0.38, cy + h / 2), (cx + h * 0.48, cy)], fill)


def rocket(c):
	c.ellipse(0.37, 0.10, 0.63, 0.74)
	c.rect(0.37, 0.42, 0.63, 0.70)
	c.poly([(0.37, 0.46), (0.21, 0.63), (0.21, 0.77), (0.37, 0.69)])
	c.poly([(0.63, 0.46), (0.79, 0.63), (0.79, 0.77), (0.63, 0.69)])
	c.circle(0.5, 0.38, 0.065, OFF)
	c.poly([(0.42, 0.75), (0.58, 0.75), (0.50, 0.91)])


def key(c):
	c.circle(0.27, 0.50, 0.17)
	c.circle(0.27, 0.50, 0.065, OFF)
	c.rrect(0.38, 0.455, 0.88, 0.545, 0.02)
	c.rect(0.70, 0.50, 0.77, 0.66)
	c.rect(0.80, 0.50, 0.87, 0.62)


def shield(c, cx=0.5, top=0.14, w=0.60, h=0.72):
	c.poly(shield_points(cx, top, w, h))


# feature glyphs

def glyph_screens(c):
	"""Monitoring: a wall of student screens"""
	for x in (0.17, 0.535):
		for y in (0.23, 0.535):
			c.rrect(x, y, x + 0.295, y + 0.235, 0.045)


def glyph_presentation(c):
	"""Demo: presentation board"""
	c.rrect(0.15, 0.16, 0.85, 0.61, 0.05)
	play(c, 0.51, 0.385, 0.22)
	c.line([(0.50, 0.60), (0.50, 0.70)], 0.07)
	c.line([(0.41, 0.60), (0.30, 0.83)], 0.07)
	c.line([(0.59, 0.60), (0.70, 0.83)], 0.07)


def glyph_screen_fullscreen(c):
	"""Demo in full screen mode: a filled screen with corner marks"""
	monitor(c, screen=False)
	for sx, sy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
		x = 0.5 + sx * 0.235
		y = 0.42 + sy * 0.105
		c.line([(x, y - sy * 0.075), (x, y), (x - sx * 0.10, y)], 0.055, OFF)


def glyph_screen_window(c):
	"""Demo in window mode: a window on a screen"""
	monitor(c)
	c.rrect(0.30, 0.32, 0.70, 0.52, 0.02)


def glyph_screens_overlapping(c):
	"""Demo server: a screen duplicated into a window"""
	c.rrect(0.32, 0.15, 0.87, 0.59, 0.05)
	c.rrect(0.385, 0.215, 0.805, 0.525, 0.015, OFF)
	c.rrect(0.09, 0.35, 0.69, 0.85, 0.08, OFF)
	c.rrect(0.13, 0.39, 0.65, 0.81, 0.05)
	play(c, 0.40, 0.60, 0.17)


def glyph_padlock(c):
	"""Lock"""
	c.arc(0.5, 0.40, 0.155, 180, 360, 0.085, caps=False)
	c.line([(0.345, 0.40), (0.345, 0.48)], 0.085, caps=False)
	c.line([(0.655, 0.40), (0.655, 0.48)], 0.085, caps=False)
	c.rrect(0.23, 0.45, 0.77, 0.83, 0.07)
	c.circle(0.5, 0.59, 0.06, OFF)
	c.rrect(0.475, 0.60, 0.525, 0.72, 0.02, OFF)


def glyph_remote_view(c):
	"""Remote view: a screen with an eye"""
	monitor(c)
	eye(c, 0.5, 0.42, 0.22, 0.225)
	c.circle(0.5, 0.42, 0.075, OFF)
	c.circle(0.5, 0.42, 0.04)


def glyph_remote_control(c):
	"""Remote control: a screen with a mouse pointer"""
	monitor(c)
	pts = cursor(0.46, 0.30, 0.44)
	c.halo(pts, 0.045)
	c.poly(pts)


def glyph_bolt(c):
	"""Power on"""
	c.poly([(0.50, 0.12), (0.71, 0.12), (0.57, 0.41), (0.74, 0.41), (0.38, 0.88), (0.46, 0.53), (0.28, 0.53)])


def glyph_reboot(c):
	"""Reboot: circular arrow"""
	cx, cy, r, w = 0.5, 0.53, 0.25, 0.09
	c.arc(cx, cy, r, -40, 240, w)
	a = math.radians(240)
	bx, by = cx + r * math.cos(a), cy + r * math.sin(a)
	tx, ty = -math.sin(a), math.cos(a)
	nx, ny = math.cos(a), math.sin(a)
	c.poly([(bx + nx * 0.14, by + ny * 0.14), (bx + tx * 0.17, by + ty * 0.17), (bx - nx * 0.14, by - ny * 0.14)])


def glyph_power(c):
	"""Power down: power symbol"""
	c.arc(0.5, 0.54, 0.26, -55, 235, 0.09)
	c.line([(0.5, 0.16), (0.5, 0.48)], 0.09)


def glyph_login(c):
	"""Log in: arrow into a door"""
	c.line([(0.55, 0.19), (0.79, 0.19), (0.79, 0.81), (0.55, 0.81)], 0.09)
	c.line([(0.16, 0.50), (0.50, 0.50)], 0.09)
	c.poly([(0.43, 0.32), (0.65, 0.50), (0.43, 0.68)])


def glyph_logout(c):
	"""Log off: arrow out of a door"""
	c.line([(0.45, 0.19), (0.21, 0.19), (0.21, 0.81), (0.45, 0.81)], 0.09)
	c.line([(0.40, 0.50), (0.68, 0.50)], 0.09)
	c.poly([(0.62, 0.32), (0.84, 0.50), (0.62, 0.68)])


def glyph_message(c):
	"""Text message: speech bubble"""
	c.rrect(0.13, 0.18, 0.87, 0.68, 0.10)
	c.poly([(0.25, 0.60), (0.22, 0.86), (0.48, 0.64)])
	c.line([(0.29, 0.355), (0.71, 0.355)], 0.07, OFF)
	c.line([(0.29, 0.505), (0.55, 0.505)], 0.07, OFF)


def glyph_camera(c):
	"""Screenshot"""
	c.rrect(0.33, 0.21, 0.60, 0.36, 0.04)
	c.rrect(0.13, 0.29, 0.87, 0.79, 0.08)
	c.circle(0.5, 0.545, 0.175, OFF)
	c.circle(0.5, 0.545, 0.105)
	c.circle(0.755, 0.39, 0.032, OFF)


def glyph_start_app(c):
	"""Start application: rocket"""
	c.layer(rocket, angle=-45)


def glyph_open_website(c):
	"""Open website: globe and mouse pointer"""
	globe(c, 0.44, 0.44, 0.28)
	pts = cursor(0.55, 0.49, 0.40)
	c.halo(pts, 0.045)
	c.poly(pts)


def glyph_distribute(c):
	"""Distribute files: arrow out of a tray"""
	c.line([(0.15, 0.55), (0.15, 0.81), (0.85, 0.81), (0.85, 0.55)], 0.09)
	c.line([(0.5, 0.33), (0.5, 0.66)], 0.10)
	c.poly([(0.30, 0.37), (0.5, 0.13), (0.70, 0.37)])


def glyph_collect(c):
	"""Collect files: arrow into a tray"""
	c.line([(0.15, 0.55), (0.15, 0.81), (0.85, 0.81), (0.85, 0.55)], 0.09)
	c.line([(0.5, 0.17), (0.5, 0.46)], 0.10)
	c.poly([(0.30, 0.42), (0.5, 0.66), (0.70, 0.42)])


# Master window panels

def glyph_location_computers(c):
	"""Locations & computers: a room with a computer"""
	c.poly([(0.5, 0.13), (0.88, 0.47), (0.79, 0.47), (0.79, 0.84), (0.21, 0.84), (0.21, 0.47), (0.12, 0.47)])
	c.rrect(0.33, 0.49, 0.67, 0.68, 0.02, OFF)
	c.rect(0.47, 0.67, 0.53, 0.74, OFF)
	c.rrect(0.38, 0.72, 0.62, 0.77, 0.015, OFF)


def glyph_slideshow(c):
	"""Slideshow: screen with play button and thumbnails"""
	c.rrect(0.13, 0.15, 0.87, 0.61, 0.05)
	play(c, 0.51, 0.38, 0.22)
	for x in (0.13, 0.40, 0.67):
		c.rrect(x, 0.68, x + 0.20, 0.83, 0.03)


def glyph_spotlight(c):
	"""Spotlight: lamp with a beam of light"""
	c.poly([(0.37, 0.31), (0.63, 0.31), (0.83, 0.74), (0.17, 0.74)], 110)
	c.ellipse(0.15, 0.66, 0.85, 0.86)
	c.poly([(0.36, 0.13), (0.64, 0.13), (0.68, 0.29), (0.32, 0.29)])


# remote access window

def glyph_select_screen(c):
	"""Select screen: two monitors, one of them selected"""
	monitor(c, 0.10, 0.26, 0.475, 0.56, frame=0.05)
	monitor(c, 0.525, 0.26, 0.90, 0.56, screen=False)


def glyph_keyboard(c):
	"""Send shortcut"""
	c.rrect(0.10, 0.27, 0.90, 0.73, 0.07)
	kw, gap = 0.09, 0.045
	for y in (0.35, 0.47):
		for i in range(5):
			x = 0.185 + i * (kw + gap)
			c.rrect(x, y, x + kw, y + 0.075, 0.015, OFF)
	c.rrect(0.30, 0.59, 0.70, 0.655, 0.015, OFF)


def glyph_close(c):
	"""Exit"""
	c.line([(0.31, 0.31), (0.69, 0.69)], 0.12)
	c.line([(0.69, 0.31), (0.31, 0.69)], 0.12)


# Configurator pages

def glyph_sliders(c):
	"""General"""
	for y, k in ((0.29, 0.64), (0.50, 0.36), (0.71, 0.56)):
		c.line([(0.17, y), (0.83, y)], 0.065)
		c.circle(k, y, 0.115, OFF)
		c.circle(k, y, 0.08)


def glyph_cog(c):
	"""Service"""
	cx, cy, r = 0.5, 0.5, 0.24
	c.circle(cx, cy, r)
	for i in range(8):
		a = 2 * math.pi * i / 8
		ux, uy = math.cos(a), math.sin(a)
		vx, vy = -uy, ux
		pts = []
		for rr, half in ((r - 0.03, 0.075), (r + 0.095, 0.05)):
			pts.append((cx + ux * rr + vx * half, cy + uy * rr + vy * half))
			pts.insert(0, (cx + ux * rr - vx * half, cy + uy * rr - vy * half))
		c.poly(pts)
	c.circle(cx, cy, 0.095, OFF)


def glyph_monitor_grid(c):
	"""Master: a screen showing a grid of screens"""
	monitor(c)
	for x in (0.26, 0.52):
		for y in (0.30, 0.44):
			c.rrect(x, y, x + 0.22, y + 0.10, 0.015)


def glyph_shield_keyhole(c):
	"""Access control"""
	shield(c)
	c.circle(0.5, 0.42, 0.075, OFF)
	c.poly([(0.465, 0.45), (0.535, 0.45), (0.565, 0.64), (0.435, 0.64)], OFF)


def glyph_key(c):
	"""Authentication keys"""
	c.layer(key, angle=45, scale=1.04, dx=0.02)


def glyph_tree(c):
	"""LDAP: directory tree"""
	c.rrect(0.37, 0.15, 0.63, 0.35, 0.04)
	c.line([(0.5, 0.33), (0.5, 0.64)], 0.06, caps=False)
	c.line([(0.23, 0.64), (0.23, 0.50), (0.77, 0.50), (0.77, 0.64)], 0.06)
	for x in (0.13, 0.40, 0.67):
		c.rrect(x, 0.62, x + 0.20, 0.82, 0.04)


def glyph_desktop_services(c):
	"""Desktop services: rocket and globe"""
	c.layer(rocket, angle=-45, scale=0.80, dx=-0.08, dy=-0.08)
	c.circle(0.70, 0.70, 0.215, OFF)
	globe(c, 0.70, 0.70, 0.165, 0.05)


def glyph_code(c):
	"""Web API"""
	c.line([(0.33, 0.31), (0.16, 0.50), (0.33, 0.69)], 0.085)
	c.line([(0.67, 0.31), (0.84, 0.50), (0.67, 0.69)], 0.085)
	c.line([(0.57, 0.25), (0.43, 0.75)], 0.085)


def glyph_penguin(c):
	"""Linux"""
	c.ellipse(0.26, 0.32, 0.74, 0.83)
	c.ellipse(0.35, 0.13, 0.65, 0.45)
	c.ring(0.355, 0.48, 0.645, 0.78, 0.035, OFF)
	c.circle(0.445, 0.27, 0.03, OFF)
	c.circle(0.555, 0.27, 0.03, OFF)
	c.poly([(0.455, 0.335), (0.545, 0.335), (0.50, 0.385)], OFF)
	for x in (0.24, 0.52):
		c.ellipse(x - 0.03, 0.76, x + 0.27, 0.91, OFF)
		c.ellipse(x, 0.79, x + 0.24, 0.88)


def glyph_windows(c):
	"""Windows"""
	for x in (0.18, 0.525):
		for y in (0.18, 0.525):
			c.rrect(x, y, x + 0.295, y + 0.295, 0.02)




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


# ---------------------------------------------------------------------------------------------
# Monochrome sets: the coverage mask of the glyph, without the tile and slightly enlarged
# ---------------------------------------------------------------------------------------------

MONO_LIGHT = (35, 38, 41, 255)		# Breeze "shade black" for light colour schemes
MONO_DARK = (239, 240, 241, 255)	# Breeze "cardboard grey" for dark colour schemes
MONO_SCALE = 1.2


def glyph_mask(painter):
	canvas = Canvas()
	painter(canvas)
	return canvas.mask


def tile_glyph_mask(painter):
	"""coverage mask of an add-on glyph drawn with ImageDraw: white parts are the glyph,
	parts painted in the tile colour are cut-outs"""
	img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
	painter(ImageDraw.Draw(img))
	r, g, b, a = img.split()
	white = ImageChops.darker(ImageChops.darker(r, g), b).point(lambda v: 255 if v >= 250 else 0)
	return ImageChops.multiply(white, a)


def monochrome(mask, color):
	big = int(S * MONO_SCALE)
	offset = (big - S) // 2
	mask = mask.resize((big, big), Image.BICUBIC).crop((offset, offset, offset + S, offset + S))
	img = Image.new("RGBA", (S, S), color)
	img.putalpha(mask)
	return img


# ---------------------------------------------------------------------------------------------
# Classic set: Breeze style icons for the add-ons, matching the original icons of Veyon (flat
# shapes with a subtle vertical gradient, white glyphs and a long shadow towards the bottom right)
# ---------------------------------------------------------------------------------------------

# colours sampled from the original icons
B_BLUE = ((32, 184, 249), (26, 129, 242))
B_GREEN = ((117, 215, 6), (68, 160, 4))
B_ORANGE = ((255, 197, 53), (236, 148, 17))
B_RED = ((219, 42, 63), (200, 22, 38))
B_TEAL = ((26, 188, 156), (17, 140, 118))
B_SEA = ((28, 170, 250), (27, 118, 230))
B_LAND = ((163, 246, 193), (90, 218, 135))
B_SCREEN = ((49, 54, 59), (35, 38, 41))
B_FRAME = ((189, 195, 199), (149, 160, 163))
B_PAPER = ((252, 252, 252), (232, 234, 236))
B_DARK_BLUE = (21, 102, 196)
B_DARK_GREEN = (52, 120, 3)
B_GREY = (77, 77, 77)
B_PAPER_EDGE = (189, 195, 199)
SHADOW_OPACITY = 0.2

_gradients = {}


def vgradient(colors):
	if colors not in _gradients:
		top, bottom = colors
		column = Image.new("RGBA", (1, S))
		for y in range(S):
			t = y / (S - 1)
			column.putpixel((0, y), tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
		_gradients[colors] = column.resize((S, S), Image.NEAREST)
	return _gradients[colors]


def shape(painter):
	return glyph_mask(painter)


def fill(mask, colors):
	img = vgradient(colors).copy()
	img.putalpha(mask)
	return img


def solid(mask, color, opacity=1.0):
	img = Image.new("RGBA", (S, S), tuple(color[:3]) + (255,))
	img.putalpha(mask if opacity >= 1.0 else mask.point(lambda v: int(v * opacity)))
	return img


def shifted(mask, dx, dy):
	out = Image.new("L", (S, S), 0)
	out.paste(mask, (dx, dy))
	return out


def long_shadow(glyph, clip):
	"""shadow cast by the glyph towards the bottom right up to the edge of the clip shape"""
	shadow = glyph
	d = 1
	while d < S:
		shadow = ImageChops.lighter(shadow, shifted(shadow, d, d))
		d *= 2
	return solid(ImageChops.multiply(shadow, clip), (0, 0, 0), SHADOW_OPACITY)


def compose(*layers):
	img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
	for layer in layers:
		img.alpha_composite(layer)
	return img


def emblem(base, colors, glyph, glyph_color=WHITE):
	"""base shape with a glyph and its long shadow"""
	return compose(fill(base, colors), long_shadow(glyph, base), solid(glyph, glyph_color))


def circle_emblem(colors, glyph_painter):
	return emblem(shape(lambda c: c.circle(0.5, 0.5, 0.44)), colors, shape(glyph_painter))


def puzzle(c, cx, cy, s):
	"""puzzle piece of size s centred at cx, cy with two knobs and a socket"""
	x0, y0 = cx - s / 2, cy - s / 2
	c.rrect(x0, y0, x0 + s, y0 + s, s * 0.10)
	c.circle(cx, y0, s * 0.19)
	c.circle(x0 + s, cy, s * 0.19)
	c.circle(cx, y0 + s, s * 0.16, OFF)


def classic_store():
	body = shape(lambda c: c.rrect(0.14, 0.32, 0.86, 0.88, 0.06))
	handle = shape(lambda c: c.arc(0.5, 0.34, 0.17, 180, 360, 0.065))
	return compose(solid(handle, B_DARK_BLUE), emblem(body, B_BLUE, shape(lambda c: puzzle(c, 0.47, 0.62, 0.24))))


def classic_generic():
	piece = shape(lambda c: puzzle(c, 0.47, 0.54, 0.50))
	return compose(solid(shifted(piece, 0, p(0.04)), B_DARK_GREEN), fill(piece, B_GREEN))


def terminal_prompt(c):
	c.line([(0.25, 0.43), (0.36, 0.53), (0.25, 0.63)], 0.06)
	c.line([(0.43, 0.65), (0.62, 0.65)], 0.06)


def window_buttons(c):
	for x in (0.17, 0.25, 0.33):
		c.circle(x, 0.22, 0.025)


def classic_terminal():
	frame = shape(lambda c: c.rrect(0.08, 0.15, 0.92, 0.85, 0.05))
	screen = shape(lambda c: c.rect(0.12, 0.29, 0.88, 0.81))
	return compose(fill(frame, B_FRAME), solid(shape(window_buttons), B_SCREEN[0]),
				   emblem(screen, B_SCREEN, shape(terminal_prompt), MONO_DARK))


def process_list(c):
	for i, length in enumerate((0.44, 0.30, 0.38)):
		y = 0.23 + i * 0.10
		c.circle(0.19, y + 0.025, 0.025)
		c.rrect(0.25, y, 0.25 + length, y + 0.05, 0.02)
	c.line([(0.17, 0.60), (0.36, 0.60), (0.43, 0.53), (0.51, 0.63), (0.58, 0.56), (0.83, 0.56)], 0.035)


def classic_task_manager():
	bezel = shape(lambda c: c.rrect(0.06, 0.12, 0.94, 0.72, 0.04))
	screen = shape(lambda c: c.rect(0.10, 0.16, 0.90, 0.68))
	neck = shape(lambda c: c.rect(0.44, 0.71, 0.56, 0.80))
	foot = shape(lambda c: c.rrect(0.28, 0.79, 0.72, 0.86, 0.025))
	return compose(solid(bezel, B_SCREEN[0]), solid(neck, (127, 140, 141)), solid(foot, B_FRAME[0]),
				   emblem(screen, B_BLUE, shape(process_list)))


def app_rules_grid(c):
	for x in (0.375, 0.515):
		for y in (0.33, 0.47):
			c.rrect(x, y, x + 0.11, y + 0.11, 0.02)


def classic_application_control():
	return emblem(shape(lambda c: shield(c, 0.5, 0.08, 0.72, 0.84)), B_TEAL, shape(app_rules_grid))


def speaker(c, dx):
	c.poly([(0.25 + dx, 0.42), (0.35 + dx, 0.42), (0.49 + dx, 0.29), (0.49 + dx, 0.71), (0.35 + dx, 0.58), (0.25 + dx, 0.58)])


def classic_volume(c):
	speaker(c, -0.03)
	for r in (0.11, 0.20):
		c.arc(0.46, 0.5, r, -48, 48, 0.055)


def classic_volume_mute(c):
	speaker(c, -0.04)
	c.line([(0.57, 0.41), (0.74, 0.59)], 0.06)
	c.line([(0.57, 0.59), (0.74, 0.41)], 0.06)


def classic_volume_lock(c):
	speaker(c, -0.05)
	c.arc(0.655, 0.46, 0.06, 180, 360, 0.045, caps=False)
	c.line([(0.595, 0.46), (0.595, 0.49)], 0.045, caps=False)
	c.line([(0.715, 0.46), (0.715, 0.49)], 0.045, caps=False)
	c.rrect(0.56, 0.48, 0.75, 0.64, 0.02)


def file_arrows(c):
	c.line([(0.40, 0.76), (0.40, 0.54)], 0.07, caps=False)
	c.poly([(0.30, 0.56), (0.40, 0.44), (0.50, 0.56)])
	c.line([(0.60, 0.46), (0.60, 0.68)], 0.07, caps=False)
	c.poly([(0.50, 0.66), (0.60, 0.78), (0.70, 0.66)])


def folder_back(c):
	c.rrect(0.08, 0.15, 0.44, 0.32, 0.03)
	c.rrect(0.08, 0.21, 0.92, 0.86, 0.04)


def classic_file_manager():
	paper = shape(lambda c: c.rect(0.13, 0.26, 0.87, 0.40))
	front = shape(lambda c: c.rrect(0.08, 0.34, 0.92, 0.86, 0.04))
	return compose(solid(shape(folder_back), B_DARK_BLUE), fill(paper, B_PAPER), emblem(front, B_BLUE, shape(file_arrows)))


CONTINENTS = [
	[(0.17, 0.32), (0.30, 0.20), (0.44, 0.22), (0.43, 0.32), (0.35, 0.39), (0.37, 0.49), (0.45, 0.55),
	 (0.42, 0.68), (0.35, 0.84), (0.30, 0.72), (0.27, 0.58), (0.15, 0.48)],
	[(0.55, 0.17), (0.70, 0.13), (0.84, 0.27), (0.73, 0.33), (0.62, 0.33), (0.59, 0.41), (0.71, 0.44),
	 (0.83, 0.53), (0.76, 0.68), (0.66, 0.82), (0.60, 0.70), (0.58, 0.56), (0.52, 0.47), (0.51, 0.29)],
]


def continents(c):
	for pts in CONTINENTS:
		c.poly(pts)
		c.line(pts + [pts[0]], 0.04)


def globe_layers(cut=None):
	sphere = shape(lambda c: c.circle(0.5, 0.5, 0.42))
	land = ImageChops.multiply(shape(continents), sphere)
	if cut is not None:
		sphere = ImageChops.subtract(sphere, cut)
		land = ImageChops.subtract(land, cut)
	return fill(sphere, B_SEA), fill(land, B_LAND)


def classic_internet_access():
	return compose(*globe_layers())


def classic_internet_block():
	cut = shape(lambda c: c.circle(0.72, 0.72, 0.25))
	badge = shape(lambda c: c.circle(0.72, 0.72, 0.205))
	bar = shape(lambda c: c.rrect(0.61, 0.69, 0.83, 0.75, 0.02))
	return compose(*globe_layers(cut), emblem(badge, B_RED, bar))


def browser_window():
	outline = shape(lambda c: c.rrect(0.07, 0.13, 0.93, 0.87, 0.035))
	page = shape(lambda c: c.rrect(0.085, 0.145, 0.915, 0.855, 0.025))
	title = ImageChops.multiply(shape(lambda c: c.rect(0.07, 0.13, 0.93, 0.30)), outline)
	dots = shape(lambda c: [c.circle(x, 0.215, 0.025) for x in (0.15, 0.23, 0.31)])
	return compose(solid(outline, B_PAPER_EDGE), fill(page, B_PAPER), fill(title, B_BLUE), solid(dots, WHITE))


def funnel(c):
	c.poly([(0.30, 0.40), (0.70, 0.40), (0.555, 0.57), (0.555, 0.77), (0.445, 0.71), (0.445, 0.57)])


def send_arrow(c):
	c.line([(0.30, 0.76), (0.60, 0.50)], 0.09)
	c.poly([(0.72, 0.38), (0.71, 0.64), (0.46, 0.41)])


def classic_website_filter():
	return compose(browser_window(), solid(shape(funnel), B_GREY))


def classic_webpage_send():
	return compose(browser_window(), fill(shape(send_arrow), B_GREEN))


def upload_arrow(c):
	c.line([(0.50, 0.80), (0.50, 0.52)], 0.09, caps=False)
	c.poly([(0.33, 0.55), (0.50, 0.36), (0.67, 0.55)])


def classic_file_send():
	doc = shape(lambda c: c.poly([(0.18, 0.07), (0.62, 0.07), (0.82, 0.27), (0.82, 0.93), (0.18, 0.93)]))
	fold = shape(lambda c: c.poly([(0.62, 0.07), (0.62, 0.27), (0.82, 0.27)]))
	return compose(emblem(ImageChops.subtract(doc, fold), B_BLUE, shape(upload_arrow)), solid(fold, B_DARK_BLUE))


# ---------------------------------------------------------------------------------------------
# Icon table
# ---------------------------------------------------------------------------------------------

SETS = ("modern", "monochrome", "monochrome-dark", "classic")
SET_TITLES = {"modern": "Modern", "monochrome": "Monochrome", "monochrome-dark": "Monochrome (dark)", "classic": "Classic"}
THEME_PREFIX = "veyon-"
THEME_DIR = "scalable"

# macos-addons right before the Veyon icons were restyled (PR #11): source of the classic originals
CLASSIC_REF = "56713f6d257472ab7edc979eea7b3797e17efdfb"


class Icon:
	def __init__(self, name, lib, label, modern, mask, classic):
		self.name = name			# icon theme name
		self.lib = lib				# directory of the library which ships the icon (icons/<set>/ below it)
		self.label = label
		self.modern = modern		# callable returning the image of the modern set
		self.mask = mask			# callable returning the glyph mask for the monochrome sets
		self.classic = classic		# callable returning the classic icon, or path of the original in CLASSIC_REF

	def path(self, icon_set):
		return os.path.join(ROOT, self.lib, "icons", icon_set, self.name + ".png")


def tile_icon(name, lib, label, colors, painter, classic):
	"""add-on icon: glyph drawn with ImageDraw on a tile, classic icon drawn by a classic_* function"""
	return Icon(name, lib, label, lambda: make(colors, painter), lambda: tile_glyph_mask(painter), classic)


def glyph_icon(name, lib, label, colors, painter, original):
	"""Veyon icon: glyph drawn on a Canvas, classic icon restored from git history"""
	return Icon(name, lib, label, lambda: make_glyph(colors, painter), lambda: glyph_mask(painter), original)


def circle(painter):
	return lambda: circle_emblem(B_ORANGE, painter)


CORE = "core/resources"
MASTER = "master/resources"
CONFIGURATOR = "configurator/resources"

ICONS = [
	# Add-On Store and bundled add-ons
	tile_icon("veyon-addon-store", CORE, "Add-ons", BLUE, glyph_bag, classic_store),
	tile_icon("veyon-addon-generic", CORE, "Add-on", SLATE, glyph_puzzle, classic_generic),
	tile_icon("veyon-addon-script", CORE, "Script add-on", PURPLE, lambda d: glyph_terminal(d, bg(PURPLE)), classic_terminal),
	tile_icon("veyon-task-manager", "addons/taskmanager/resources", "Task manager", TEAL, glyph_list_bars,
			  classic_task_manager),
	tile_icon("veyon-application-control", "addons/taskmanager/resources", "App rules", TEAL,
			  lambda d: glyph_shield(d, "grid", bg(TEAL)), classic_application_control),
	tile_icon("veyon-volume", "addons/volumecontrol/resources", "Volume", ORANGE, glyph_volume, circle(classic_volume)),
	tile_icon("veyon-volume-mute", "addons/volumecontrol/resources", "Mute", ORANGE, glyph_volume_mute,
			  circle(classic_volume_mute)),
	tile_icon("veyon-volume-lock", "addons/volumecontrol/resources", "Lock volume", ORANGE,
			  lambda d: glyph_volume_lock(d, bg(ORANGE)), circle(classic_volume_lock)),
	tile_icon("veyon-file-manager", "addons/filemanager/resources", "File manager", INDIGO,
			  lambda d: glyph_folder_arrows(d, bg(INDIGO)), classic_file_manager),
	tile_icon("veyon-internet-access", "addons/internetaccess/resources", "Internet access", GREEN, glyph_globe,
			  classic_internet_access),
	tile_icon("veyon-internet-block", "addons/internetaccess/resources", "Block internet", RED,
			  lambda d: glyph_globe_block(d, (215, 70, 76, 255)), classic_internet_block),
	tile_icon("veyon-website-filter", "addons/websitefilter/resources", "Website filter", PINK,
			  lambda d: glyph_website_filter(d, bg(PINK)), classic_website_filter),
	tile_icon("veyon-webpage-send", "addons/webpagesender/resources", "Send web page", BLUE,
			  lambda d: glyph_webpage_send(d, bg(BLUE)), classic_webpage_send),
	tile_icon("veyon-file-send", "addons/filesender/resources", "Send files", INDIGO,
			  lambda d: glyph_file_send(d, bg(INDIGO)), classic_file_send),
	tile_icon("veyon-scripted-addons", "addons/scriptedaddons/resources", "Script add-ons", PURPLE,
			  lambda d: glyph_terminal(d, bg(PURPLE)), classic_terminal),

	# Master toolbar and feature menus. Related features share a colour: SLATE monitoring/neutral,
	# GREEN demo, PURPLE lock and security, TEAL remote access and screen views, RED power, ORANGE
	# user sessions, BLUE content sent to the students (message, application, website), PINK
	# screenshots, INDIGO files and directories.
	glyph_icon("veyon-monitoring", CORE, "Monitoring", SLATE, glyph_screens, "core/resources/presentation-none.png"),
	glyph_icon("veyon-demo", "plugins/demo", "Demo", GREEN, glyph_presentation, "plugins/demo/demo.png"),
	glyph_icon("veyon-demo-fullscreen", "plugins/demo", "Demo: full screen", GREEN, glyph_screen_fullscreen,
			   "plugins/demo/presentation-fullscreen.png"),
	glyph_icon("veyon-demo-window", "plugins/demo", "Demo: window", GREEN, glyph_screen_window,
			   "plugins/demo/presentation-window.png"),
	glyph_icon("veyon-screen-lock", "plugins/screenlock", "Lock", PURPLE, glyph_padlock,
			   "plugins/screenlock/system-lock-screen.png"),
	glyph_icon("veyon-remote-view", "plugins/remoteaccess", "Remote view", TEAL, glyph_remote_view,
			   "plugins/remoteaccess/kmag.png"),
	glyph_icon("veyon-remote-control", "plugins/remoteaccess", "Remote control", TEAL, glyph_remote_control,
			   "plugins/remoteaccess/krdc.png"),
	glyph_icon("veyon-power-on", "plugins/powercontrol", "Power on", RED, glyph_bolt,
			   "plugins/powercontrol/preferences-system-power-management.png"),
	glyph_icon("veyon-reboot", "plugins/powercontrol", "Reboot", RED, glyph_reboot, "plugins/powercontrol/system-reboot.png"),
	glyph_icon("veyon-power-down", "plugins/powercontrol", "Power down", RED, glyph_power,
			   "plugins/powercontrol/system-shutdown.png"),
	glyph_icon("veyon-log-in", "plugins/usersessioncontrol", "Log in", ORANGE, glyph_login,
			   "plugins/usersessioncontrol/login-user.png"),
	glyph_icon("veyon-log-off", "plugins/usersessioncontrol", "Log off", ORANGE, glyph_logout,
			   "plugins/usersessioncontrol/logout-user.png"),
	glyph_icon("veyon-text-message", "plugins/textmessage", "Text message", BLUE, glyph_message,
			   "plugins/textmessage/dialog-information.png"),
	glyph_icon("veyon-screenshot", CORE, "Screenshot", PINK, glyph_camera, "plugins/screenshot/camera-photo.png"),
	glyph_icon("veyon-start-app", "plugins/desktopservices", "Start application", BLUE, glyph_start_app,
			   "plugins/desktopservices/preferences-desktop-launch-feedback.png"),
	glyph_icon("veyon-open-website", "plugins/desktopservices", "Open website", BLUE, glyph_open_website,
			   "plugins/desktopservices/internet-web-browser.png"),
	glyph_icon("veyon-file-transfer", "plugins/filetransfer", "File transfer", INDIGO, glyph_distribute,
			   "plugins/filetransfer/distribute-files.png"),
	glyph_icon("veyon-collect-files", "plugins/filetransfer", "Collect files", INDIGO, glyph_collect,
			   "plugins/filetransfer/collect-files.png"),
	# Master window panels
	glyph_icon("veyon-locations-computers", MASTER, "Locations & computers", INDIGO, glyph_location_computers,
			   "master/resources/computers.png"),
	glyph_icon("veyon-slideshow", MASTER, "Slideshow", TEAL, glyph_slideshow, "master/resources/computer-slideshow.png"),
	glyph_icon("veyon-spotlight", MASTER, "Spotlight", ORANGE, glyph_spotlight, "master/resources/spotlight.png"),
	# remote access window
	glyph_icon("veyon-select-screen", "plugins/remoteaccess", "Select screen", TEAL, glyph_select_screen,
			   "plugins/remoteaccess/preferences-system-windows-effect-desktopgrid.png"),
	glyph_icon("veyon-send-shortcut", "plugins/remoteaccess", "Send shortcut", TEAL, glyph_keyboard,
			   "plugins/remoteaccess/preferences-desktop-keyboard.png"),
	glyph_icon("veyon-exit", "plugins/remoteaccess", "Exit", SLATE, glyph_close, "plugins/remoteaccess/application-exit.png"),
	# Configurator pages
	glyph_icon("veyon-config-general", CONFIGURATOR, "General", SLATE, glyph_sliders,
			   "configurator/resources/configure-shortcuts.png"),
	glyph_icon("veyon-config-service", CONFIGURATOR, "Service", TEAL, glyph_cog,
			   "configurator/resources/application-x-sharedlib.png"),
	glyph_icon("veyon-config-master", CONFIGURATOR, "Master", BLUE, glyph_monitor_grid,
			   "configurator/resources/application-x-ms-dos-executable.png"),
	glyph_icon("veyon-config-access-control", CONFIGURATOR, "Access control", PURPLE, glyph_shield_keyhole,
			   "configurator/resources/network-vpn.png"),
	glyph_icon("veyon-authentication-keys", CORE, "Authentication keys", PURPLE, glyph_key,
			   "core/resources/application-x-pem-key.png"),
	glyph_icon("veyon-builtin-directory", "plugins/builtindirectory", "Locations & computers", INDIGO,
			   glyph_location_computers, "plugins/builtindirectory/builtindirectory.png"),
	glyph_icon("veyon-ldap", "plugins/ldap", "LDAP", INDIGO, glyph_tree, "plugins/ldap/application-x-kexi-connectiondata.png"),
	glyph_icon("veyon-demo-server", "plugins/demo", "Demo server", GREEN, glyph_screens_overlapping,
			   "plugins/demo/window-duplicate.png"),
	glyph_icon("veyon-desktop-services", "plugins/desktopservices", "Desktop services", BLUE, glyph_desktop_services,
			   "plugins/desktopservices/desktop-services.png"),
	glyph_icon("veyon-file-transfer-settings", "plugins/filetransfer", "File transfer", INDIGO, glyph_distribute,
			   "plugins/filetransfer/distribute-files-dark.png"),
	glyph_icon("veyon-web-api", "plugins/webapi", "Web API", TEAL, glyph_code, "plugins/webapi/webapi.png"),
	glyph_icon("veyon-linux", "plugins/platform/linux", "Linux", SLATE, glyph_penguin, "plugins/platform/linux/tux.png"),
	glyph_icon("veyon-windows", "plugins/platform/windows", "Windows", BLUE, glyph_windows,
			   "plugins/platform/windows/windows-logo.png"),
]


# ---------------------------------------------------------------------------------------------
# output: PNGs, index.theme files and the icon theme sections of the .qrc files
# ---------------------------------------------------------------------------------------------

def save(img, path):
	out = img.resize((SIZE, SIZE), Image.LANCZOS)
	os.makedirs(os.path.dirname(path), exist_ok=True)
	out.save(path)
	print("wrote", os.path.relpath(path, ROOT).replace(os.sep, "/"))
	return out


def git_show(ref, path):
	try:
		return subprocess.run(["git", "-C", ROOT, "show", f"{ref}:{path}"], capture_output=True, check=True).stdout
	except (OSError, subprocess.CalledProcessError):
		return None


def restore_classic(icon):
	data = git_show(CLASSIC_REF, icon.classic)
	if data is None:
		raise SystemExit(f"could not read {icon.classic} from {CLASSIC_REF}")
	path = icon.path("classic")
	os.makedirs(os.path.dirname(path), exist_ok=True)
	with open(path, "wb") as f:
		f.write(data)
	print("restored", os.path.relpath(path, ROOT).replace(os.sep, "/"))


def write_index_themes():
	for icon_set in SETS:
		lines = [
			"[Icon Theme]",
			f"Name=Veyon {SET_TITLES[icon_set]}",
			"Comment=Icons of Veyon (generated by addons/tools/make-icons.py)",
		]
		if icon_set != "modern":
			lines.append(f"Inherits={THEME_PREFIX}modern")
		lines += [f"Directories={THEME_DIR}", "", f"[{THEME_DIR}]", "Size=128", "MinSize=8", "MaxSize=512", "Type=Scalable", ""]
		path = os.path.join(ROOT, CORE, "icons", icon_set, "index.theme")
		os.makedirs(os.path.dirname(path), exist_ok=True)
		with open(path, "w", newline="\n") as f:
			f.write("\n".join(lines))


def qrc_for(lib):
	"""the .qrc file of the library: in the icons' directory or one up, named after one of them"""
	for directory in (lib, os.path.dirname(lib)):
		for name in (os.path.basename(directory), os.path.basename(os.path.dirname(directory))):
			path = os.path.join(ROOT, directory, name + ".qrc")
			if name and os.path.exists(path):
				return path
	raise SystemExit(f"no .qrc file found for {lib}")


def update_qrc(lib, names):
	qrc = qrc_for(lib)
	with open(qrc, newline="") as f:
		text = f.read()
	newline = "\r\n" if "\r\n" in text else "\n"
	indent = re.search(r"^([ \t]*)<qresource", text, re.M)
	indent = indent.group(1) if indent else "  "
	text = re.sub(r'[ \t]*<qresource prefix="/icons/' + THEME_PREFIX + r'[^"]*">.*?</qresource>\r?\n', "", text, flags=re.S)
	prefix = os.path.relpath(os.path.join(ROOT, lib), os.path.dirname(qrc)).replace(os.sep, "/")
	prefix = "" if prefix == "." else prefix + "/"
	lines = []
	for icon_set in SETS:
		lines.append(f'{indent}<qresource prefix="/icons/{THEME_PREFIX}{icon_set}">')
		if lib == CORE:
			lines.append(f'{indent * 2}<file alias="index.theme">{prefix}icons/{icon_set}/index.theme</file>')
		for name in names:
			lines.append(f'{indent * 2}<file alias="{THEME_DIR}/{name}.png">{prefix}icons/{icon_set}/{name}.png</file>')
		lines.append(f"{indent}</qresource>")
	text = text.replace("</RCC>", newline.join(lines) + newline + "</RCC>")
	with open(qrc, "w", newline="") as f:
		f.write(text)
	print("updated", os.path.relpath(qrc, ROOT).replace(os.sep, "/"))


def generate(restore):
	images = {icon_set: {} for icon_set in SETS}
	for icon in ICONS:
		mask = icon.mask()
		images["modern"][icon.name] = save(icon.modern(), icon.path("modern"))
		images["monochrome"][icon.name] = save(monochrome(mask, MONO_LIGHT), icon.path("monochrome"))
		images["monochrome-dark"][icon.name] = save(monochrome(mask, MONO_DARK), icon.path("monochrome-dark"))
		if callable(icon.classic):
			images["classic"][icon.name] = save(icon.classic(), icon.path("classic"))
		else:
			if restore:
				restore_classic(icon)
			if not os.path.exists(icon.path("classic")):
				raise SystemExit(f"{icon.path('classic')} is missing - run with --restore-classic")
			images["classic"][icon.name] = Image.open(icon.path("classic")).convert("RGBA")

	write_index_themes()
	libs = {}
	for icon in ICONS:
		libs.setdefault(icon.lib, []).append(icon.name)
	for lib, names in libs.items():
		update_qrc(lib, names)
	return images


# ---------------------------------------------------------------------------------------------
# contact sheets
# ---------------------------------------------------------------------------------------------

PREVIEW_SIZES = (16, 20, 32, 48)
LIGHT = (246, 246, 246, 255)
DARK = (38, 40, 44, 255)
TEXT_LIGHT = (30, 30, 30, 255)
TEXT_DARK = (225, 225, 225, 255)

# (title, background, icon set) - the monochrome set is shown in the variant Veyon picks for the background
COLUMNS = [
	("Modern", LIGHT, "modern"), ("Modern", DARK, "modern"),
	("Monochrome", LIGHT, "monochrome"), ("Monochrome", DARK, "monochrome-dark"),
	("Classic", LIGHT, "classic"), ("Classic", DARK, "classic"),
]

MASTER_TOOLBAR = [
	"veyon-monitoring", "veyon-demo", "veyon-screen-lock", "veyon-remote-view", "veyon-remote-control",
	"veyon-power-on", "veyon-reboot", "veyon-power-down", "veyon-log-in", "veyon-log-off", "veyon-text-message",
	"veyon-screenshot", "veyon-start-app", "veyon-open-website", "veyon-file-transfer", "veyon-task-manager",
	"veyon-volume", "veyon-internet-access", "veyon-website-filter", "veyon-webpage-send", "veyon-file-send",
	"veyon-file-manager",
]

CONFIGURATOR_PAGES = [
	"veyon-addon-store", "veyon-config-general", "veyon-config-service", "veyon-config-master",
	"veyon-config-access-control", "veyon-authentication-keys", "veyon-builtin-directory", "veyon-demo-server",
	"veyon-desktop-services", "veyon-file-transfer-settings", "veyon-ldap", "veyon-windows", "veyon-linux",
	"veyon-web-api", "veyon-file-manager", "veyon-internet-access", "veyon-scripted-addons", "veyon-website-filter",
]


def font(size):
	try:
		return ImageFont.load_default(size=size)
	except TypeError:
		return ImageFont.load_default()


def scaled(img, size):
	# area averaging, like Qt's smooth scaling of QIcon pixmaps
	return img.resize((size, size), Image.BOX)


def text(d, xy, s, size, fill_color):
	d.text(xy, s, font=font(size), fill=fill_color)


def foreground(background):
	return TEXT_LIGHT if background == LIGHT else TEXT_DARK


def background_name(background):
	return "light" if background == LIGHT else "dark"


def size_matrix(names, labels, images, title):
	"""every icon of every set at all preview sizes on light and dark backgrounds"""
	strip_w = sum(PREVIEW_SIZES) + 12 * (len(PREVIEW_SIZES) + 1)
	label_w, row_h, head_h = 230, 64, 64
	sheet = Image.new("RGBA", (label_w + len(COLUMNS) * (strip_w + 8), head_h + row_h * len(names)), (255, 255, 255, 255))
	d = ImageDraw.Draw(sheet)
	text(d, (12, 10), f"{title} ({'/'.join(map(str, PREVIEW_SIZES))} px)", 20, TEXT_LIGHT)
	for ci, (column_title, background, _) in enumerate(COLUMNS):
		x = label_w + ci * (strip_w + 8)
		text(d, (x + 12, 38), f"{column_title}, {background_name(background)}", 14, TEXT_LIGHT)
	for ri, name in enumerate(names):
		y = head_h + ri * row_h
		text(d, (12, y + 14), labels[name], 15, TEXT_LIGHT)
		text(d, (12, y + 36), name, 11, (110, 110, 110, 255))
		for ci, (_, background, icon_set) in enumerate(COLUMNS):
			x = label_w + ci * (strip_w + 8)
			d.rectangle([x, y, x + strip_w, y + row_h - 1], fill=background)
			ix = x + 12
			for s in PREVIEW_SIZES:
				sheet.alpha_composite(scaled(images[icon_set][name], s), (ix, y + (row_h - s) // 2))
				ix += s + 12
	return sheet


def toolbar_mockup(names, labels, images):
	"""the Master toolbar at 32 px in every set"""
	cell_w, icon, pad, label_w = 84, 32, 14, 130
	row_h = icon + 46
	sheet = Image.new("RGBA", (label_w + cell_w * len(names) + pad, pad + row_h * len(COLUMNS)), (255, 255, 255, 255))
	d = ImageDraw.Draw(sheet)
	for ri, (column_title, background, icon_set) in enumerate(COLUMNS):
		y = pad + ri * row_h
		d.rectangle([0, y, sheet.width, y + row_h - 4], fill=background)
		fg = foreground(background)
		text(d, (10, y + 12), column_title, 14, fg)
		text(d, (10, y + 32), background_name(background), 12, fg)
		for i, name in enumerate(names):
			x = label_w + i * cell_w
			sheet.alpha_composite(scaled(images[icon_set][name], icon), (x + (cell_w - icon) // 2, y + 6))
			label = labels[name]
			tw = d.textlength(label, font=font(11))
			text(d, (x + (cell_w - tw) / 2, y + icon + 14), label, 11, fg)
	return sheet


def page_list_mockup(names, labels, images):
	"""the Configurator's page list at 48 px in every set"""
	col_w, icon, row_h = 260, 48, 58
	sheet = Image.new("RGBA", (col_w * len(COLUMNS), 40 + row_h * len(names)), (255, 255, 255, 255))
	d = ImageDraw.Draw(sheet)
	for ci, (column_title, background, icon_set) in enumerate(COLUMNS):
		x = ci * col_w
		d.rectangle([x + 4, 0, x + col_w - 4, sheet.height], fill=background)
		fg = foreground(background)
		text(d, (x + 14, 10), f"{column_title}, {background_name(background)}", 14, fg)
		for ri, name in enumerate(names):
			y = 40 + ri * row_h
			sheet.alpha_composite(scaled(images[icon_set][name], icon), (x + 14, y + (row_h - icon) // 2))
			text(d, (x + 14 + icon + 12, y + row_h // 2 - 9), labels[name], 15, fg)
	return sheet


def write_contact_sheets(out_dir, images):
	os.makedirs(out_dir, exist_ok=True)
	labels = {icon.name: icon.label for icon in ICONS}
	addons = [icon.name for icon in ICONS if callable(icon.classic)]
	features = [icon.name for icon in ICONS if not callable(icon.classic) and icon.name not in CONFIGURATOR_PAGES]
	sheets = {
		"master-toolbar.png": toolbar_mockup(MASTER_TOOLBAR, labels, images),
		"configurator-pages.png": page_list_mockup(CONFIGURATOR_PAGES, labels, images),
		"sizes-addons.png": size_matrix(addons, labels, images, "Add-On Store and add-ons"),
		"sizes-master.png": size_matrix(features, labels, images,
										"Master toolbar, feature menus, Master panels and remote access window"),
		"sizes-configurator.png": size_matrix(CONFIGURATOR_PAGES, labels, images, "Configurator pages"),
	}
	for name, sheet in sheets.items():
		path = os.path.join(out_dir, name)
		sheet.convert("RGB").save(path, optimize=True)
		print("wrote", path)


def main():
	parser = argparse.ArgumentParser(description="generates the icon sets of Veyon")
	parser.add_argument("--contact-sheet", metavar="DIR", help="also write previews of all icon sets into DIR")
	parser.add_argument("--restore-classic", action="store_true",
						help=f"(re)write the original icons of the classic set from git revision {CLASSIC_REF[:9]}")
	args = parser.parse_args()

	images = generate(args.restore_classic)

	if args.contact_sheet:
		write_contact_sheets(args.contact_sheet, images)


if __name__ == "__main__":
	main()
