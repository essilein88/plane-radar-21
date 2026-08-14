#pragma once

namespace ui {

/** Draw the static sonar/radar grid (black disc, green overlay, labels). */
void radarDisplayDraw();

/** Redraw aircraft only (blits cached grid; no full-screen clear). */
void radarDisplayRefreshAircraft();

/** Advances the animated radar sweep if enough time has elapsed. */
bool radarDisplayAnimate();

/** Handles a touch point; returns true if the display changed. */
bool radarDisplayHandleTouch(int x, int y);

}  // namespace ui
