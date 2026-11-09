(() => {
  'use strict';

  const STREET_STYLE = 'https://tiles.openfreemap.org/styles/positron';
  const PLAIN_STYLE = { version: 8, sources: {}, layers: [{ id: 'bg', type: 'background', paint: { 'background-color': '#f6f5f1' } }] };
  const FLORIDA = [[-87.6, 24.4], [-79.9, 31.1]];

  const RGB = {
    open: [18, 160, 115], closed: [138, 144, 153], flood: [214, 69, 61],
    route: [14, 31, 58], ink: [22, 25, 29], white: [255, 255, 255],
  };
  const ALGOS = [
    { name: 'Dijkstra', rgb: [42, 120, 214], css: 'var(--dijkstra)' },
    { name: 'A*', rgb: [235, 104, 52], css: 'var(--astar)' },
  ];

  function $(id) {
    return document.getElementById(id);
  }
  function fmt(n) {
    return Math.round(n).toLocaleString('en-US');
  }
  function ms(value) {
    return (value < 1 ? value.toFixed(3) : value.toFixed(1)) + ' ms';
  }
  function titleCase(s) {
    return s.toLowerCase().replace(/\b[a-z]/g, (c) => c.toUpperCase());
  }

  const state = {
    nodes: null, shelters: [], points: [], shelterState: [], zones: [], closedArcs: 0, openShelters: 0,
    tab: 'route', goal: 'nearest', origin: null, dest: null, waitingForDest: false,
    route: null, explored: [null, null], showExplored: 'none',
    drawFlood: false, floodKm: 3, online: false, roads: null, closed: null, busy: false,
    race: { playing: false, shown: 0, speed: Math.pow(10, 5.2), last: 0, frame: null },
  };
  const maps = [null, null];

  async function api(path, method = 'GET') {
    const r = await fetch(path, { method });
    const body = await r.json();
    if (!r.ok) throw new Error(body.error);
    return body;
  }
  async function bin(path) {
    const r = await fetch(path);
    if (!r.ok) throw new Error((await r.json()).error);
    return r.arrayBuffer();
  }

  // Road endpoints are pairs of node ids.
  function lineData(buf) {
    const pairs = new Uint32Array(buf);
    const count = pairs.length / 2;
    const source = new Float32Array(count * 2);
    const target = new Float32Array(count * 2);
    for (let k = 0; k < count; k++) {
      const u = pairs[2 * k];
      const v = pairs[2 * k + 1];
      source[2 * k] = state.nodes[2 * u];
      source[2 * k + 1] = state.nodes[2 * u + 1];
      target[2 * k] = state.nodes[2 * v];
      target[2 * k + 1] = state.nodes[2 * v + 1];
    }
    return {
      length: count,
      attributes: {
        getSourcePosition: { value: source, size: 2 },
        getTargetPosition: { value: target, size: 2 },
      },
    };
  }

  // Points are stored in visit order.
  function exploredData(buf) {
    const order = new Uint32Array(buf);
    const pos = new Float32Array(order.length * 2);
    for (let k = 0; k < order.length; k++) {
      pos[2 * k] = state.nodes[2 * order[k]];
      pos[2 * k + 1] = state.nodes[2 * order[k] + 1];
    }
    return { length: order.length, attributes: { getPosition: { value: pos, size: 2 } } };
  }

  function makeMap(i) {
    const map = new maplibregl.Map({
      container: `map-${i}`, style: state.online ? STREET_STYLE : PLAIN_STYLE,
      bounds: FLORIDA, dragRotate: false, attributionControl: { compact: true },
    });
    map.addControl(new maplibregl.NavigationControl({ showCompass: false }), 'top-right');
    const overlay = new deck.MapboxOverlay({ layers: [], getTooltip });
    map.addControl(overlay);
    map.on('click', (e) => onMapClick(i, e));
    return { map, overlay };
  }

  function layers(pane) {
    const mapLayers = [];
    const tab = state.tab;
    if (!state.online && state.roads) {
      mapLayers.push(new deck.LineLayer({ id: 'roads', data: state.roads, getColor: [96, 104, 114, 110], getWidth: 0.7 }));
    }
    if (state.closed) {
      mapLayers.push(new deck.LineLayer({ id: 'closed', data: state.closed, getColor: RGB.flood, getWidth: 1.8 }));
    }
    mapLayers.push(new deck.ScatterplotLayer({
      id: 'flood', data: state.zones, getPosition: (z) => [z.lon, z.lat], getRadius: (z) => z.km * 1000,
      getFillColor: [...RGB.flood, 45], getLineColor: RGB.flood, stroked: true, lineWidthUnits: 'pixels', getLineWidth: 1.5,
    }));

    let algo = -1;
    if (tab === 'race') algo = pane;
    else if (tab === 'route' && state.showExplored !== 'none') algo = Number(state.showExplored);
    if (algo >= 0 && state.explored[algo]) {
      const ex = state.explored[algo];
      const shown = tab === 'race' ? state.race.shown : ex.length;
      mapLayers.push(new deck.ScatterplotLayer({
        id: `explored-${algo}`,
        data: { length: Math.min(ex.length, Math.round(shown)), attributes: ex.attributes },
        radiusUnits: 'pixels', getRadius: ex.length < 20000 ? 2.6 : 1.6,
        getFillColor: [...ALGOS[algo].rgb, 90],
      }));
    }

    mapLayers.push(new deck.ScatterplotLayer({
      id: 'shelters', data: state.points, pickable: true,
      getPosition: (s) => [s.lon, s.lat], getFillColor: (s) => shelterColor(state.shelterState[s.i]),
      getLineColor: RGB.white, stroked: true, lineWidthUnits: 'pixels', getLineWidth: 1,
      getRadius: 250, radiusMinPixels: 2.2, radiusMaxPixels: 7, updateTriggers: { getFillColor: state.shelterState },
    }));

    const res = state.route && state.route.results[tab === 'race' ? pane : 0];
    const raceDone = tab !== 'race' || (state.explored[pane] && state.race.shown >= state.explored[pane].length);
    if (res && res.found && raceDone) {
      const data = [{ path: res.path }];
      mapLayers.push(new deck.PathLayer({ id: 'route-edge', data, getPath: (d) => d.path, getColor: RGB.white, getWidth: 9, widthUnits: 'pixels', capRounded: true, jointRounded: true }));
      mapLayers.push(new deck.PathLayer({ id: 'route', data, getPath: (d) => d.path, getColor: RGB.route, getWidth: 4.5, widthUnits: 'pixels', capRounded: true, jointRounded: true }));
      if (!state.route.dest) {
        mapLayers.push(new deck.ScatterplotLayer({
          id: 'target', data: [res.target], getPosition: (d) => [d.lon, d.lat], getRadius: 11, radiusUnits: 'pixels',
          filled: false, stroked: true, getLineColor: RGB.open, lineWidthUnits: 'pixels', getLineWidth: 3,
        }));
      }
    }
    const marks = [];
    if (state.origin) marks.push({ ...(state.route ? state.route.origin : state.origin), start: true });
    if (state.dest) marks.push({ ...(state.route && state.route.dest ? state.route.dest : state.dest), start: false });
    mapLayers.push(new deck.ScatterplotLayer({
      id: 'markers', data: marks, getPosition: (d) => [d.lon, d.lat], radiusUnits: 'pixels', getRadius: 7,
      getFillColor: (d) => (d.start ? RGB.ink : RGB.white), getLineColor: (d) => (d.start ? RGB.white : RGB.ink),
      stroked: true, lineWidthUnits: 'pixels', getLineWidth: 3,
    }));
    return mapLayers;
  }

  function shelterColor(status) {
    if (status === 'open') return RGB.open;
    if (status === 'flooded') return RGB.flood;
    return RGB.closed;
  }

  function getTooltip(info) {
    if (info.layer && info.layer.id === 'shelters' && info.object) {
      const s = info.object;
      const st = state.shelterState[s.i];
      const cap = s.cap > 0 ? `capacity ${fmt(s.cap)}` : 'capacity not listed';
      let status = 'Inside a flood zone';
      if (st === 'open') status = 'Open. Click to close.';
      else if (st === 'closed') status = 'Closed. Click to open.';
      return { text: `${titleCase(s.name)}\n${titleCase(s.city)}, ${titleCase(s.county)} County, ${cap}\n${status}` };
    }
    return null;
  }

  function render() {
    maps[0].overlay.setProps({ layers: layers(0) });
    if (maps[1]) maps[1].overlay.setProps({ layers: state.tab === 'race' ? layers(1) : [] });
    renderBadges();
  }

  async function act(fn) {
    if (state.busy) return;
    state.busy = true;
    const controls = document.querySelectorAll('.tabs button, #goal button, #flood-draw, #flood-km, #reset');
    for (const control of controls) control.disabled = true;
    showError('');
    try {
      await fn();
    } catch (err) {
      showError(err.message);
    } finally {
      state.busy = false;
      for (const control of controls) control.disabled = false;
    }
  }
  function showError(text) {
    $('error').textContent = text;
    $('error').hidden = !text;
  }

  function onMapClick(pane, e) {
    const p = { lat: e.lngLat.lat, lon: e.lngLat.lng };
    if (state.drawFlood) return act(() => scenario(`/api/flood?lat=${p.lat}&lon=${p.lon}&km=${state.floodKm}`));
    const hit = maps[pane].overlay.pickObject({ x: e.point.x, y: e.point.y, radius: 3, layerIds: ['shelters'] });
    if (state.goal === 'pick' && state.waitingForDest) {
      return act(() => {
        state.dest = hit ? { lat: hit.object.lat, lon: hit.object.lon } : p;
        state.waitingForDest = false;
        renderHint();
        return runRoute();
      });
    }
    if (hit) return act(() => toggleShelter(hit.object.i));
    act(() => {
      state.origin = p;
      state.dest = null;
      state.waitingForDest = state.goal === 'pick';
      renderHint();
      return runRoute();
    });
  }

  function clearRoute() {
    state.route = null;
    state.explored = [null, null];
    stopRace();
    state.race.shown = 0;
    renderResult();
  }

  async function runRoute() {
    clearRoute();
    render();
    try {
      if (state.origin && (state.goal === 'nearest' || state.dest)) {
        let q = `/api/route?from=${state.origin.lat},${state.origin.lon}`;
        if (state.dest) q += `&to=${state.dest.lat},${state.dest.lon}`;
        const route = await api(q);
        const ex = await Promise.all([
          bin('/api/explored?algo=0'),
          bin('/api/explored?algo=1'),
        ]);
        state.route = route;
        state.explored = [exploredData(ex[0]), exploredData(ex[1])];
        zoomToRoute();
      }
    } finally {
      renderResult();
      startRace();
    }
  }

  function zoomToRoute() {
    let west = state.route.origin.lon;
    let east = west;
    let south = state.route.origin.lat;
    let north = south;
    for (const result of state.route.results) {
      for (const point of result.path) {
        west = Math.min(west, point[0]);
        east = Math.max(east, point[0]);
        south = Math.min(south, point[1]);
        north = Math.max(north, point[1]);
      }
    }
    const box = [[west, south], [east, north]];
    const view = maps[0].map.getBounds();
    const small = box[1][0] - box[0][0] < 0.12 * (view.getEast() - view.getWest());
    if (small || !view.contains(box[0]) || !view.contains(box[1])) {
      maps[0].map.fitBounds(box, { padding: 90, maxZoom: 14, duration: 700 });
    }
  }

  async function toggleShelter(i) {
    if (state.shelterState[i] === 'flooded') throw new Error('This shelter is flooded. Reset clears the flood zones.');
    await scenario(`/api/shelter?i=${i}&open=${state.shelterState[i] === 'open' ? 0 : 1}`);
  }

  async function scenario(path) {
    const sc = await api(path, 'POST');
    state.shelterState = sc.state;
    state.zones = sc.zones;
    state.closedArcs = sc.closedArcs;
    state.openShelters = sc.openShelters;
    clearRoute();
    state.closed = null;
    renderStorm();
    render();
    if (sc.zones.length) state.closed = lineData(await bin('/api/closed.bin'));
    render();
    await runRoute();
  }

  function startRace() {
    state.race.shown = 0;
    playRace();
    render();
  }
  function stopRace() {
    if (state.race.frame !== null) cancelAnimationFrame(state.race.frame);
    state.race.frame = null;
    state.race.playing = false;
    $('race-play').textContent = 'Play';
  }
  function playRace() {
    stopRace();
    if (state.tab !== 'race' || !state.route || raceTotal() === 0) return;
    state.race.playing = true;
    state.race.last = performance.now();
    $('race-play').textContent = 'Pause';
    state.race.frame = requestAnimationFrame(tick);
  }
  function raceTotal() {
    let total = 0;
    for (const explored of state.explored) {
      if (explored && explored.length > total) total = explored.length;
    }
    return total;
  }
  function tick(t) {
    state.race.frame = null;
    if (!state.race.playing) return;
    state.race.shown += state.race.speed * (t - state.race.last) / 1000;
    state.race.last = t;
    if (state.race.shown >= raceTotal()) {
      state.race.shown = raceTotal();
      stopRace();
    }
    render();
    if (state.race.playing) state.race.frame = requestAnimationFrame(tick);
  }
  function renderBadges() {
    for (let i = 0; i < 2; i++) {
      $(`badge-${i}`).hidden = state.tab !== 'race';
      let body = 'Set a route to start the race.';
      if (state.route) {
        const total = state.explored[i].length;
        body = `Nodes: ${fmt(Math.min(state.race.shown, total))} / ${fmt(total)}<br>Search time: ${ms(state.route.results[i].ms)}`;
      }
      $(`badge-${i}`).innerHTML = `<div class="badge-title"><span class="dot" style="background:${ALGOS[i].css}"></span>${ALGOS[i].name}</div>${body}`;
    }
  }

  function renderHint() {
    let h = 'Click a starting point.';
    if (state.drawFlood) h = `Click to add a ${state.floodKm} km flood zone and close the roads through it.`;
    else if (state.goal === 'pick') h = state.waitingForDest ? 'Click the destination.' : 'Click the start, then the destination.';
    $('hint').textContent = h;
  }

  function renderResult() {
    const r = state.route;
    $('result-empty').hidden = r !== null;
    $('result').hidden = r === null;
    $('race-stats').innerHTML = '';
    if (!r) return;
    const [d, a] = r.results;
    if (!d.found) {
      $('r-kind').textContent = 'No route';
      $('r-name').textContent = r.dest ? 'The destination cannot be reached' : 'No open shelter can be reached';
      $('r-sub').textContent = r.dest ? 'No open road connects the start to it.' : 'No open shelter is connected to the start by open roads.';
      $('r-km').textContent = '-';
    } else if (r.dest) {
      $('r-kind').textContent = 'Route to destination';
      $('r-name').textContent = 'Your destination';
      $('r-sub').textContent = `Intersection ${fmt(r.dest.node)}`;
      $('r-km').textContent = d.km.toFixed(1);
    } else {
      const s = state.shelters[d.shelter];
      $('r-kind').textContent = 'Nearest open shelter';
      $('r-name').textContent = titleCase(s.name);
      $('r-sub').textContent = `${titleCase(s.city)}, ${titleCase(s.county)} County, ` + (s.cap > 0 ? `capacity ${fmt(s.cap)}` : 'capacity not listed');
      $('r-km').textContent = d.km.toFixed(d.km < 10 ? 2 : 1);
    }
    let rows = '';
    for (let i = 0; i < r.results.length; i++) {
      const result = r.results[i];
      rows += `<tr><td><span class="dot" style="background:${ALGOS[i].css}"></span>${ALGOS[i].name}</td>`;
      rows += `<td>${fmt(result.settled)}</td>`;
      rows += `<td class="num">${ms(result.ms)}</td></tr>`;
    }
    $('r-rows').innerHTML = rows;
    if (d.found) {
      $('r-note').textContent = `Route cost: ${fmt(d.distance)} graph units.`;
      $('race-stats').textContent = `Dijkstra: ${fmt(d.settled)} nodes, ${ms(d.ms)}. A*: ${fmt(a.settled)} nodes, ${ms(a.ms)}.`;
    } else {
      $('r-note').textContent = 'No route found.';
    }
  }

  function renderStorm() {
    $('scenario-stats').textContent = (state.zones.length ? `${state.zones.length} flood zone(s), ${fmt(state.closedArcs)} arcs closed. ` : '') +
      `${fmt(state.openShelters)} shelters open. Click a shelter on the map to close or open it.`;
  }

  function setTab(tab) {
    state.tab = tab;
    for (const button of document.querySelectorAll('.tabs button')) {
      button.classList.toggle('active', button.dataset.tab === tab);
    }
    for (const section of document.querySelectorAll('.tab-body')) {
      section.hidden = section.dataset.for !== tab;
    }
    $('pane-1').hidden = tab !== 'race';
    if (tab === 'race' && !maps[1]) {
      maps[1] = makeMap(1);
      let busy = false;
      function follow(a, b) {
        a.on('move', () => {
          if (busy) return;
          busy = true;
          b.jumpTo({ center: a.getCenter(), zoom: a.getZoom() });
          busy = false;
        });
      }
      follow(maps[0].map, maps[1].map);
      follow(maps[1].map, maps[0].map);
    }
    requestAnimationFrame(() => {
      maps[0].map.resize();
      if (maps[1]) {
        maps[1].map.resize();
        maps[1].map.jumpTo({ center: maps[0].map.getCenter(), zoom: maps[0].map.getZoom() });
      }
    });
    startRace();
  }

  function wire() {
    $('loading-retry').addEventListener('click', () => window.location.reload());
    document.querySelectorAll('.tabs button').forEach((b) => b.addEventListener('click', () => setTab(b.dataset.tab)));
    document.querySelectorAll('#goal button').forEach((b) => b.addEventListener('click', () => act(() => {
      state.goal = b.dataset.goal;
      document.querySelectorAll('#goal button').forEach((x) => x.classList.toggle('active', x === b));
      state.dest = null;
      state.waitingForDest = state.goal === 'pick' && state.origin !== null;
      renderHint();
      return runRoute();
    })));
    document.querySelectorAll('#explored button').forEach((b) => b.addEventListener('click', () => {
      state.showExplored = b.dataset.show;
      document.querySelectorAll('#explored button').forEach((x) => x.classList.toggle('active', x === b));
      render();
    }));
    $('flood-draw').addEventListener('click', () => {
      state.drawFlood = !state.drawFlood;
      $('flood-draw').classList.toggle('on', state.drawFlood);
      $('flood-draw').textContent = state.drawFlood ? 'Cancel flood zone' : 'Add flood zone';
      renderHint();
    });
    $('flood-km').addEventListener('input', (e) => {
      state.floodKm = +e.target.value;
      $('flood-km-out').textContent = `${state.floodKm} km`;
      renderHint();
    });
    $('reset').addEventListener('click', () => act(() => scenario('/api/reset')));
    $('race-play').addEventListener('click', () => {
      if (!state.route) return;
      if (state.race.playing) stopRace();
      else {
        if (state.race.shown >= raceTotal()) state.race.shown = 0;
        playRace();
      }
      render();
    });
    $('race-restart').addEventListener('click', startRace);
    $('race-finish').addEventListener('click', () => {
      stopRace();
      state.race.shown = raceTotal();
      render();
    });
    $('race-speed').addEventListener('input', (e) => {
      state.race.speed = Math.pow(10, +e.target.value);
      $('race-speed-out').textContent = `${Math.round(state.race.speed / 1000)}k/s`;
    });
  }

  async function start() {
    wire();
    const [info, nodes, shelters] = await Promise.all([api('/api/info'), bin('/api/nodes.bin'), api('/api/shelters')]);
    state.nodes = new Float32Array(nodes);
    state.shelterState = info.scenario.state;
    state.shelters = shelters;
    for (let i = 0; i < shelters.length; i++) {
      shelters[i].i = i;
      if (state.shelterState[i] !== 'off_network') state.points.push(shelters[i]);
    }
    state.zones = info.scenario.zones;
    state.closedArcs = info.scenario.closedArcs;
    state.openShelters = info.scenario.openShelters;
    if (state.zones.length) state.closed = lineData(await bin('/api/closed.bin'));
    try {
      const response = await fetch(STREET_STYLE, { signal: AbortSignal.timeout(4000) });
      state.online = response.ok;
    } catch {
      state.online = false;
    }
    if (!state.online) state.roads = lineData(await bin('/api/roads.bin'));
    maps[0] = makeMap(0);
    $('status').textContent = `${fmt(info.nodes)} nodes, ${fmt(info.arcs)} directed edges. ` +
      (state.online ? 'Street map background. Routes use the DIMACS graph.' : 'Offline map: DIMACS road graph.');
    renderStorm();
    renderHint();
    renderResult();
    render();
    $('loading').hidden = true;
  }

  start().catch((err) => {
    $('loading-message').textContent = `Could not load the map: ${err.message}. Check that the server is running, then reload.`;
    $('loading-retry').hidden = false;
  });
})();
