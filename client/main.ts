import { GameMap, Tile, hash2, MAP_W, MAP_H, DOCK_X, DOCK_Y } from '../shared/map.ts';
import { SPECIES, holdValue } from '../shared/fish.ts';
import type { ClientMsg, ServerMsg, Objective } from '../shared/protocol.ts';

const VIEW_W = 81;
const VIEW_H = 41;

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
const screen = $<HTMLPreElement>('screen');

let ws: WebSocket;
let map: GameMap | null = null;
type State = Extract<ServerMsg, { t: 'state' }>;
let state: State | null = null;
let objectives: Objective[] = [];

const send = (msg: ClientMsg) => ws?.readyState === WebSocket.OPEN && ws.send(JSON.stringify(msg));
const esc = (s: string) => s.replace(/[&<>"]/g, (c) => `&#${c.charCodeAt(0)};`);

// ---------- Entrada ----------

$<HTMLFormElement>('join-form').addEventListener('submit', (e) => {
  e.preventDefault();
  const name = $<HTMLInputElement>('name').value.trim();
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  ws = new WebSocket(`${proto}://${location.host}/ws`);
  ws.onopen = () => send({ t: 'join', name });
  ws.onmessage = (ev) => onMessage(JSON.parse(ev.data));
  ws.onclose = () => {
    screen.textContent = '\n\n   Conexão perdida. Recarregue a página.';
  };
  $('join').hidden = true;
  $('game').hidden = false;
});

function onMessage(msg: ServerMsg) {
  if (msg.t === 'welcome') {
    map = new GameMap(msg.seed);
    buildMinimapBase(map);
  } else if (msg.t === 'state') {
    state = msg;
    if (msg.objectives) objectives = msg.objectives;
    render();
  }
}

// ---------- Teclado ----------

const held = new Set<string>();
const KEYS: Record<string, [number, number]> = {
  w: [0, -1], arrowup: [0, -1],
  s: [0, 1], arrowdown: [0, 1],
  a: [-1, 0], arrowleft: [-1, 0],
  d: [1, 0], arrowright: [1, 0],
};
let lastDir = '0,0';

function sendDirection() {
  let dx = 0, dy = 0;
  for (const k of held) {
    dx += KEYS[k][0];
    dy += KEYS[k][1];
  }
  dx = Math.sign(dx);
  dy = Math.sign(dy);
  const dir = `${dx},${dy}`;
  if (dir !== lastDir) {
    lastDir = dir;
    send({ t: 'input', dx, dy });
  }
}

addEventListener('keydown', (e) => {
  if ($('game').hidden) return;
  const k = e.key.toLowerCase();
  if (k in KEYS) {
    e.preventDefault();
    held.add(k);
    sendDirection();
  } else if (k === 'e') {
    send({ t: 'sell' });
  } else if (k === 'm') {
    $('minimap').classList.toggle('big');
  }
});
addEventListener('keyup', (e) => {
  const k = e.key.toLowerCase();
  if (held.delete(k)) sendDirection();
});
addEventListener('blur', () => {
  held.clear();
  sendDirection();
});

// ---------- Desenho ----------

const TILE_CLASS: Record<Tile, string> = {
  [Tile.Deep]: 'deep',
  [Tile.Water]: 'water',
  [Tile.Shallow]: 'shallow',
  [Tile.Sand]: 'sand',
  [Tile.Grass]: 'grass',
  [Tile.Forest]: 'forest',
  [Tile.Mountain]: 'mountain',
  [Tile.Dock]: 'dock',
};

function tileChar(t: Tile, x: number, y: number, tick: number): string {
  switch (t) {
    case Tile.Deep:
    case Tile.Water: {
      // Ondas que andam devagar para o mar parecer vivo.
      const wave = (x + Math.floor(tick / 4) + Math.floor(y / 2)) % 7;
      return wave === 0 ? '-' : '~';
    }
    case Tile.Shallow: return hash2(x, y, 7) < 0.15 ? '~' : '.';
    case Tile.Sand: return ':';
    case Tile.Grass: return '"';
    case Tile.Forest: return hash2(x, y, 3) < 0.5 ? '%' : '&';
    case Tile.Mountain: return '^';
    case Tile.Dock: return 'H';
  }
}

function render() {
  if (!map || !state) return;
  const { you, tick } = state;
  const ox = you.x - (VIEW_W >> 1), oy = you.y - (VIEW_H >> 1);
  const chars: string[] = new Array(VIEW_W * VIEW_H);
  const cls: string[] = new Array(VIEW_W * VIEW_H);

  for (let sy = 0; sy < VIEW_H; sy++) {
    for (let sx = 0; sx < VIEW_W; sx++) {
      const x = ox + sx, y = oy + sy, i = sy * VIEW_W + sx;
      const t = map.get(x, y);
      chars[i] = x < 0 || y < 0 || x >= MAP_W || y >= MAP_H ? ' ' : tileChar(t, x, y, tick);
      cls[i] = TILE_CLASS[t];
    }
  }

  const put = (x: number, y: number, ch: string, c: string) => {
    const sx = x - ox, sy = y - oy;
    if (sx < 0 || sy < 0 || sx >= VIEW_W || sy >= VIEW_H) return;
    chars[sy * VIEW_W + sx] = ch;
    cls[sy * VIEW_W + sx] = c;
  };
  const label = (x: number, y: number, text: string, c: string) => {
    const start = x - (text.length >> 1);
    for (let k = 0; k < text.length; k++) put(start + k, y, text[k], c);
  };

  // Cardumes: peixinhos >< piscando dentro do raio.
  for (const s of state.schools) {
    for (let y = s.y - s.r; y <= s.y + s.r; y++) {
      for (let x = s.x - s.r; x <= s.x + s.r; x++) {
        if (Math.hypot(x - s.x, y - s.y) > s.r) continue;
        const h = hash2(x + Math.floor(tick / 3), y, s.id);
        if (h < 0.45) put(x, y, h < 0.22 ? '>' : '<', 'school');
      }
    }
  }

  for (const p of state.players) {
    put(p.x, p.y, '@', `p${p.color}`);
    label(p.x, p.y - 1, p.name, 'name');
  }
  put(you.x, you.y, '@', 'me-boat');

  // Junta caracteres vizinhos de mesma cor em um só <span>.
  let html = '';
  for (let sy = 0; sy < VIEW_H; sy++) {
    let run = '', runCls = '';
    for (let sx = 0; sx < VIEW_W; sx++) {
      const i = sy * VIEW_W + sx;
      if (cls[i] !== runCls) {
        if (run) html += `<span class="${runCls}">${esc(run)}</span>`;
        run = '';
        runCls = cls[i];
      }
      run += chars[i];
    }
    html += `<span class="${runCls}">${esc(run)}</span>\n`;
  }
  screen.innerHTML = html;
  renderHud(state);
  renderMinimap(state);
}

// ---------- Minimapa ----------

const minimap = $<HTMLCanvasElement>('minimap');
const mctx = minimap.getContext('2d')!;
const MM_SCALE = minimap.width / MAP_W; // 1 pixel = 2 células
let minimapBase: HTMLCanvasElement | null = null;

const MM_COLORS: Record<Tile, string> = {
  [Tile.Deep]: '#0b1f3d',
  [Tile.Water]: '#163f6e',
  [Tile.Shallow]: '#2c7a97',
  [Tile.Sand]: '#c9b77e',
  [Tile.Grass]: '#4f9a4e',
  [Tile.Forest]: '#2a6b3c',
  [Tile.Mountain]: '#8a949e',
  [Tile.Dock]: '#c08552',
};

// O terreno não muda, então é desenhado uma vez só numa imagem de fundo.
function buildMinimapBase(m: GameMap) {
  const c = document.createElement('canvas');
  c.width = minimap.width;
  c.height = minimap.height;
  const ctx = c.getContext('2d')!;
  const img = ctx.createImageData(c.width, c.height);
  const step = 1 / MM_SCALE;
  for (let py = 0; py < c.height; py++) {
    for (let px = 0; px < c.width; px++) {
      // Se houver terra em qualquer célula do bloco, pinta terra (ilhas pequenas não somem).
      let t = m.get(Math.floor(px * step), Math.floor(py * step));
      for (let k = 1; k < step * step && t <= Tile.Shallow; k++) {
        const o = m.get(Math.floor(px * step) + (k % step), Math.floor(py * step) + Math.floor(k / step));
        if (o > t) t = o;
      }
      const hex = MM_COLORS[t];
      const i = (py * c.width + px) * 4;
      img.data[i] = parseInt(hex.slice(1, 3), 16);
      img.data[i + 1] = parseInt(hex.slice(3, 5), 16);
      img.data[i + 2] = parseInt(hex.slice(5, 7), 16);
      img.data[i + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  minimapBase = c;
}

const DIRS = ['L', 'SE', 'S', 'SO', 'O', 'NO', 'N', 'NE'];
const compass = (dx: number, dy: number) => DIRS[(Math.round(Math.atan2(dy, dx) / (Math.PI / 4)) + 8) % 8];

function renderMinimap(s: State) {
  if (!minimapBase) return;
  const { you } = s;
  const S = MM_SCALE;
  mctx.drawImage(minimapBase, 0, 0);

  // Área que aparece na tela principal.
  mctx.strokeStyle = 'rgba(255,255,255,0.35)';
  mctx.lineWidth = 1;
  mctx.strokeRect((you.x - (VIEW_W >> 1)) * S + 0.5, (you.y - (VIEW_H >> 1)) * S + 0.5, VIEW_W * S, VIEW_H * S);

  // Cardumes piscando devagar.
  const pulse = 0.55 + 0.45 * Math.sin(s.tick / 4);
  mctx.fillStyle = `rgba(125,249,255,${pulse})`;
  for (const o of objectives) {
    mctx.beginPath();
    mctx.arc(o.x * S, o.y * S, 2.2, 0, Math.PI * 2);
    mctx.fill();
  }

  // Porto (lojista).
  mctx.fillStyle = '#c08552';
  mctx.fillRect(DOCK_X * S - 3, DOCK_Y * S - 3, 6, 6);
  mctx.strokeStyle = '#fff';
  mctx.strokeRect(DOCK_X * S - 3.5, DOCK_Y * S - 3.5, 7, 7);

  // Outros jogadores (só os que estão por perto).
  mctx.fillStyle = '#ff6b6b';
  for (const p of s.players) mctx.fillRect(p.x * S - 1.5, p.y * S - 1.5, 3, 3);

  // Você.
  mctx.fillStyle = '#ffd166';
  mctx.beginPath();
  mctx.arc(you.x * S, you.y * S, 3, 0, Math.PI * 2);
  mctx.fill();
  mctx.strokeStyle = '#000';
  mctx.stroke();

  // Objetivo mais próximo: porto se o porão está cheio, senão o cardume mais perto.
  let target: { x: number; y: number; label: string } | null = null;
  if (you.holdCount >= you.capacity) {
    target = { x: DOCK_X, y: DOCK_Y, label: 'Porto (porão cheio)' };
  } else {
    let best = Infinity;
    for (const o of objectives) {
      const d = Math.hypot(o.x - you.x, o.y - you.y);
      if (d < best) {
        best = d;
        target = { x: o.x, y: o.y, label: `Cardume de ${SPECIES[o.species].name}` };
      }
    }
  }
  const tip = $('hud-target');
  if (!target) {
    tip.textContent = '';
    return;
  }
  const dx = target.x - you.x, dy = target.y - you.y, dist = Math.round(Math.hypot(dx, dy));
  tip.textContent = dist < 2 ? `${target.label}: você chegou!` : `${target.label}: ${dist} células a ${compass(dx, dy)}`;
  mctx.strokeStyle = '#ffd166';
  mctx.setLineDash([3, 3]);
  mctx.beginPath();
  mctx.moveTo(you.x * S, you.y * S);
  mctx.lineTo(target.x * S, target.y * S);
  mctx.stroke();
  mctx.setLineDash([]);
}

function renderHud(s: State) {
  const { you } = s;
  $('hud-name').textContent = `⚓ ${you.name}`;
  $('hud-stats').innerHTML = `
    <div class="row"><span>Dinheiro</span><b>$${you.money}</b></div>
    <div class="row"><span>Posição</span><span>${you.x}, ${you.y}</span></div>
    <div class="row"><span>Velocidade</span><span>${you.speed} nós</span></div>
    <div class="row"><span>Online</span><span>${s.online}</span></div>
    ${you.atDock ? '<div class="dock-tip">No cais: aperte E para vender</div>' : ''}`;

  const hold = Object.entries(you.hold)
    .sort((a, b) => SPECIES[b[0]].price - SPECIES[a[0]].price)
    .map(([id, n]) => `<div class="row"><span>${SPECIES[id].name}</span><span>${n}</span></div>`)
    .join('');
  $('hud-hold').innerHTML = `
    ${hold || '<div class="row"><span>vazio</span></div>'}
    <div class="row" style="margin-top:6px"><span>${you.holdCount}/${you.capacity}</span><span>vale $${holdValue(you.hold)}</span></div>`;

  $('hud-rank').innerHTML = s.rank
    .map((r) => `<li class="${r.name === you.name ? 'me' : ''}"><div class="row"><span>${esc(r.name)}</span><span>$${r.money}</span></div></li>`)
    .join('');
  $('hud-log').innerHTML = s.log.map((l) => `<div>${esc(l)}</div>`).join('');
}
