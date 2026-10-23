import { GameMap, MAP_W, MAP_H, isWater, isSailable, DOCK_X, DOCK_Y, PORT_X, PORT_Y, PORT_RADIUS } from '../shared/map.ts';
import { SPECIES, rollSpecies, holdValue } from '../shared/fish.ts';
import { VIEW_RADIUS, type SchoolView, type ServerMsg, type PlayerView, type Objective } from '../shared/protocol.ts';

const MAX_SCHOOLS = 70;
const PASSIVE_CATCH_CHANCE = 0.06; // por tick em movimento
const SCHOOL_CATCH_CHANCE = 0.35; // por tick dentro do cardume
const BASE_CAPACITY = 150;
const BASE_SPEED = 5; // células por segundo

export interface Player {
  id: string;
  name: string;
  x: number;
  y: number;
  color: number;
  dx: number;
  dy: number;
  moveBudget: number;
  money: number;
  hold: Record<string, number>;
  capacity: number;
  speed: number;
  log: string[];
  send: (msg: ServerMsg) => void;
}

interface School extends SchoolView {
  expiresAt: number;
}

export class Game {
  readonly map: GameMap;
  readonly players = new Map<string, Player>();
  private schools: School[] = [];
  private nextSchoolId = 1;
  tick = 0;

  constructor(readonly seed: number) {
    this.map = new GameMap(seed);
  }

  addPlayer(id: string, name: string, send: Player['send']): Player {
    const spawn = this.findSpawn();
    const p: Player = {
      id,
      name: name.slice(0, 16) || 'Marujo',
      ...spawn,
      color: Math.floor(Math.random() * 6),
      dx: 0,
      dy: 0,
      moveBudget: 0,
      money: 0,
      hold: {},
      capacity: BASE_CAPACITY,
      speed: BASE_SPEED,
      log: ['Bem-vindo ao mar! Navegue para pescar. Volte ao cais (H) e aperte E para vender.'],
      send,
    };
    this.players.set(id, p);
    return p;
  }

  removePlayer(id: string) {
    this.players.delete(id);
  }

  setInput(p: Player, dx: number, dy: number) {
    p.dx = Math.sign(dx) | 0;
    p.dy = Math.sign(dy) | 0;
  }

  // Lojista provisório no cais: compra todo o porão.
  sell(p: Player) {
    if (!this.atDock(p)) return;
    const value = holdValue(p.hold);
    if (value === 0) return p.log.push('O lojista: "Seu porão está vazio, marujo."');
    p.money += value;
    p.hold = {};
    p.log.push(`Vendeu o porão por $${value}.`);
  }

  step() {
    this.tick++;
    this.updateSchools();
    for (const p of this.players.values()) this.updatePlayer(p);
    this.broadcast();
  }

  private atDock(p: Player) {
    return Math.abs(p.x - DOCK_X) <= 1 && Math.abs(p.y - DOCK_Y) <= 1;
  }

  private findSpawn() {
    for (let i = 0; i < 200; i++) {
      const a = Math.random() * Math.PI * 2;
      const r = PORT_RADIUS + 2 + Math.random() * 6;
      const x = Math.round(PORT_X + Math.cos(a) * r);
      const y = Math.round(PORT_Y + Math.sin(a) * r);
      if (isWater(this.map.get(x, y))) return { x, y };
    }
    return { x: DOCK_X, y: DOCK_Y + 1 };
  }

  private holdCount(p: Player) {
    return Object.values(p.hold).reduce((s, n) => s + n, 0);
  }

  private addFish(p: Player, species: string, n: number) {
    const room = p.capacity - this.holdCount(p);
    if (room <= 0) {
      if (this.tick % 50 === 0) p.log.push('Porão cheio! Volte ao porto para vender.');
      return;
    }
    const got = Math.min(n, room);
    p.hold[species] = (p.hold[species] ?? 0) + got;
    p.log.push(`+${got} ${SPECIES[species].name}`);
  }

