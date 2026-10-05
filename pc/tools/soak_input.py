#!/usr/bin/env python3
# Prints an --input script for a soak run: scripted navigation through every
# screen, repeated, with random pointer movement in between. usage: soak.py FRAMES [SEED]
import random
import sys

frames = int(sys.argv[1])
rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
ev = ["P0:0@1", "A@300"]
t = 420


def point(x, y, dt=30):
    global t
    ev.append("P%.2f:%.2f@%d" % (x, y, t))
    t += 6
    ev.append("P%.2f:%.2f@%d" % (x, y + 0.02, t))
    t += dt


def press(b="A", hold=2, dt=60):
    global t
    ev.append("%s@%d+%d" % (b, t, hold))
    t += hold + dt


def wander(n):
    global t
    for _ in range(n):
        ev.append("P%.2f:%.2f@%d" % (rng.uniform(-1.05, 1.05), rng.uniform(-1.05, 1.05), t))
        t += rng.randint(3, 25)


BL, BM, BR, TL, TR = (-0.7, 0.8), (0, 0.8), (0.7, 0.8), (-0.67, -0.8), (0.66, -0.8)
t = 900
while t < frames - 6000 and len(ev) < 850:
    # A: International News, scroll the list, an article without the globe view, back twice
    point(0, 0.45); press(dt=250)
    wander(6)
    point(*BM); press(hold=30); point(0, -0.8); press(hold=60)
    point(0, -0.26); press(dt=300)
    point(*BM); press(hold=40); point(*TR); press(); point(*TL); press()
    wander(8)
    point(*BL); press(dt=300)
    point(*BL); press(dt=300)
    # B: National News, second headline (has a location): article, globe,
    # zoom, pin, regional list, article, and back (5 backs)
    point(0, 0.19); press(dt=300)
    point(0, 0.2); press(dt=300)
    point(*BM); press(hold=40)
    point(*BR); press(dt=300)
    wander(8)
    point(*TR); press(); point(*TL); press()
    point(0.08, -0.03, dt=60); press(dt=250)
    point(0.2, -0.3); press(dt=300)
    point(*TR); press(); point(*BM); press(hold=30)
    wander(5)
    for _ in range(5):
        point(*BL); press(dt=300)
    # C: slide show, into an article, continue, next, previous, article, end
    point(*BR); press(dt=700)
    wander(6)
    point(0, 0); press(dt=250)
    point(*BM); press(hold=30); point(*BR); press(dt=300)
    press("RIGHT", dt=200); press("LEFT", dt=200)
    point(0, 0); press(dt=250); point(*BL); press(dt=500)
    wander(4)
print(",".join(ev))
