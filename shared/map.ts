// Geração procedural do oceano. Cliente e servidor rodam o mesmo código com a
// mesma seed, então o mapa nunca precisa trafegar pela rede.

export const MAP_W = 512;
export const MAP_H = 512;

export enum Tile {
  Deep,
  Water,
  Shallow,
  Sand,
  Grass,
  Forest,
  Mountain,
  Dock,
}

export const isWater = (t: Tile) => t === Tile.Deep || t === Tile.Water || t === Tile.Shallow;
export const isSailable = (t: Tile) => isWater(t) || t === Tile.Dock;

// Centro do mapa: ilha-porto inicial, onde ficam os NPCs.
export const PORT_X = MAP_W >> 1;
export const PORT_Y = MAP_H >> 1;
export const PORT_RADIUS = 7;
export const DOCK_X = PORT_X;
export const DOCK_Y = PORT_Y + PORT_RADIUS;

export function hash2(x: number, y: number, seed: number): number {
  let h = (x * 374761393 + y * 668265263 + seed * 2147483647) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}

const smooth = (t: number) => t * t * (3 - 2 * t);

function valueNoise(x: number, y: number, seed: number): number {
  const ix = Math.floor(x), iy = Math.floor(y);
  const fx = smooth(x - ix), fy = smooth(y - iy);
  const a = hash2(ix, iy, seed), b = hash2(ix + 1, iy, seed);
  const c = hash2(ix, iy + 1, seed), d = hash2(ix + 1, iy + 1, seed);
  return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

function fbm(x: number, y: number, seed: number): number {
  let sum = 0, amp = 0.5, freq = 1, norm = 0;
  for (let o = 0; o < 5; o++) {
    sum += valueNoise(x * freq, y * freq, seed + o * 101) * amp;
    norm += amp;
    amp *= 0.5;
    freq *= 2;
  }
  return sum / norm;
}

export class GameMap {
  readonly tiles: Uint8Array;

  constructor(readonly seed: number) {
    this.tiles = new Uint8Array(MAP_W * MAP_H);
    for (let y = 0; y < MAP_H; y++) {
      for (let x = 0; x < MAP_W; x++) {
        this.tiles[y * MAP_W + x] = this.classify(x, y);
      }
    }
  }

  private classify(x: number, y: number): Tile {
    const pd = Math.hypot(x - PORT_X, y - PORT_Y);
    if (x === DOCK_X && y === DOCK_Y) return Tile.Dock;
    if (pd <= PORT_RADIUS) return pd > PORT_RADIUS - 1.5 ? Tile.Sand : pd < 2 ? Tile.Forest : Tile.Grass;
    // Água calma ao redor do porto, para quem acabou de entrar.
    if (pd <= PORT_RADIUS + 14) return pd <= PORT_RADIUS + 3 ? Tile.Shallow : Tile.Water;

    // Quanto mais longe do centro, mais fundo e perigoso fica o mar.
    const edge = Math.hypot(x - PORT_X, y - PORT_Y) / (MAP_W / 2);
    const e = fbm(x / 48, y / 48, this.seed) - edge * 0.12;
    if (e < 0.4) return Tile.Deep;
    if (e < 0.55) return Tile.Water;
    if (e < 0.6) return Tile.Shallow;
    if (e < 0.62) return Tile.Sand;
    if (e < 0.67) return Tile.Grass;
    if (e < 0.72) return Tile.Forest;
    return Tile.Mountain;
  }

  get(x: number, y: number): Tile {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return Tile.Mountain;
    return this.tiles[y * MAP_W + x] as Tile;
  }
}