  private updatePlayer(p: Player) {
    let moved = false;
    if (p.dx || p.dy) {
      // Velocidade em células/s convertida para "orçamento" de movimento por tick.
      p.moveBudget += p.speed / 10;
      while (p.moveBudget >= 1) {
        p.moveBudget -= 1;
        const nx = p.x + p.dx, ny = p.y + p.dy;
        if (isSailable(this.map.get(nx, ny))) {
          p.x = nx;
          p.y = ny;
          moved = true;
        } else if (p.dx && p.dy && isSailable(this.map.get(nx, p.y))) {
          p.x = nx; // desliza pela costa
          moved = true;
        } else if (p.dx && p.dy && isSailable(this.map.get(p.x, ny))) {
          p.y = ny;
          moved = true;
        }
      }
    } else {
      p.moveBudget = 0;
    }

    const tile = this.map.get(p.x, p.y);
    if (moved && Math.random() < PASSIVE_CATCH_CHANCE) {
      const sp = rollSpecies(tile, Math.random());
      if (sp) this.addFish(p, sp, 1);
    }

    for (const s of this.schools) {
      if (Math.hypot(p.x - s.x, p.y - s.y) <= s.r && Math.random() < SCHOOL_CATCH_CHANCE) {
        this.addFish(p, s.species, 1 + Math.floor(Math.random() * 3));
      }
    }
    if (p.log.length > 8) p.log.splice(0, p.log.length - 8);
  }

  private updateSchools() {
    this.schools = this.schools.filter((s) => s.expiresAt > this.tick);
    while (this.schools.length < MAX_SCHOOLS) {
      const x = Math.floor(Math.random() * MAP_W), y = Math.floor(Math.random() * MAP_H);
      const tile = this.map.get(x, y);
      // Nada de cardume colado no porto: precisa sair para buscar.
      if (!isWater(tile) || Math.hypot(x - PORT_X, y - PORT_Y) < 30) continue;
      const species = rollSpecies(tile, Math.random())!;
      this.schools.push({
        id: this.nextSchoolId++,
        x,
        y,
        r: 2 + Math.floor(Math.random() * 4),
        species,
        expiresAt: this.tick + 600 + Math.floor(Math.random() * 1200), // 1 a 3 minutos
      });
    }
    // Cardumes migram devagar.
    if (this.tick % 20 === 0) {
      for (const s of this.schools) {
        const nx = s.x + Math.floor(Math.random() * 3) - 1, ny = s.y + Math.floor(Math.random() * 3) - 1;
        if (isWater(this.map.get(nx, ny))) {
          s.x = nx;
          s.y = ny;
        }
      }
    }
  }

  private broadcast() {
    const all = [...this.players.values()];
    const rank = all
      .map((p) => ({ name: p.name, money: p.money }))
      .sort((a, b) => b.money - a.money)
      .slice(0, 10);
    const objectives: Objective[] | undefined =
      this.tick % 10 === 0 ? this.schools.map(({ x, y, species }) => ({ kind: 'school', x, y, species })) : undefined;
    const near = (x: number, y: number, ox: number, oy: number, extra = 0) =>
      Math.abs(x - ox) <= VIEW_RADIUS + extra && Math.abs(y - oy) <= VIEW_RADIUS + extra;

    for (const p of all) {
      const players: PlayerView[] = all
        .filter((o) => o !== p && near(o.x, o.y, p.x, p.y))
        .map(({ id, name, x, y, color }) => ({ id, name, x, y, color }));
      const schools = this.schools
        .filter((s) => near(s.x, s.y, p.x, p.y, s.r))
        .map(({ id, x, y, r, species }) => ({ id, x, y, r, species }));
      p.send({
        t: 'state',
        tick: this.tick,
        you: {
          id: p.id,
          name: p.name,
          x: p.x,
          y: p.y,
          color: p.color,
          money: p.money,
          hold: p.hold,
          holdCount: this.holdCount(p),
          capacity: p.capacity,
          speed: p.speed,
          atDock: this.atDock(p),
        },
        players,
        schools,
        rank,
        online: all.length,
        log: p.log,
        objectives,
      });
    }
  }
}
