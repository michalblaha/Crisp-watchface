// Clay configuration page for Crisp.
// Each item's messageKey matches a key declared in package.json; Clay sends
// toggles as 0/1, colors as 0xRRGGBB integers (ready for GColorFromHEX on the
// watch) and selects as their string value. Defaults mirror set defaults in
// src/c/config.c.

// Corner content choices. The value is parsed back into the CORNER_* enum on
// the watch (see store_corner in src/c/comm.c), so these must stay in sync.
var CORNER_OPTIONS = [
  { "label": "Nothing", "value": "0" },
  { "label": "Battery", "value": "1" },
  { "label": "Bluetooth", "value": "2" },
  { "label": "Heart rate", "value": "3" },
  { "label": "Steps", "value": "4" },
  { "label": "Weather (low/high)", "value": "5" },
  { "label": "Rain chance", "value": "6" }
];

// Default content per corner, matching set_default_config on the watch.
function cornerItem(messageKey, label, defaultValue) {
  return {
    "type": "select",
    "messageKey": messageKey,
    "label": label,
    "defaultValue": defaultValue,
    "options": CORNER_OPTIONS
  };
}

module.exports = [
  {
    "type": "heading",
    "defaultValue": "Crisp"
  },
  {
    "type": "text",
    "defaultValue": "Customizable analog watchface with splashes of color."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Display"
      },
      {
        "type": "select",
        "messageKey": "THEME",
        "label": "Theme",
        "description": "Light inverts the background and text. Each theme remembers its own accent colors (set below).",
        "defaultValue": "0",
        "options": [
          { "label": "Dark", "value": "0" },
          { "label": "Light", "value": "1" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "DATE",
        "label": "Show weekday & month",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "DAY",
        "label": "Show day of month",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SECOND_HAND",
        "label": "Show second hand",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "HR_INDICATOR",
        "label": "Show heart rate indicator",
        "description": "Shows a heart above the center while the watch measures your heart rate; two hearts during a workout.",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "BATTERY",
        "label": "Show battery on hour markers",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "BT",
        "label": "Show disconnection indicator",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Corners"
      },
      {
        "type": "text",
        "defaultValue": "Pick what each screen corner shows. Weather and rain chance need the Pebble app connected and location access."
      },
      cornerItem("CORNER_TL", "Top left", "1"),
      cornerItem("CORNER_TR", "Top right", "3"),
      cornerItem("CORNER_BL", "Bottom left", "4"),
      cornerItem("CORNER_BR", "Bottom right", "5"),
      {
        "type": "select",
        "messageKey": "TEMP_UNIT",
        "label": "Temperature unit",
        "description": "Automatic follows your watch's measurement system (metric/imperial).",
        "defaultValue": "0",
        "options": [
          { "label": "Automatic (watch units)", "value": "0" },
          { "label": "Celsius", "value": "1" },
          { "label": "Fahrenheit", "value": "2" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Colors"
      },
      {
        "type": "text",
        "defaultValue": "These accents are remembered per theme: switching the theme above loads that theme's own colors."
      },
      {
        "type": "color",
        "messageKey": "HOUR_MARKERS_COLOR",
        "label": "Hour markers",
        "defaultValue": "ffffff",
        "sunlight": false
      },
      {
        "type": "color",
        "messageKey": "MINUTE_MARKERS_COLOR",
        "label": "Minute markers",
        "defaultValue": "aaaaaa",
        "sunlight": false
      },
      {
        "type": "color",
        "messageKey": "CHARGING_MARKERS_COLOR",
        "label": "Charging markers",
        "defaultValue": "00ff00",
        "sunlight": false
      },
      {
        "type": "color",
        "messageKey": "HANDS_COLOR",
        "label": "Hour & minute hands",
        "defaultValue": "aaaaaa",
        "sunlight": false
      },
      {
        "type": "color",
        "messageKey": "TIPS_COLOR",
        "label": "Hand tips & center",
        "defaultValue": "ffffff",
        "sunlight": false
      },
      {
        "type": "color",
        "messageKey": "SECOND_HAND_COLOR",
        "label": "Second hand",
        "defaultValue": "aa0000",
        "sunlight": false
      },
      {
        "type": "color",
        "messageKey": "SECOND_TIP_COLOR",
        "label": "Second hand tip",
        "defaultValue": "ffff00",
        "sunlight": false
      },
      {
        "type": "color",
        "messageKey": "CALENDAR_DAY_COLOR",
        "label": "Day of month",
        "defaultValue": "ffff00",
        "sunlight": false
      },
      {
        // Resets the pickers above to the current theme's default palette
        // (handled in custom-clay.js; no messageKey, nothing sent on its own).
        "type": "button",
        "id": "resetColors",
        "defaultValue": "Reset colors to theme defaults",
        "primary": false
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  },
  {
    // Shown under the Save button. Keep in sync with "version" in package.json.
    "type": "text",
    "defaultValue": "Crisp v1.3.0"
  }
];
