#!/usr/bin/env python3
#
# make-icons.py - generates the PNG icons of the Add-On Store, the bundled add-ons, the Veyon
#                 features (Master toolbar) and the Configurator pages
#
# usage: python3 addons/tools/make-icons.py                       (requires Pillow)
#        python3 addons/tools/make-icons.py --contact-sheet DIR [--before REF]
#
# All icons are drawn at a high resolution and downsampled for smooth edges. They consist of
# a rounded, gradient filled tile with a white glyph which works on light and dark themes.
#
# --contact-sheet writes before/after previews of every generated icon at 16, 20, 32 and 48 px
# on light and dark backgrounds into DIR. "Before" is the icon as committed in REF (default HEAD).

import argparse
import io
import math
import os
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


def save(img, path):
	out = img.resize((SIZE, SIZE), Image.LANCZOS)
	os.makedirs(os.path.dirname(path), exist_ok=True)
	out.save(path)
	print("wrote", os.path.relpath(path, ROOT))
	return out


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


ADDON_ICONS = [
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

# Related features share a colour: SLATE monitoring/neutral, GREEN demo, PURPLE lock and security,
# TEAL remote access and screen views, RED power, ORANGE user sessions, BLUE content sent to the
# students (message, application, website), PINK screenshots, INDIGO files and directories.
FEATURE_ICONS = [
	# Master toolbar and feature menus
	("core/resources/presentation-none.png", SLATE, glyph_screens, "Monitoring"),
	("plugins/demo/demo.png", GREEN, glyph_presentation, "Demo"),
	("plugins/demo/presentation-fullscreen.png", GREEN, glyph_screen_fullscreen, "Demo: full screen"),
	("plugins/demo/presentation-window.png", GREEN, glyph_screen_window, "Demo: window"),
	("plugins/screenlock/system-lock-screen.png", PURPLE, glyph_padlock, "Lock"),
	("plugins/remoteaccess/kmag.png", TEAL, glyph_remote_view, "Remote view"),
	("plugins/remoteaccess/krdc.png", TEAL, glyph_remote_control, "Remote control"),
	("plugins/powercontrol/preferences-system-power-management.png", RED, glyph_bolt, "Power on"),
	("plugins/powercontrol/system-reboot.png", RED, glyph_reboot, "Reboot"),
	("plugins/powercontrol/system-shutdown.png", RED, glyph_power, "Power down"),
	("plugins/usersessioncontrol/login-user.png", ORANGE, glyph_login, "Log in"),
	("plugins/usersessioncontrol/logout-user.png", ORANGE, glyph_logout, "Log off"),
	("plugins/textmessage/dialog-information.png", BLUE, glyph_message, "Text message"),
	("plugins/screenshot/camera-photo.png", PINK, glyph_camera, "Screenshot"),
	("plugins/desktopservices/preferences-desktop-launch-feedback.png", BLUE, glyph_start_app, "Start application"),
	("plugins/desktopservices/internet-web-browser.png", BLUE, glyph_open_website, "Open website"),
	("plugins/filetransfer/distribute-files.png", INDIGO, glyph_distribute, "File transfer"),
	("plugins/filetransfer/collect-files.png", INDIGO, glyph_collect, "Collect files"),
	# Master window panels
	("master/resources/computers.png", INDIGO, glyph_location_computers, "Locations & computers"),
	("master/resources/camera-photo.png", PINK, glyph_camera, "Screenshots"),
	("master/resources/computer-slideshow.png", TEAL, glyph_slideshow, "Slideshow"),
	("master/resources/spotlight.png", ORANGE, glyph_spotlight, "Spotlight"),
	# remote access window
	("plugins/remoteaccess/preferences-system-windows-effect-desktopgrid.png", TEAL, glyph_select_screen, "Select screen"),
	("plugins/remoteaccess/preferences-desktop-keyboard.png", TEAL, glyph_keyboard, "Send shortcut"),
	("plugins/remoteaccess/camera-photo.png", PINK, glyph_camera, "Screenshot"),
	("plugins/remoteaccess/application-exit.png", SLATE, glyph_close, "Exit"),
	# Configurator pages
	("configurator/resources/configure-shortcuts.png", SLATE, glyph_sliders, "General"),
	("configurator/resources/application-x-sharedlib.png", TEAL, glyph_cog, "Service"),
	("configurator/resources/application-x-ms-dos-executable.png", BLUE, glyph_monitor_grid, "Master"),
	("configurator/resources/network-vpn.png", PURPLE, glyph_shield_keyhole, "Access control"),
	("core/resources/application-x-pem-key.png", PURPLE, glyph_key, "Authentication keys"),
	("plugins/builtindirectory/builtindirectory.png", INDIGO, glyph_location_computers, "Locations & computers"),
	("plugins/ldap/application-x-kexi-connectiondata.png", INDIGO, glyph_tree, "LDAP"),
	("plugins/demo/window-duplicate.png", GREEN, glyph_screens_overlapping, "Demo server"),
	("plugins/desktopservices/desktop-services.png", BLUE, glyph_desktop_services, "Desktop services"),
	("plugins/filetransfer/distribute-files-dark.png", INDIGO, glyph_distribute, "File transfer"),
	("plugins/webapi/webapi.png", TEAL, glyph_code, "Web API"),
	("plugins/platform/linux/tux.png", SLATE, glyph_penguin, "Linux"),
	("plugins/platform/windows/windows-logo.png", BLUE, glyph_windows, "Windows"),
]


# ---------------------------------------------------------------------------------------------
# contact sheets
# ---------------------------------------------------------------------------------------------

PREVIEW_SIZES = (16, 20, 32, 48)
LIGHT = (246, 246, 246, 255)
DARK = (38, 40, 44, 255)
TEXT_LIGHT = (30, 30, 30, 255)
TEXT_DARK = (225, 225, 225, 255)

MASTER_TOOLBAR = [
	"core/resources/presentation-none.png", "plugins/demo/demo.png", "plugins/screenlock/system-lock-screen.png",
	"plugins/remoteaccess/kmag.png", "plugins/remoteaccess/krdc.png",
	"plugins/powercontrol/preferences-system-power-management.png", "plugins/powercontrol/system-reboot.png",
	"plugins/powercontrol/system-shutdown.png", "plugins/usersessioncontrol/login-user.png",
	"plugins/usersessioncontrol/logout-user.png", "plugins/textmessage/dialog-information.png",
	"plugins/screenshot/camera-photo.png", "plugins/desktopservices/preferences-desktop-launch-feedback.png",
	"plugins/desktopservices/internet-web-browser.png", "plugins/filetransfer/distribute-files.png",
	"addons/taskmanager/resources/taskmanager.png", "addons/volumecontrol/resources/volume.png",
	"addons/internetaccess/resources/internet-access.png", "addons/websitefilter/resources/website-filter.png",
	"addons/webpagesender/resources/webpage-send.png", "addons/filesender/resources/file-send.png",
	"addons/filemanager/resources/filemanager.png",
]

CONFIGURATOR_PAGES = [
	"core/resources/addon-store.png", "configurator/resources/configure-shortcuts.png",
	"configurator/resources/application-x-sharedlib.png", "configurator/resources/application-x-ms-dos-executable.png",
	"configurator/resources/network-vpn.png", "core/resources/application-x-pem-key.png",
	"plugins/builtindirectory/builtindirectory.png", "plugins/demo/window-duplicate.png",
	"plugins/desktopservices/desktop-services.png", "plugins/filetransfer/distribute-files-dark.png",
	"plugins/ldap/application-x-kexi-connectiondata.png", "plugins/platform/windows/windows-logo.png",
	"plugins/platform/linux/tux.png", "plugins/webapi/webapi.png",
	"addons/filemanager/resources/filemanager.png", "addons/internetaccess/resources/internet-access.png",
	"addons/scriptedaddons/resources/scripted-addons.png", "addons/websitefilter/resources/website-filter.png",
]

ADDON_LABELS = {
	"core/resources/addon-store.png": "Add-ons",
	"addons/taskmanager/resources/taskmanager.png": "Task manager",
	"addons/volumecontrol/resources/volume.png": "Volume",
	"addons/internetaccess/resources/internet-access.png": "Internet access",
	"addons/websitefilter/resources/website-filter.png": "Website filter",
	"addons/webpagesender/resources/webpage-send.png": "Send web page",
	"addons/filesender/resources/file-send.png": "Send files",
	"addons/filemanager/resources/filemanager.png": "File manager",
	"addons/scriptedaddons/resources/scripted-addons.png": "Script add-ons",
}


def font(size):
	try:
		return ImageFont.load_default(size=size)
	except TypeError:
		return ImageFont.load_default()


def scaled(img, size):
	# area averaging, like Qt's smooth scaling of QIcon pixmaps
	return img.resize((size, size), Image.BOX)


def committed_image(ref, path):
	try:
		data = subprocess.run(["git", "-C", ROOT, "show", f"{ref}:{path}"], capture_output=True, check=True).stdout
	except (OSError, subprocess.CalledProcessError):
		return None
	return Image.open(io.BytesIO(data)).convert("RGBA")


def text(d, xy, s, size, fill):
	d.text(xy, s, font=font(size), fill=fill)


def size_matrix(rows, before, after, title):
	"""every icon at all preview sizes, before and after, on light and dark backgrounds"""
	strip_w = sum(PREVIEW_SIZES) + 12 * (len(PREVIEW_SIZES) + 1)
	label_w, row_h, head_h = 250, 64, 64
	groups = [("before", LIGHT, before), ("before", DARK, before), ("after", LIGHT, after), ("after", DARK, after)]
	sheet = Image.new("RGBA", (label_w + len(groups) * (strip_w + 8), head_h + row_h * len(rows)), (255, 255, 255, 255))
	d = ImageDraw.Draw(sheet)
	text(d, (12, 10), title, 20, TEXT_LIGHT)
	for gi, (name, background, _) in enumerate(groups):
		x = label_w + gi * (strip_w + 8)
		text(d, (x + 12, 38), f"{name}, {'light' if background == LIGHT else 'dark'}  ({'/'.join(map(str, PREVIEW_SIZES))} px)", 13, TEXT_LIGHT)
	for ri, (path, label) in enumerate(rows):
		y = head_h + ri * row_h
		text(d, (12, y + 14), label, 15, TEXT_LIGHT)
		text(d, (12, y + 36), path.split("/")[-1], 11, (110, 110, 110, 255))
		for gi, (_, background, images) in enumerate(groups):
			x = label_w + gi * (strip_w + 8)
			d.rectangle([x, y, x + strip_w, y + row_h - 1], fill=background)
			img = images.get(path)
			if img is None:
				continue
			ix = x + 12
			for s in PREVIEW_SIZES:
				sheet.alpha_composite(scaled(img, s), (ix, y + (row_h - s) // 2))
				ix += s + 12
	return sheet


def toolbar_mockup(paths, labels, before, after):
	"""the Master toolbar at 32 px, before and after"""
	cell_w, icon, pad = 84, 32, 14
	rows = [("before", LIGHT, before), ("after", LIGHT, after), ("before", DARK, before), ("after", DARK, after)]
	row_h = icon + 46
	sheet = Image.new("RGBA", (70 + cell_w * len(paths) + pad, pad + row_h * len(rows)), (255, 255, 255, 255))
	d = ImageDraw.Draw(sheet)
	for ri, (name, background, images) in enumerate(rows):
		y = pad + ri * row_h
		d.rectangle([0, y, sheet.width, y + row_h - 4], fill=background)
		fg = TEXT_LIGHT if background == LIGHT else TEXT_DARK
		text(d, (10, y + 12), name, 13, fg)
		for i, path in enumerate(paths):
			x = 70 + i * cell_w
			img = images.get(path)
			if img is not None:
				sheet.alpha_composite(scaled(img, icon), (x + (cell_w - icon) // 2, y + 6))
			label = labels.get(path, "")
			tw = d.textlength(label, font=font(11))
			text(d, (x + (cell_w - tw) / 2, y + icon + 14), label, 11, fg)
	return sheet


def page_list_mockup(paths, labels, before, after):
	"""the Configurator's page list at 48 px, before and after"""
	col_w, icon, row_h = 290, 48, 58
	cols = [("before", LIGHT, before), ("after", LIGHT, after), ("before", DARK, before), ("after", DARK, after)]
	sheet = Image.new("RGBA", (col_w * len(cols), 36 + row_h * len(paths)), (255, 255, 255, 255))
	d = ImageDraw.Draw(sheet)
	for ci, (name, background, images) in enumerate(cols):
		x = ci * col_w
		d.rectangle([x + 4, 0, x + col_w - 4, sheet.height], fill=background)
		fg = TEXT_LIGHT if background == LIGHT else TEXT_DARK
		text(d, (x + 14, 10), name, 14, fg)
		for ri, path in enumerate(paths):
			y = 36 + ri * row_h
			img = images.get(path)
			if img is not None:
				sheet.alpha_composite(scaled(img, icon), (x + 14, y + (row_h - icon) // 2))
			text(d, (x + 14 + icon + 12, y + row_h // 2 - 9), labels.get(path, ""), 15, fg)
	return sheet


def write_contact_sheets(out_dir, generated, ref):
	os.makedirs(out_dir, exist_ok=True)
	labels = dict(ADDON_LABELS)
	for path, _, _, label in FEATURE_ICONS:
		labels.setdefault(path, label)
	paths = set(generated) | set(MASTER_TOOLBAR) | set(CONFIGURATOR_PAGES)
	before = {path: committed_image(ref, path) for path in paths}
	after = {path: generated.get(path) or Image.open(os.path.join(ROOT, path)).convert("RGBA") for path in paths}

	sheets = {
		"master-toolbar.png": toolbar_mockup(MASTER_TOOLBAR, labels, before, after),
		"configurator-pages.png": page_list_mockup(CONFIGURATOR_PAGES, labels, before, after),
		"sizes-master.png": size_matrix([(path, label) for path, _, _, label in FEATURE_ICONS
										 if not path.startswith("configurator/") and path not in CONFIGURATOR_PAGES],
										before, after, "Master toolbar, feature menus, Master panels and remote access window"),
		"sizes-configurator.png": size_matrix([(path, labels[path]) for path in CONFIGURATOR_PAGES],
											  before, after, "Configurator pages"),
	}
	for name, sheet in sheets.items():
		path = os.path.join(out_dir, name)
		sheet.convert("RGB").save(path, optimize=True)
		print("wrote", path)


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--contact-sheet", metavar="DIR", help="also write before/after previews into DIR")
	parser.add_argument("--before", metavar="REF", default="HEAD", help="git revision of the 'before' icons")
	args = parser.parse_args()

	generated = {}
	for path, colors, painter in ADDON_ICONS:
		generated[path] = save(make(colors, painter), os.path.join(ROOT, path))
	for path, colors, painter, _ in FEATURE_ICONS:
		generated[path] = save(make_glyph(colors, painter), os.path.join(ROOT, path))

	if args.contact_sheet:
		write_contact_sheets(args.contact_sheet, generated, args.before)


if __name__ == "__main__":
	main()
