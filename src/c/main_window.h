#pragma once

#include "globals.h"
#include "config.h"

void main_window_push();

// Re-apply the live config to the already-loaded window: rebuild the corner
// readouts, re-subscribe the tick service (second-hand toggle) and repaint.
// Called from comm.c whenever a fresh config or weather sample arrives.
void main_window_refresh(void);
