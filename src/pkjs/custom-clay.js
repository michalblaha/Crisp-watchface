// Clay custom function: per-theme accent palette memory.
//
// Crisp has one set of color pickers but two themes (dark / light). This
// function makes the watch only ever hold the *active* theme's palette while
// the phone remembers a separate palette per theme: switching the Theme select
// stashes the current colors under the old theme and loads the colors saved for
// the new one (seeding from the theme's defaults the first time). Edits are
// persisted continuously so each theme keeps its own colors across sessions.
//
// IMPORTANT: Clay serializes this function with toString() and injects only its
// body into the config page (a separate webview), so it CANNOT reference module
// scope. Every constant and helper must live INSIDE the exported function.

module.exports = function() {
  var clayConfig = this;

  // The accent color keys, in the order shown on the config page. Must match the
  // color items in config.js and the PERSIST_KEY_*_COLOR slots on the watch.
  var COLOR_KEYS = [
    'HOUR_MARKERS_COLOR',
    'MINUTE_MARKERS_COLOR',
    'CHARGING_MARKERS_COLOR',
    'HANDS_COLOR',
    'TIPS_COLOR',
    'SECOND_HAND_COLOR',
    'SECOND_TIP_COLOR',
    'CALENDAR_DAY_COLOR'
  ];

  // Per-theme default palettes (THEME select values: '0' = dark, '1' = light).
  // Dark mirrors the config.js / config.c defaults; light is tuned for contrast
  // against a white panel (yellow and bright green wash out on white).
  var PALETTES = {
    '0': {
      HOUR_MARKERS_COLOR: 'ffffff',
      MINUTE_MARKERS_COLOR: 'aaaaaa',
      CHARGING_MARKERS_COLOR: '00ff00',
      HANDS_COLOR: 'aaaaaa',
      TIPS_COLOR: 'ffffff',
      SECOND_HAND_COLOR: 'aa0000',
      SECOND_TIP_COLOR: 'ffff00',
      CALENDAR_DAY_COLOR: 'ffff00'
    },
    '1': {
      HOUR_MARKERS_COLOR: '000000',
      MINUTE_MARKERS_COLOR: '555555',
      CHARGING_MARKERS_COLOR: '00aa00',
      HANDS_COLOR: '555555',
      TIPS_COLOR: '000000',
      SECOND_HAND_COLOR: 'ff0000',
      SECOND_TIP_COLOR: 'ff5500',
      CALENDAR_DAY_COLOR: '0055aa'
    }
  };

  var STORAGE_PREFIX = 'crisp-palette-';

  // Some config webviews disable / throw on localStorage. Accessing it must
  // never abort AFTER_BUILD (that would skip every handler registration below
  // and leave the theme swap and reset dead), so wrap it and keep an in-memory
  // fallback that at least remembers palettes within the current config session.
  var memStore = {};

  function readStore(key) {
    try {
      var v = localStorage.getItem(key);
      if (v !== null && v !== undefined) {
        return v;
      }
    } catch (e) {
      // localStorage unavailable: fall through to the in-memory store.
    }
    return (key in memStore) ? memStore[key] : null;
  }

  function writeStore(key, value) {
    memStore[key] = value;
    try {
      localStorage.setItem(key, value);
    } catch (e) {
      // localStorage unavailable: the in-memory store already holds it.
    }
  }

  function storageKey(theme) {
    return STORAGE_PREFIX + theme;
  }

  // A fresh copy of a theme's default palette (so callers can mutate it freely).
  function defaultPalette(theme) {
    var defaults = PALETTES[theme] || PALETTES['0'];
    var copy = {};
    for (var i = 0; i < COLOR_KEYS.length; i++) {
      copy[COLOR_KEYS[i]] = defaults[COLOR_KEYS[i]];
    }
    return copy;
  }

  // The saved palette for a theme, falling back to its defaults.
  function loadPalette(theme) {
    var raw = readStore(storageKey(theme));
    if (raw) {
      try {
        return JSON.parse(raw);
      } catch (e) {
        // Corrupt entry: fall through to defaults.
      }
    }
    return defaultPalette(theme);
  }

  function savePalette(theme, palette) {
    writeStore(storageKey(theme), JSON.stringify(palette));
  }

  // The color manipulator's get() returns an integer, while the seed defaults
  // are hex strings. Normalize to a 6-digit lowercase hex string so saved
  // palettes and defaults share one format (set() accepts either, but a single
  // format keeps localStorage clean and the comparisons predictable).
  function toHex(value) {
    if (typeof value === 'string') {
      return value.replace(/^#|^0x/, '');
    }
    var s = (value >>> 0).toString(16);
    while (s.length < 6) {
      s = '0' + s;
    }
    return s;
  }

  // Read the current values out of the color pickers.
  function readPickers() {
    var palette = {};
    for (var i = 0; i < COLOR_KEYS.length; i++) {
      var item = clayConfig.getItemByMessageKey(COLOR_KEYS[i]);
      if (item) {
        palette[COLOR_KEYS[i]] = toHex(item.get());
      }
    }
    return palette;
  }

  // Push a palette into the color pickers.
  function writePickers(palette) {
    for (var i = 0; i < COLOR_KEYS.length; i++) {
      var item = clayConfig.getItemByMessageKey(COLOR_KEYS[i]);
      if (item && palette[COLOR_KEYS[i]] != null) {
        item.set(palette[COLOR_KEYS[i]]);
      }
    }
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var themeItem = clayConfig.getItemByMessageKey('THEME');
    if (!themeItem) {
      return;
    }

    var activeTheme = themeItem.get();
    // Guards the picker writes done by a theme swap so they do not get
    // mis-attributed as user edits of the new theme.
    var swapping = false;

    // Persist every manual color edit under the active theme.
    for (var i = 0; i < COLOR_KEYS.length; i++) {
      var colorItem = clayConfig.getItemByMessageKey(COLOR_KEYS[i]);
      if (!colorItem) {
        continue;
      }
      colorItem.on('change', function() {
        if (swapping) {
          return;
        }
        savePalette(activeTheme, readPickers());
      });
    }

    // On a theme switch, stash the old theme's colors and restore the new
    // theme's. The pickers now describe the new theme, so the submit (which
    // sends the picker values) hands the watch the matching palette.
    themeItem.on('change', function() {
      var newTheme = themeItem.get();
      if (newTheme === activeTheme) {
        return;
      }
      savePalette(activeTheme, readPickers());
      swapping = true;
      writePickers(loadPalette(newTheme));
      swapping = false;
      activeTheme = newTheme;
    });

    // Reset the pickers to the active theme's default palette and remember it.
    var resetButton = clayConfig.getItemById('resetColors');
    if (resetButton) {
      resetButton.on('click', function() {
        var defaults = defaultPalette(activeTheme);
        swapping = true;
        writePickers(defaults);
        swapping = false;
        savePalette(activeTheme, defaults);
      });
    }

    // Finally, show the palette saved for the theme the form opened on. Done
    // last so a failure here can never block the handlers registered above.
    swapping = true;
    writePickers(loadPalette(activeTheme));
    swapping = false;
  });
};
