// Phone-side glue. Clay renders the configuration page, persists the chosen
// values and, on close, transmits every messageKey to the watch automatically
// (toggles as 0/1, colors as 0xRRGGBB, selects as their value). The watch maps
// each key onto its persisted setting in src/c/comm.c.
//
// On top of Clay this also relays weather: it geolocates the phone, queries the
// free, key-less Open-Meteo API for today's low/high temperature and the
// current hour's precipitation probability, and sends them to the watch so the
// corner readouts (Weather / Rain chance) can display them.
var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

// --- Weather tunables -----------------------------------------------------
var WEATHER_REFRESH_MS = 30 * 60 * 1000;  // re-fetch every 30 minutes
var GEO_TIMEOUT_MS = 15000;
var GEO_MAX_AGE_MS = 10 * 60 * 1000;      // accept a cached fix up to 10 min old
var WEATHER_BASE_URL = 'https://api.open-meteo.com/v1/forecast';

function sendWeather(tminC, tmaxC, precipPct) {
  Pebble.sendAppMessage(
    {
      'WEATHER_TEMP_MIN': tminC,
      'WEATHER_TEMP_MAX': tmaxC,
      'WEATHER_PRECIP': precipPct
    },
    function() {
      console.log('Weather sent: ' + tminC + '/' + tmaxC + 'C ' + precipPct + '%');
    },
    function(e) { console.log('Weather send failed: ' + JSON.stringify(e)); }
  );
}

function parseWeather(body) {
  var data = JSON.parse(body);

  // Today's forecast low/high (forecast_days=1 -> single daily entry).
  var tminC = Math.round(data.daily.temperature_2m_min[0]);
  var tmaxC = Math.round(data.daily.temperature_2m_max[0]);

  // Precipitation probability is hourly only; pick the entry for the current
  // hour. current.time looks like "2026-06-05T21:45"; hourly.time entries are
  // "2026-06-05T21:00", so match on the "...THH" prefix.
  var precipPct = 0;
  var nowHour = data.current.time.slice(0, 13);
  var idx = data.hourly.time.indexOf(nowHour + ':00');
  if (idx >= 0) {
    precipPct = data.hourly.precipitation_probability[idx];
  }

  sendWeather(tminC, tmaxC, precipPct);
}

function requestWeather(lat, lon) {
  var url = WEATHER_BASE_URL +
    '?latitude=' + lat +
    '&longitude=' + lon +
    '&current=temperature_2m' +
    '&hourly=precipitation_probability' +
    '&daily=temperature_2m_max,temperature_2m_min' +
    '&timezone=auto&forecast_days=1';

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.onload = function() {
    try {
      parseWeather(xhr.responseText);
    } catch (e) {
      console.log('Weather parse error: ' + e);
    }
  };
  xhr.onerror = function() { console.log('Weather request failed'); };
  xhr.send();
}

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) { requestWeather(pos.coords.latitude, pos.coords.longitude); },
    function(err) { console.log('Geolocation error: ' + err.message); },
    { timeout: GEO_TIMEOUT_MS, maximumAge: GEO_MAX_AGE_MS }
  );
}

Pebble.addEventListener('ready', function() {
  console.log('Crisp PebbleKit JS ready');
  fetchWeather();
  setInterval(fetchWeather, WEATHER_REFRESH_MS);
});
