import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';
import { randomUUID } from 'node:crypto';
import { WebSocketServer, WebSocket } from 'ws';
import { Game } from './game.ts';
import { TICK_MS, type ClientMsg } from '../shared/protocol.ts';

const PORT = Number(process.env.PORT ?? 3000);
const SEED = Number(process.env.SEED ?? 1337);
const DIST = join(import.meta.dirname, '..', 'dist');
const MIME: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript',
  '.css': 'text/css',
};

const game = new Game(SEED);

// Em produção o próprio servidor entrega o cliente compilado (npm run build).
const http = createServer(async (req, res) => {
  const url = new URL(req.url ?? '/', 'http://x');
  const path = normalize(url.pathname === '/' ? '/index.html' : url.pathname);
  try {
    const body = await readFile(join(DIST, path));
    res.writeHead(200, { 'content-type': MIME[extname(path)] ?? 'application/octet-stream' });
    res.end(body);
  } catch {
    res.writeHead(404).end('Rode "npm run build" ou use "npm run dev".');
  }
});

const wss = new WebSocketServer({ server: http, path: '/ws' });

wss.on('connection', (ws) => {
  const id = randomUUID();
  let joined = false;
  const send = (msg: unknown) => ws.readyState === WebSocket.OPEN && ws.send(JSON.stringify(msg));

  ws.on('message', (raw) => {
    let msg: ClientMsg;
    try {
      msg = JSON.parse(String(raw));
    } catch {
      return;
    }
    const p = game.players.get(id);
    if (msg.t === 'join' && !joined) {
      joined = true;
      game.addPlayer(id, String(msg.name ?? ''), send);
      send({ t: 'welcome', id, seed: SEED });
      console.log(`+ ${msg.name} entrou (${game.players.size} online)`);
    } else if (p && msg.t === 'input') {
      game.setInput(p, Number(msg.dx), Number(msg.dy));
    } else if (p && msg.t === 'sell') {
      game.sell(p);
    }
  });

  ws.on('close', () => {
    const p = game.players.get(id);
    if (p) console.log(`- ${p.name} saiu`);
    game.removePlayer(id);
  });
});

setInterval(() => game.step(), TICK_MS);

http.listen(PORT, () => console.log(`Servidor do jogo em http://localhost:${PORT} (seed ${SEED})`));
